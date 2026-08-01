/*
 * 文件: bl_flash.h
 * 功能: STM32F103 内部 Flash 的擦除/编程/校验接口(Bootloader 专用)。
 * 安全约束: 所有操作只允许作用于 App 区域(0x08004000 - 0x0800FFFF),
 *           从设计上杜绝误擦 Bootloader 自身。
 */
#ifndef BL_FLASH_H
#define BL_FLASH_H

#include <stdint.h>

/* 解锁内部 Flash(写 KEYR 密钥序列) */
void    bl_flash_init(void);

/* 擦除 [addr, addr+len) 区域, 按 1KB 扇区逐个擦除 */
uint8_t bl_flash_erase_range(uint32_t addr, uint32_t len);

/* 向 Flash 写数据(addr/len 必须 4 字节对齐, 内部按半字编程) */
uint8_t bl_flash_write(uint32_t addr, const uint8_t *buf, uint32_t len);

/* 逐字节比对 Flash 内容与内存缓冲, 用于烧写后验证 */
uint8_t bl_flash_verify(uint32_t addr, const uint8_t *buf, uint32_t len);

/* 计算 Flash 上 [addr, addr+len) 区域的 CRC-32 */
uint8_t bl_flash_crc(uint32_t addr, uint32_t len, uint32_t *out);

#endif
