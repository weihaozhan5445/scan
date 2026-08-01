/*
 * ============================================================================
 * 文件: Int_w24c02.h
 * 功能: W24C02(256B I2C EEPROM)驱动 —— 存放下电不丢失的运行参数。
 *
 * 地址分配(与 Bootloader 工程 bl_config.h 的 EEPROM 表一一对应):
 *   0x00 出厂初始化标记, 0x01/0x02 当前固件版本, 0x03 升级状态机,
 *   0x04/0x05 新版本号, 0x06 启动失败计数, 0x07/0x08 新固件长度。
 * ============================================================================
 */
#ifndef __INT_W24C02_H
#define __INT_W24C02_H

#include <stdio.h>
#include "i2c.h"
#include "usart.h"

/* ---- 器件地址 ---- */
#define W24C02_ADDR 0xA0                 /* 写地址(7位地址0x50左移1位) */
#define W24C02_ADDR_R (W24C02_ADDR | 0x01)  /* 读地址 */

#define W24C02_ADDR_SIZE 8               /* 内部地址宽度 8 位 */
#define W24C02_PAGE_SIZE 16              /* EEPROM 页大小 16 字节(连续写限制) */

/* ---- EEPROM 地址分配表 ---- */
#define ADDR_EEPROM_INIT_FLAG   0x00U  /* 出厂初始化标记(0xAA=已初始化)      */
#define ADDR_FW_VER_MAJOR      0x01U  /* 当前固件主版本                      */
#define ADDR_FW_VER_MINOR      0x02U  /* 当前固件次版本                      */
#define ADDR_UPGRADE_FLAG      0x03U  /* Bootloader 升级状态机字节            */
#define ADDR_NEW_VER_MAJ       0x04U  /* OTA 暂存的新版本主号                 */
#define ADDR_NEW_VER_MIN       0x05U  /* OTA 暂存的新版本次号                 */
#define ADDR_BOOT_ATTEMPT_CNT  0x06U  /* 连续启动失败计数(升级确认用)          */
#define ADDR_FW_LEN_H          0x07U  /* 暂存固件 payload 长度高字节           */
#define ADDR_FW_LEN_L          0x08U  /* 暂存固件 payload 长度低字节           */

/* ---- Bootloader 状态机取值(与 Bootloader/bl_config.h 保持一致) ----
 * 0xFF=NORMAL(正常/已确认) → 0xAA=STAGED(待烧写) → 0xCC=FLASHED(待确认)
 * 0x5A=ENTER_BL(请求进入 Bootloader 控制台)                            */
#define BOOT_STATE_NORMAL   0xFFU
#define BOOT_STATE_STAGED   0xAAU
#define BOOT_STATE_FLASHED  0xCCU
#define BOOT_STATE_ENTER_BL 0x5AU

/* ---- 静态初始版本(出厂写入) ---- */
#define FW_VER_MAJOR 1U
#define FW_VER_MINOR 0U

/* 读一个字节(返回该地址的值) */
uint8_t Int_w24c02_read_byte(uint8_t byte_addr);

/* 写一个字节(内部已处理 EEPROM 写周期等待) */
void Int_w24c02_write_byte(uint8_t byte_addr, uint8_t data);

/* 连续读 len 字节 */
void Int_w24c02_read_bytes(uint8_t byte_addr, uint8_t *data, uint16_t len);

/* 连续写 len 字节(自动分页, 页间等待写周期) */
void Int_w24c02_write_bytes(uint8_t byte_addr, uint8_t *data, uint16_t len);

#endif /* !__INT_W24C02_H */
