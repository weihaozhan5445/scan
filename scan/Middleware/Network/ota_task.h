/*
 * ============================================================================
 * 文件: ota_task.h
 * 功能: OTA/云端通信的公共类型与全局变量声明。
 * 说明: 本头文件被 OTA 状态机(firmware_update_service.c)、MQTT 解析
 *       (ota_command_service.c)和任务层(environment_tasks.c)共同引用。
 * ============================================================================
 */
#ifndef __OTA_TASK_H
#define __OTA_TASK_H
#include "stm32f1xx_hal.h"
#include "app_update.h"
#include "ESP8266.h"
#include "Int_w24c02.h"
#include "Dht11.h"
#include "Buzzer.h"
#include "MQ_2.h"
#include <string.h>
#include <stdio.h>

/* MQTT 下发的 OTA 固件参数结构体 */
typedef struct
{
    uint8_t ota_enable;         /* 1=本次消息包含 OTA 指令              */
    char fw_url[128];           /* 固件下载地址(HTTP URL)               */
    uint32_t fw_len;            /* 固件 payload 字节数                  */
    uint32_t fw_crc;            /* 固件 payload 的 CRC-32                */
    uint8_t new_major;          /* 升级后主版本号                       */
    uint8_t new_minor;          /* 升级后次版本号                       */
} MqttOtaDef;

/* MQTT 下发的报警阈值结构体 */
typedef struct
{
    uint16_t temp_max;          /* 温度报警上限(摄氏度, 0=禁用)         */
    uint16_t smoke_max;         /* 烟雾报警上限(ppm, 0=禁用)            */
} MqttThrDef;

/* 全局变量: 云端参数 + 最近传感器整型值 */
extern MqttOtaDef g_mqtt_ota_param;   /* OTA 参数(解析结果)             */
extern MqttThrDef g_mqtt_thr_param;   /* 报警阈值(可被云端修改)          */
extern uint16_t g_temp;               /* 最近温度(整型, 供上报)          */
extern uint16_t g_humi;               /* 最近湿度(整型, 供上报)          */
extern uint16_t g_smoke;              /* 最近烟雾浓度(整型, 供上报)       */

/* 上报温湿度/烟雾/报警状态 JSON 到阿里云 */
void MQTT_UploadEnvData(uint8_t alarm_flag);
/* 上报当前固件版本给云端对比 */
void MQTT_UploadFwVersion(void);
/* 解析云端下发 JSON 指令(远程 OTA / 远程修改阈值) */
void MQTT_ParseDownMsg(char *msg_buf);

#endif
