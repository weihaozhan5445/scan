/*
 * ============================================================================
 * 文件: Dht11.h
 * 功能: DHT11 温湿度传感器驱动接口(单总线, PB15)。
 * 数据格式: 40bit = 湿度整/小 + 温度整/小 + 校验和。
 * 注意: 读取函数内部使用临界区与微秒延时(delay_us), 见 Dht11.c。
 * ============================================================================
 */
#ifndef __INF_DHT11__
#define __INF_DHT11__

#include "gpio.h"
#include "com_debug.h"
#include "FreeRTOS.h"
#include "task.h"
#include "delay.h"

/* 数据线操作宏: 拉高/拉低/读取(引脚定义见 Core/Inc/main.h) */
#define DHT11_DATA_H HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,DHT11_DATA_Pin,GPIO_PIN_SET)
#define DHT11_DATA_L HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,DHT11_DATA_Pin,GPIO_PIN_RESET)
#define DHT11_DATA_Read HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port,DHT11_DATA_Pin)

void dht11_init(void);                                   /* 上电稳定等待 */
void dht11_get_date(float *temperature, float *humidity); /* 读取温湿度 */

#endif
