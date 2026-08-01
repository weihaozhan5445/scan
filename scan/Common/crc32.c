/*
 * ============================================================================
 * 文件: crc32.c
 * 功能: 标准 CRC-32(zlib 兼容)实现。
 *
 * 参数:
 *   多项式 0x04C11DB7(反射 0xEDB88320), 初值 0xFFFFFFFF, 结果异或 0xFFFFFFFF。
 * 该算法与 Python zlib.crc32() 一致, 方便在电脑上用脚本生成/核对 OTA 镜像。
 *
 * 实现: 256 项查表法。下载 34KB 固件边收边算, 开销可忽略。
 * ============================================================================
 */
#include "crc32.h"

/* CRC 查找表(static 放 .bss, 由 CRC32_InitTable 填充) */
static uint32_t crc32_table[256];
static uint8_t  crc32_table_ready = 0U;

/* 生成查找表: 对 0..255 做 8 次反射迭代 */
void CRC32_InitTable(void)
{
    uint32_t i;

    if (crc32_table_ready != 0U)
    {
        return;
    }
    for (i = 0U; i < 256U; i++)
    {
        uint32_t c = i;
        uint8_t  k;

        for (k = 0U; k < 8U; k++)
        {
            c = (c & 1U) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_table_ready = 1U;
}

/* 增量更新: 查表法, 每次处理 1 字节 */
uint32_t CRC32_Update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    if ((data == NULL) || (crc32_table_ready == 0U))
    {
        return crc;
    }
    for (i = 0U; i < len; i++)
    {
        crc = crc32_table[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8);
    }
    return crc;
}

/* 收尾异或 0xFFFFFFFF */
uint32_t CRC32_Final(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFUL;
}
