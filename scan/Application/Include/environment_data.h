/*
 * 文件: environment_data.h
 * 功能: 环境数据结构定义与采集接口。
 */
#ifndef ENVIRONMENT_DATA_H
#define ENVIRONMENT_DATA_H

#include <stdint.h>

/* 一次采集到的环境数据快照 */
typedef struct
{
    float temperature_c;      /* 温度(摄氏度)            */
    float humidity_percent;   /* 湿度(%RH)               */
    uint16_t illuminance_lux; /* 光照强度(lx)            */
    float smoke_ppm;          /* 烟雾浓度(ppm, 粗略值)    */
    uint8_t alarm_active;     /* 报警标志: 1=温度或烟雾超限 */
} EnvironmentData;

/* 读取一次完整环境数据, 返回 1=成功 */
uint8_t EnvironmentData_Read(EnvironmentData *data);

#endif
