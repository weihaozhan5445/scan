/*
 * ============================================================================
 * 文件: environment_tasks.c
 * 功能: FreeRTOS 任务层 —— 采集/显示/报警/上传/OTA 五个任务及消息队列。
 *
 * 任务与优先级(数值越大优先级越高):
 *   Collect(3): 每 500ms 采集一次环境数据, 分发给各队列, 并喂看门狗;
 *   Display(2): 收到数据即刷新 OLED;
 *   Alarm  (2): 收到数据按报警标志驱动蜂鸣器;
 *   Upload (1): 连接 WiFi/MQTT, 每 30s 上报一次环境数据;
 *   OTA    (1): 轮询云端下行指令 + 驱动升级状态机。
 *
 * 并发安全:
 *   ESP8266 串口是共享资源, Upload 与 OTA 两个任务通过 xEspMutex 互斥,
 *   避免 AT 指令与响应互相串扰。
 * ============================================================================
 */
#include "app_tasks.h"
#include "display_service.h"
#include "environment_data.h"
#include "app_hooks.h"
#include "iap.h"
#include "Buzzer.h"
#include "ESP8266.h"
#include "ota_service.h"
#include "ota_task.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "main.h"
#include <stdio.h>

/* ---- 周期参数 ---- */
#define SAMPLE_PERIOD_MS 500U           /* 采集周期 500ms                  */
#define UPLOAD_PERIOD_MS 30000U         /* 云端上报周期 30s                */
#define RECONNECT_PERIOD_MS 2000U       /* WiFi 连接失败后重试间隔 2s      */

/* ---- 队列与互斥锁 ---- */
static QueueHandle_t display_queue;     /* 采集 → 显示 队列               */
static QueueHandle_t alarm_queue;       /* 采集 → 报警 队列               */
static QueueHandle_t upload_queue;      /* 采集 → 上传 队列               */
static SemaphoreHandle_t xEspMutex;     /* ESP8266 访问互斥锁             */

/*
 * 采集任务: 每 500ms 读一次传感器, 同时把数据"覆盖"进三个队列
 * (xQueueOverwrite 保证队列里始终是最新数据, 不会积压)。
 * 第一次成功采集时调用 App_Boot_Confirm() 完成升级确认。
 */
static void collect_task(void *argument)
{
    EnvironmentData data;
    uint8_t confirmed = 0U;

    (void)argument;
    for (;;)
    {
        if (EnvironmentData_Read(&data) != 0U)
        {
            if (confirmed == 0U)
            {
                /* 第一次成功采集 => 通知 Bootloader 本 App 运行正常(升级确认) */
                App_Boot_Confirm();
                confirmed = 1U;
            }
            xQueueOverwrite(display_queue, &data);
            xQueueOverwrite(alarm_queue, &data);
            xQueueOverwrite(upload_queue, &data);
        }
        App_Watchdog_Feed();            /* 采集任务周期喂狗(双保险)       */
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

/* 显示任务: 阻塞等队列数据, 收到就刷新 OLED */
static void display_task(void *argument)
{
    EnvironmentData data;

    (void)argument;
    for (;;)
    {
        if (xQueueReceive(display_queue, &data, portMAX_DELAY) == pdPASS)
            DisplayService_Render(&data);
    }
}

/* 报警任务: 根据 alarm_active 开关蜂鸣器 */
static void alarm_task(void *argument)
{
    EnvironmentData data;

    (void)argument;
    for (;;)
    {
        if (xQueueReceive(alarm_queue, &data, portMAX_DELAY) == pdPASS)
        {
            if (data.alarm_active != 0U) BUZZER_ON();
            else BUZZER_OFF();
        }
    }
}

/*
 * 上传任务: 先连 WiFi/MQTT(失败每 2s 重试), 成功后每 30s 上报一次。
 * 所有 ESP 操作都在互斥锁保护下进行, 避免与 OTA 任务冲突。
 */
static void upload_task(void *argument)
{
    EnvironmentData data;
    char json[160];
    uint8_t connected = 0U;

    (void)argument;
    vTaskDelay(pdMS_TO_TICKS(800U));          /* 等采集任务先跑起来, 有数据可发 */
    for (;;)
    {
        if (xSemaphoreTake(xEspMutex, pdMS_TO_TICKS(5000U)) == pdTRUE)
        {
            if (connected == 0U)
            {
                /* 首次: 建立 WiFi + MQTT 连接, 成功后上报固件版本 */
                if (ESP_MQTT_Init() != 0U)
                {
                    connected = 1U;
                    MQTT_UploadFwVersion();
                }
            }
            else if (xQueueReceive(upload_queue, &data, 0U) == pdPASS)
            {
                /* 打包最新环境数据并发布(整数上报, 避免浮点打印开销) */
                (void)snprintf(json, sizeof(json),
                               "{\"temperature\":%d,\"humidity\":%d,\"light\":%u,\"smoke\":%d,\"alarm\":%u}",
                               (int)data.temperature_c, (int)data.humidity_percent,
                               data.illuminance_lux, (int)data.smoke_ppm, data.alarm_active);
                ESP_MQTT_Publish(MQTT_UP_TOPIC, json);
            }
            xSemaphoreGive(xEspMutex);
        }
        /* 已连接 30s 上报一次; 未连接 2s 重试一次 */
        vTaskDelay(pdMS_TO_TICKS(connected ? UPLOAD_PERIOD_MS : RECONNECT_PERIOD_MS));
    }
}

/*
 * OTA 任务: 每 100ms 轮询一次。
 *   1. 读云端下行 MQTT 消息 → 解析(可能触发升级状态机);
 *   2. 驱动 App_update_work() 状态机(下载阶段会长时间持锁, 期间上传暂停);
 * 持锁原因: 下载使用同一个 ESP 串口, 必须与上传任务互斥。
 */
static void ota_task(void *argument)
{
    char message[256];

    (void)argument;
    for (;;)
    {
        if (xSemaphoreTake(xEspMutex, pdMS_TO_TICKS(2000U)) == pdTRUE)
        {
            if (ESP_MQTT_GetRecvMsg(message, sizeof(message)) != 0U)
                MQTT_ParseDownMsg(message);
            App_update_work();
            xSemaphoreGive(xEspMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100U));
    }
}

/*
 * 任务/队列/互斥锁统一创建入口(main 调用)。
 * 任一创建失败即进入 Error_Handler。
 */
void AppTasks_Create(void)
{
    display_queue = xQueueCreate(1U, sizeof(EnvironmentData));
    alarm_queue = xQueueCreate(1U, sizeof(EnvironmentData));
    upload_queue = xQueueCreate(1U, sizeof(EnvironmentData));
    xEspMutex = xSemaphoreCreateMutex();

    if (display_queue == NULL || alarm_queue == NULL || upload_queue == NULL ||
        xEspMutex == NULL)
    {
        Error_Handler();
    }

    (void)xTaskCreate(collect_task, "Collect", 256U, NULL, 3U, NULL);
    (void)xTaskCreate(display_task, "Display", 256U, NULL, 2U, NULL);
    (void)xTaskCreate(alarm_task, "Alarm", 128U, NULL, 2U, NULL);
    (void)xTaskCreate(upload_task, "Upload", 384U, NULL, 1U, NULL);
    (void)xTaskCreate(ota_task, "OTA", 384U, NULL, 1U, NULL);
}
