#ifndef __APP_RTOS_H
#define __APP_RTOS_H
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "APP_GetData.h"
#include "OLED.h"
#include "APP_show.h"
#include <string.h>
#include "../OTA/ESP8266.h"
#include "ota_task.h"
#include <stdint.h>
/* 全局消息队列：传递环境数据结构体，整个项目共用 */
extern QueueHandle_t xEnvDataQueue;

/* 四个任务句柄（可选，用于任务启停） */
extern TaskHandle_t xGetDataTaskHandle;
extern TaskHandle_t xShowTaskHandle;
extern TaskHandle_t xAlarmTaskHandle;
extern TaskHandle_t xUploadTaskHandle;
extern TaskHandle_t xOtaTaskHandle;
/* 任务创建总入口，main函数调用 */
void App_CreateAllTask(void);

#endif
