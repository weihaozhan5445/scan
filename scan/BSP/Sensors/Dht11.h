#ifndef __INF_DHT11__
#define __INF_DHT11__


#include "gpio.h"
#include "com_debug.h"
#include "FreeRTOS.h"
#include "task.h"
#include "delay.h"


#define DHT11_DATA_H HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,DHT11_DATA_Pin,GPIO_PIN_SET)
#define DHT11_DATA_L HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,DHT11_DATA_Pin,GPIO_PIN_RESET)
#define DHT11_DATA_Read HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port,DHT11_DATA_Pin)

void dht11_init(void);

void dht11_get_date(float *temperature,float *humidity);

#endif
