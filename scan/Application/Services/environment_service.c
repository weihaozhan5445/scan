/*
 * ============================================================================
 * 文件: environment_service.c
 * 功能: 环境数据采集服务 —— 汇总各传感器读数并做报警判断。
 * 传感器:
 *   DHT11   : 温度、湿度(I2C/GPIO 单总线)
 *   BH1750  : 光照强度(I2C)
 *   MQ-2    : 烟雾浓度(ADC1 + DMA 采样)
 * 报警阈值默认 35°C / 65ppm, 可被云端 MQTT 指令远程修改(g_mqtt_thr_param)。
 * ============================================================================
 */
#include "environment_data.h"
#include "ota_task.h"               /* g_mqtt_thr_param 阈值结构 */
#include "BH1750.h"
#include "Dht11.h"
#include "MQ_2.h"

/* 云端未下发阈值前的默认报警阈值(实际使用 g_mqtt_thr_param 的初始化值) */
#define TEMPERATURE_ALARM_DEFAULT_C 35.0f
#define SMOKE_ALARM_DEFAULT_PPM     65.0f

/*
 * 读取一次完整环境数据。
 * 返回 1=成功; 数据异常时仍返回 1(由上层决定如何使用)。
 * 报警标志 = 温度超限 或 烟雾超限。
 */
uint8_t EnvironmentData_Read(EnvironmentData *data)
{
    uint16_t temp_max;
    uint16_t smoke_max;

    if (data == NULL) return 0U;

    /* 1. 采集三个传感器 */
    dht11_get_date(&data->temperature_c, &data->humidity_percent);
    data->illuminance_lux = BH1750_Readlight();
    data->smoke_ppm = MQ2_GetPPM();

    /* 2. 阈值可被云端下发修改(默认 35/65), 0 表示禁用该项报警 */
    temp_max = g_mqtt_thr_param.temp_max;
    smoke_max = g_mqtt_thr_param.smoke_max;
    data->alarm_active = ((temp_max != 0U) && (data->temperature_c > (float)temp_max)) ||
                         ((smoke_max != 0U) && (data->smoke_ppm > (float)smoke_max)) ? 1U : 0U;

    (void)TEMPERATURE_ALARM_DEFAULT_C;   /* 保留常量仅作文档说明 */
    (void)SMOKE_ALARM_DEFAULT_PPM;
    return 1U;
}
