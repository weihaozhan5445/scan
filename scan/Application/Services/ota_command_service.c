/*
 * ============================================================================
 * 文件: ota_command_service.c
 * 功能: 解析云端 MQTT 下发的 JSON 指令(远程 OTA / 远程阈值)并触发相应动作。
 *
 * 下行指令示例:
 *   OTA   : {"ota_enable":1,"fw_url":"http://.../scan_ota.bin",
 *            "fw_len":34008,"fw_crc":894877103,"new_major":1,"new_minor":1}
 *   阈值  : {"temp_max":40,"smoke_max":80}
 *
 * 解析结果写入 g_mqtt_ota_param / g_mqtt_thr_param 全局结构,
 * OTA 指令还会把 update_state 置为 UPDATE_RECV_SEND_CMD 触发升级流程。
 * ============================================================================
 */
#include "ota_task.h"
#include "ota_image.h"
#include "app_update.h"
#include "ESP8266.h"
#include <string.h>

/* 兼容别名: 旧代码通过 W24C02_WriteByte 写 EEPROM */
void W24C02_WriteByte(uint8_t byte_addr, uint8_t data)
{
    Int_w24c02_write_byte(byte_addr, data);
}

/* ---- 云端参数全局变量 ---- */
MqttOtaDef g_mqtt_ota_param;                        /* OTA 参数                 */
MqttThrDef g_mqtt_thr_param = {35U, 65U};           /* 默认阈值: 35C / 65ppm    */
uint16_t g_temp;                                    /* 最近一次温度(整型)        */
uint16_t g_humi;                                    /* 最近一次湿度(整型)        */
uint16_t g_smoke;                                   /* 最近一次烟雾浓度(整型)    */

/*
 * 上报环境数据 JSON 到云端(温度/湿度/烟雾/报警标志)。
 * 说明: 当前上传任务直接拼 JSON, 此函数保留用于兼容/其它调用点。
 */
void MQTT_UploadEnvData(uint8_t alarm_flag)
{
    char json[128];

    (void)snprintf(json, sizeof(json),
                   "{\"temperature\":%u,\"humidity\":%u,\"smoke\":%u,\"alarm_flag\":%u}",
                   g_temp, g_humi, g_smoke, alarm_flag);
    ESP_MQTT_Publish(MQTT_UP_TOPIC, json);
}

/* 上报当前固件版本, 供云端做版本对比 */
void MQTT_UploadFwVersion(void)
{
    char json[128];

    (void)snprintf(json, sizeof(json),
                   "{\"fw_version\":\"%u.%u\"}", FW_VER_MAJOR, FW_VER_MINOR);
    ESP_MQTT_Publish(MQTT_UP_TOPIC, json);
}

/*
 * 解析云端下发指令。
 * 参数 message: 从 ESP 收到的 JSON 字符串(以 '{' 开头)。
 * 说明: 阈值指令立即生效; OTA 指令需要 ota_enable==1 且带有效 fw_url 才触发。
 */
void MQTT_ParseDownMsg(char *message)
{
    if ((message == NULL) || (message[0] == '\0'))
    {
        return;
    }

    /* ---- 1. 远程修改报警阈值(带范围校验, 防止非法值) ---- */
    if (strstr(message, "temp_max") != NULL)
    {
        uint32_t v = GetJsonNum32(message, "temp_max");
        if ((v > 0U) && (v <= 100U))
        {
            g_mqtt_thr_param.temp_max = (uint16_t)v;
        }
    }
    if (strstr(message, "smoke_max") != NULL)
    {
        uint32_t v = GetJsonNum32(message, "smoke_max");
        if ((v > 0U) && (v <= 1000U))
        {
            g_mqtt_thr_param.smoke_max = (uint16_t)v;
        }
    }

    /* ---- 2. OTA 指令: 必须 ota_enable==1 ---- */
    if ((strstr(message, "ota_enable") == NULL) ||
        (GetJsonNum32(message, "ota_enable") != 1U))
    {
        return;
    }

    {
        char url[sizeof(g_mqtt_ota_param.fw_url)] = {0};
        uint32_t len;
        uint32_t crc;

        /* 必须有下载地址, 否则忽略 */
        GetJsonStr(message, "fw_url", url, sizeof(url));
        if (url[0] == '\0')
        {
            return;
        }
        /* 长度必须在合法范围内(256B ~ 48KB), 防止越界 */
        len = GetJsonNum32(message, "fw_len");
        crc = GetJsonNum32(message, "fw_crc");
        if ((len < 0x100U) || (len > APP_MAX_SIZE))
        {
            return;
        }

        /* 保存 OTA 参数并触发升级状态机 */
        g_mqtt_ota_param.ota_enable = 1U;
        (void)strncpy(g_mqtt_ota_param.fw_url, url, sizeof(g_mqtt_ota_param.fw_url) - 1U);
        g_mqtt_ota_param.fw_url[sizeof(g_mqtt_ota_param.fw_url) - 1U] = '\0';
        g_mqtt_ota_param.fw_len = len;
        g_mqtt_ota_param.fw_crc = crc;
        g_mqtt_ota_param.new_major = (uint8_t)GetJsonNum32(message, "new_major");
        g_mqtt_ota_param.new_minor = (uint8_t)GetJsonNum32(message, "new_minor");

        update_state = UPDATE_RECV_SEND_CMD;        /* 进入升级流程 */
    }
}
