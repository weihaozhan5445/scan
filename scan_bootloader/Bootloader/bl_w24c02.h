/*
 * 文件: bl_w24c02.h
 * 功能: W24C02(256B I2C EEPROM)驱动接口 —— 存放升级状态/版本号。
 */
#ifndef BL_W24C02_H
#define BL_W24C02_H

#include <stdint.h>
#include <stddef.h>

#define W24C02_DEV_ADDR   0xA0U    /* 器件地址(7位地址左移1位) */
#define W24C02_PAGE_SIZE  16U      /* EEPROM 一页 16 字节(写限制) */

/* 读一个字节 */
uint8_t bl_w24c02_read_byte(uint8_t addr, uint8_t *out);

/* 写一个字节(内部等待 EEPROM 写周期) */
uint8_t bl_w24c02_write_byte(uint8_t addr, uint8_t data);

/* 连续读 len 字节 */
uint8_t bl_w24c02_read_bytes(uint8_t addr, uint8_t *buf, uint16_t len);

/* 连续写 len 字节(自动分页, 每页之间等待写周期) */
uint8_t bl_w24c02_write_bytes(uint8_t addr, const uint8_t *buf, uint16_t len);

#endif
