#include "ota_task.h"

MqttOtaDef g_mqtt_ota_param = {0};
MqttThrDef g_mqtt_thr_param = {0};
uint16_t g_temp = 0, g_humi = 0, g_smoke = 0;

// 上传环境监测数据与报警标志
void MQTT_UploadEnvData(uint8_t alarm_flag)
{
    char json_buf[256] = {0};
    sprintf(json_buf,
            "{\"temperature\":%d,\"humidity\":%d,\"smoke\":%d,\"alarm_flag\":%d}",
            g_temp, g_humi, g_smoke, alarm_flag);
    ESP_MQTT_Publish(MQTT_UP_TOPIC, json_buf);
}

// 读取W24C02固件版本，上报云端
void MQTT_UploadFwVersion(void)
{
    char ver_json[128] = {0};
    uint8_t major = Int_w24c02_read_byte(ADDR_FW_VER_MAJOR);
    uint8_t minor = Int_w24c02_read_byte(ADDR_FW_VER_MINOR);
    sprintf(ver_json, "{\"firmware_version\":\"%d.%d\"}", major, minor);
    ESP_MQTT_Publish(MQTT_UP_TOPIC, ver_json);
}

// 解析云端下发JSON指令
void MQTT_ParseDownMsg(char *msg_buf)
{
    // 分支1：远程下发OTA升级指令
    if (strstr(msg_buf, "u"))
    {
        g_mqtt_ota_param.ota_enable = 1;
        // 提取固件HTTP下载地址
        char *p_url = strstr(msg_buf, "url\":\"");
        if (p_url != NULL)
        {
            p_url += 6;
            sscanf(p_url, "%[^\"]", g_mqtt_ota_param.fw_url);
        }
        // 提取固件总字节长度
        char *p_len = strstr(msg_buf, "fw_len\":");
        if (p_len != NULL)
        {
            p_len += 7;
            scanf(p_len, "%lu", &g_mqtt_ota_param.fw_len);
        }
        // 提取固件CRC16校验值
        char *p_crc = strstr(msg_buf, "fw_crc\":");
        if (p_crc != NULL)
        {
            p_crc += 7;
            sscanf(p_crc, "%hu", &g_mqtt_ota_param.fw_crc);
        }
        // 切换OTA状态，启动下载流程
        update_state = UPDATE_RECV_SEND_CMD;
    }
    // 分支2：远程下发报警阈值修改指令
    else if (strstr(msg_buf, "set_alarm_thr"))
    {
        char *p_temp = strstr(msg_buf, "temp_max\":");
        if (p_temp != NULL)
        {
            p_temp += 10;
            sscanf(p_temp, "%hu", &g_mqtt_thr_param.temp_max);
        }
        char *p_smoke = strstr(msg_buf, "smoke_max\":");
        if (p_smoke != NULL)
        {
            p_smoke += 11;
            sscanf(p_smoke, "%hu", &g_mqtt_thr_param.smoke_max);
        }
        // 写入W24C02永久保存阈值，掉电不丢失
        W24C02_WriteByte(ADDR_TEMP_THR_H, (g_mqtt_thr_param.temp_max >> 8) & 0xFF);
        W24C02_WriteByte(ADDR_TEMP_THR_L, g_mqtt_thr_param.temp_max & 0xFF);
        W24C02_WriteByte(ADDR_SMOKE_THR_H, (g_mqtt_thr_param.smoke_max >> 8) & 0xFF);
        W24C02_WriteByte(ADDR_SMOKE_THR_L, g_mqtt_thr_param.smoke_max & 0xFF);
    }
}

// 周期采集传感器，阈值对比，本地蜂鸣报警+MQTT上传状态
void Sensor_CollectAndAlarm(void)
{
    uint16_t temp_thr, smoke_thr;
    uint8_t alarm_flag = 0;

    // 读取温湿度
    DHT11_ReadData(&g_temp, &g_humi);
    // 读取烟雾浓度，带入温度补偿计算
    g_smoke = MQ2_GetValue(g_temp);

    // 阈值优先级：云端下发阈值 > W24C02本地默认阈值
    if (g_mqtt_thr_param.temp_max > 0)
    {
        temp_thr = g_mqtt_thr_param.temp_max;
        smoke_thr = g_mqtt_thr_param.smoke_max;
    }
    else
    {
        temp_thr = (W24C02_ReadByte(ADDR_TEMP_THR_H) << 8) | W24C02_ReadByte(ADDR_TEMP_THR_L);
        smoke_thr = (W24C02_ReadByte(ADDR_SMOKE_THR_H) << 8) | W24C02_ReadByte(ADDR_SMOKE_THR_L);
    }

    // 判断是否超标报警
    if (g_temp >= temp_thr || g_smoke >= smoke_thr)
    {
        alarm_flag = 1;
        Buzzer_On();
    }
    else
    {
        alarm_flag = 0;
        Buzzer_Off();
    }
    // MQTT上传当前环境与报警状态
    MQTT_UploadEnvData(alarm_flag);
}
