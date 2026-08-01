/*
 * 文件: bl_crc32.h
 * 功能: CRC-32 校验接口(IEEE 802.3 / zlib 兼容)。
 * 说明: 与 App 工程 Common/crc32.c 完全相同的算法, 保证两端校验结果一致。
 */
#ifndef BL_CRC32_H
#define BL_CRC32_H

#include <stdint.h>
#include <stddef.h>

/* 初始化 CRC-32 查找表(内部 256 项, 只需调用一次) */
void     bl_crc32_init(void);

/* 累加计算: 把 data[0..len-1] 计入当前 crc, 返回新 crc */
uint32_t bl_crc32_update(uint32_t crc, const uint8_t *data, uint32_t len);

/* 收尾: 对累加结果做最终异或, 得到标准 CRC-32 值 */
uint32_t bl_crc32_final(uint32_t crc);

#endif
