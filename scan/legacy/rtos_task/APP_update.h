#ifndef __APP_UPDATE_H
#define __APP_UPDATE_H
#include "stm32f1xx_hal.h"
#include "Int_w25q32.h"
#include "Int_w24c02.h"
#include "ESP8266.h"
#include "ota_task.h"

#define APP_DATA_MAX_LEN 256U

// OTA运行状态枚举
typedef enum
{
    UPDATE_IDLE,                // 空闲：正常环境监测业务
    UPDATE_RECV_SEND_CMD,       // MQTT收到升级指令，启动HTTP下载固件
    UPDATE_RECV_CHECK_DATA,     // 固件全部下载完成，全局CRC校验
    UPDATE_RECV_BOOT_UPDATE,    // W24C02写入待升级标记
    UPDATE_END                  // 软件复位，跳转Bootloader
} UpdateStateDef;

extern UpdateStateDef update_state;
extern uint8_t app_data_buff[APP_DATA_MAX_LEN];
extern uint32_t recv_fw_total_len;

// 状态机循环处理函数
void App_update_work(void);
// ESP HTTP下载固件存入W25Q32缓存
uint8_t App_DownloadFwByUrl(char *url, uint32_t total_len, uint32_t expect_crc);
// 读取W25Q固件计算全局CRC校验
void App_CheckFwCrc(void);
// 写入EEPROM升级标记，告知Boot复位后更新固件
void App_SetBootUpgradeFlag(void);
#endif

