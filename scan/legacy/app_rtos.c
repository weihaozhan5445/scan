#include "APP_rtos.h"
#include "usart.h"

#define SAMPLE_PERIOD_MS 500U
#define UPLOAD_PERIOD_MS 30000U

static QueueHandle_t display_queue;
static QueueHandle_t alarm_queue;
static QueueHandle_t upload_queue;

TaskHandle_t xGetDataTaskHandle = NULL;
TaskHandle_t xShowTaskHandle = NULL;
TaskHandle_t xAlarmTaskHandle = NULL;
TaskHandle_t xUploadTaskHandle = NULL;
TaskHandle_t xOtaTaskHandle = NULL;

static void vGetDataTask(void *parameters)
{
    APP_ALL_DATA_type data;
    (void)parameters;
    for (;;)
    {
        if (app_get_data_all(&data) != 0U)
        {
            xQueueOverwrite(display_queue, &data);
            xQueueOverwrite(alarm_queue, &data);
            xQueueOverwrite(upload_queue, &data);
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

static void vShowTask(void *parameters)
{
    APP_ALL_DATA_type data;
    (void)parameters;
    for (;;)
    {
        if (xQueueReceive(display_queue, &data, portMAX_DELAY) == pdPASS)
        {
            OLED_RefreshAll(&data);
        }
    }
}

static void vAlarmTask(void *parameters)
{
    APP_ALL_DATA_type data;
    (void)parameters;
    for (;;)
    {
        if (xQueueReceive(alarm_queue, &data, portMAX_DELAY) == pdPASS)
        {
            if (data.alarm_flag != 0U) BUZZER_ON();
            else BUZZER_OFF();
        }
    }
}

static void vUploadTask(void *parameters)
{
    APP_ALL_DATA_type data;
    char json[160];
    (void)parameters;

    for (;;)
    {
        if (ESP_MQTT_Init() == 0U)
        {
            vTaskDelay(pdMS_TO_TICKS(2000U));
            continue;
        }
        break;
    }

    for (;;)
    {
        if (xQueueReceive(upload_queue, &data, pdMS_TO_TICKS(UPLOAD_PERIOD_MS)) == pdPASS)
        {
            (void)snprintf(json, sizeof(json),
                           "{\"temperature\":%.1f,\"humidity\":%.1f,\"light\":%u,\"smoke\":%.1f,\"alarm\":%u}",
                           data.Temp, data.Hum, data.light, data.Smoke, data.alarm_flag);
            ESP_MQTT_Publish(MQTT_UP_TOPIC, json);
        }
    }
}

void vOtaTask(void *parameters)
{
    char message[256];
    (void)parameters;
    for (;;)
    {
        if (ESP_MQTT_GetRecvMsg(message, sizeof(message)) != 0U)
        {
            MQTT_ParseDownMsg(message);
        }
        App_update_work();
        vTaskDelay(pdMS_TO_TICKS(100U));
    }
}

void App_CreateAllTask(void)
{
    display_queue = xQueueCreate(1U, sizeof(APP_ALL_DATA_type));
    alarm_queue = xQueueCreate(1U, sizeof(APP_ALL_DATA_type));
    upload_queue = xQueueCreate(1U, sizeof(APP_ALL_DATA_type));
    if (display_queue == NULL || alarm_queue == NULL || upload_queue == NULL)
    {
        Error_Handler();
    }

    (void)xTaskCreate(vGetDataTask, "GetData", 512U, NULL, 3U, &xGetDataTaskHandle);
    (void)xTaskCreate(vShowTask, "Show", 512U, NULL, 2U, &xShowTaskHandle);
    (void)xTaskCreate(vAlarmTask, "Alarm", 256U, NULL, 2U, &xAlarmTaskHandle);
    (void)xTaskCreate(vUploadTask, "Upload", 512U, NULL, 1U, &xUploadTaskHandle);
    (void)xTaskCreate(vOtaTask, "OTA", 512U, NULL, 1U, &xOtaTaskHandle);
}
