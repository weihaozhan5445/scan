/*
 * 文件: storage_map.h
 * 功能: 存储映射兼容层。
 * 说明: ADDR_FW_INIT_FLAG 是 ADDR_EEPROM_INIT_FLAG 的兼容别名,
 *       EEPROM 地址表完整定义见 BSP/Storage/Int_w24c02.h。
 */
#ifndef STORAGE_MAP_H
#define STORAGE_MAP_H

#include <stdint.h>

/* 兼容别名: 固件初始化标记 == EEPROM 出厂初始化标记 */
#define ADDR_FW_INIT_FLAG ADDR_EEPROM_INIT_FLAG

/* 兼容函数: 单字节写 EEPROM(实现见 ota_command_service.c) */
void W24C02_WriteByte(uint8_t byte_addr, uint8_t data);

#endif
