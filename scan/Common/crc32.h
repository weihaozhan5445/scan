/*
 * 文件: crc32.h
 * 功能: App 侧 CRC-32 接口(与 Bootloader 的 bl_crc32 算法一致)。
 * 用途: OTA 固件下载时边下载边算 CRC, 与镜像头里的 crc32 比对。
 */
#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

/* 生成查找表(只需调用一次) */
void     CRC32_InitTable(void);

/* 增量累加: data[0..len-1] 计入 crc */
uint32_t CRC32_Update(uint32_t crc, const uint8_t *data, uint32_t len);

/* 收尾异或, 得到标准 CRC-32 */
uint32_t CRC32_Final(uint32_t crc);

#endif
