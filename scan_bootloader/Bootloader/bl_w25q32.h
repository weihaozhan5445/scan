/*
 * 文件: bl_w25q32.h
 * 功能: W25Q32(4MB SPI NOR Flash)驱动接口 —— 暂存/备份 OTA 固件用。
 * 说明: 全部使用 32 位绝对地址, 读写均带越界检查与忙等待超时。
 */
#ifndef BL_W25Q32_H
#define BL_W25Q32_H

#include <stdint.h>

/* 初始化并检测 W25Q32 是否存在, 1=检测到, 0=未检测到 */
uint8_t  bl_w25q32_init(void);

/* 从 addr 读取 len 字节到 buf */
uint8_t  bl_w25q32_read(uint32_t addr, uint8_t *buf, uint32_t len);

/* 向 addr 写 len 字节(自动处理 256B 页边界, 写入前区域必须已擦除) */
uint8_t  bl_w25q32_write(uint32_t addr, const uint8_t *buf, uint32_t len);

/* 擦除 addr 所在的 4KB 扇区(addr 必须 4KB 对齐) */
uint8_t  bl_w25q32_erase_sector(uint32_t addr);

/* 擦除 [addr, addr+len) 覆盖的所有扇区(addr 必须 4KB 对齐) */
uint8_t  bl_w25q32_erase_range(uint32_t addr, uint32_t len);

/* 计算 W25Q32 上 [addr, addr+len) 区域的 CRC-32 */
uint32_t bl_w25q32_crc(uint32_t addr, uint32_t len);

#endif
