
#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/********************* 配置区，只改这里 *********************/
#define WIFI_NAME       "你的WiFi名字"
#define WIFI_PWD        "你的WiFi密码"

#define PRO_ID          "你的ProductKey"
#define DEV_ID          "你的DeviceName"
#define DEV_SECRET      "你的DeviceSecret"

#define MQTT_PUB_TOPIC  "$sys/"PRO_ID"/"DEV_ID"/dp/post/json"
/************************************************************/

extern UART_HandleTypeDef huart1;

uint8_t ESP_Send_AT(uint8_t *cmd);
uint8_t ESP_Wait_Resp(uint32_t timeout_ms, uint8_t *resp_str);

uint8_t ESP8266_Connect_Wifi(char *ssid, char *pwd);
uint8_t ESP8266_Connect_TCP(char *ip, char *port);

// MQTT连接报文全局缓存+长度
extern uint8_t con_buf[150];
extern uint16_t pos;

// MQTT发布上报数据
uint8_t ESP8266_MQTT_Publish(char *json_str);

#endif

