/*
 * 文件: bl_i2c.h
 * 功能: Bootloader 的 I2C2 主模式驱动(用于读写 W24C02 EEPROM)。
 * 引脚: PB10=SCL, PB11=SDA, 100kHz。
 */
#ifndef BL_I2C_H
#define BL_I2C_H

#include <stdint.h>
#include <stddef.h>

/* 初始化 I2C2: 主模式 100kHz */
void    bl_i2c_init(void);

/* 向从机写 len 字节(不带寄存器地址, 一般用于直接写命令) */
uint8_t bl_i2c_write(uint8_t dev_addr, const uint8_t *buf, uint16_t len);

/* 从从机连续读 len 字节(不带寄存器地址) */
uint8_t bl_i2c_read (uint8_t dev_addr, uint8_t *buf, uint16_t len);

/* 向从机指定寄存器地址写 len 字节(EEPROM 写数据用) */
uint8_t bl_i2c_mem_write(uint8_t dev_addr, uint8_t mem_addr, const uint8_t *buf, uint16_t len);

/* 从从机指定寄存器地址读 len 字节(EEPROM 读数据用, 内部含重复起始位) */
uint8_t bl_i2c_mem_read (uint8_t dev_addr, uint8_t mem_addr, uint8_t *buf, uint16_t len);

#endif
