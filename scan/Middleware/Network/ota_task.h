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
// MQTT下发OTA固件参数结构体
typedef struct
{
    uint8_t ota_enable;
    char fw_url[128];
    uint32_t fw_len;
    uint32_t fw_crc;
    uint8_t new_major;  // 升级后主版本
    uint8_t new_minor;  // 升级后次版本

} MqttOtaDef;

// MQTT下发报警阈值结构体
typedef struct
{
    uint16_t temp_max;
    uint16_t smoke_max;
} MqttThrDef;

extern MqttOtaDef g_mqtt_ota_param;
extern MqttThrDef g_mqtt_thr_param;
// 全局传感器数据
extern uint16_t g_temp, g_humi, g_smoke;

// 上传温湿度、烟雾、报警状态JSON到阿里云
void MQTT_UploadEnvData(uint8_t alarm_flag);
// 定时上报当前固件版本给云端对比
void MQTT_UploadFwVersion(void);
// 解析云端下发JSON指令（远程OTA/远程修改阈值）
void MQTT_ParseDownMsg(char *msg_buf);

#endif

