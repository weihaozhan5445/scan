/*
 * ============================================================================
 * 文件: ESP8266.h
 * 功能: ESP8266 WiFi 模组驱动(USART1 AT 指令) —— 阿里云 MQTT + OTA HTTP。
 *
 * 使用说明:
 *   WiFi 账号密码与阿里云三元组在 wifi_config.h 中配置(该文件不入库,
 *   模板为 wifi_config.h.example, 新克隆仓库后请先复制模板并填入真实值);
 *   模组走 AT 固件: AT+CWJAP 连 WiFi, AT+QMTOPEN/QMTCONN/QMTSUB 建 MQTT,
 *   AT+QMTPUB 发布, AT+HTTPCGET 分段下载 OTA 固件。
 * ============================================================================
 */
#ifndef __ESP8266_H
#define __ESP8266_H
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define ESP_USART_HANDLE huart1             /* ESP8266 挂在 USART1 上      */
#define ESP_RX_BUF_LEN 256                  /* 接收缓冲区大小(字节)       */
extern uint8_t esp_rx_buf[ESP_RX_BUF_LEN];  /* ESP 返回数据缓冲区(中断填充) */
extern uint16_t esp_rx_idx;                 /* 有效数据长度(空闲中断更新)  */

/* ==========================================================================
 * 凭据配置: wifi_config.h(本机真实值, 不入库) / wifi_config.h.example(模板)
 * ======================================================================== */
#include "wifi_config.h"

/* MQTT 参数拼接(基于 wifi_config.h 中的三元组) */
#define MQTT_CLIENT_ID DEV_ID "|securemode=3,signmethod=hmacsha1"
#define MQTT_USERNAME   PRO_ID "&" DEV_ID
#define MQTT_PASSWORD   DEV_SECRET
/* 上行：设备上报数据 */
#define MQTT_UP_TOPIC   "/sys/" PRO_ID "/" DEV_ID "/thing/event/property/post"
/* 下行：云端下发指令（阈值/OTA） */
#define MQTT_DOWN_TOPIC "/sys/" PRO_ID "/" DEV_ID "/thing/event/property/set"

/* 底层AT发送(发完延时 wait_ms 等待响应) */
void ESP_SendAT(char *cmd, uint32_t wait_ms);
/* 等待指定响应字符串出现, 1=成功 0=超时 */
uint8_t ESP_WaitResp(uint32_t timeout_ms, uint8_t *target_str);
/* WiFi连接 + MQTT初始化(含订阅下行主题), 1=成功 */
uint8_t ESP_MQTT_Init(void);
/* MQTT发布消息(AT+QMTPUB) */
void ESP_MQTT_Publish(char *topic, char *msg);
/* 读取订阅到的下行JSON报文, 1=取到 */
uint8_t ESP_MQTT_GetRecvMsg(char *out_buf, uint16_t buf_len);
/* HTTP GET分段下载固件分包, 返回本包长度(0=超时) */
uint16_t ESP_Http_Download_Block(uint8_t *recv_buf, uint32_t timeout);
/* 提取JSON中数字字段(32位, 修复原8位溢出) */
uint32_t GetJsonNum32(char *json, const char *key);
/* 提取JSON中字符串字段 */
void GetJsonStr(char *json, const char *key, char *dst, uint16_t dst_len);
/* 串口空闲中断回调(收到一段数据时由 HAL 调用) */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
/* 串口错误回调(溢出/帧错后重新挂接收) */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);

#endif
