/*
 * ============================================================================
 * 文件: bl_crc32.c
 * 功能: CRC-32 实现, 用于校验 OTA 固件完整性。
 *
 * 算法参数(标准 CRC-32 / zlib 兼容):
 *   多项式  : 0x04C11DB7(反射形式 0xEDB88320)
 *   初值    : 0xFFFFFFFF
 *   结果异或: 0xFFFFFFFF
 * 该算法与 Python 的 zlib.crc32() 完全一致, 便于在电脑上用脚本生成/核对镜像。
 *
 * 实现方式: 256 项查找表(查表法), 比逐位计算快约 8 倍,
 * 对 48KB 固件做一次校验约几毫秒, 完全可接受。
 * ============================================================================
 */
#include "bl_crc32.h"

/* 查找表: table[i] 表示"以 i 为前导字节时的 CRC 增量" */
static uint32_t crc_table[256];
static uint8_t  crc_table_ready = 0U;   /* 表是否已生成, 避免重复计算        */

/*
 * 生成查找表(只需执行一次):
 * 对 0..255 每个值做 8 次"向右移位 + 必要时异或多项式"的迭代,
 * 得到与反射式 CRC 算法匹配的查表项。
 */
void bl_crc32_init(void)
{
    uint32_t i;

    if (crc_table_ready != 0U)
    {
        return;                     /* 已经生成过, 直接返回                */
    }
    for (i = 0U; i < 256U; i++)
    {
        uint32_t c = i;
        uint8_t  k;

        for (k = 0U; k < 8U; k++)
        {
            /* 最低位为1时异或反射多项式, 否则只右移 */
            c = (c & 1U) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        }
        crc_table[i] = c;
    }
    crc_table_ready = 1U;
}

/*
 * 增量更新 CRC:
 * 对每个输入字节: 取 (当前CRC低8位 ^ 数据字节) 查表, 再与 CRC>>8 异或。
 * 数据可分段多次调用(例如边下载边算), 顺序必须与写入顺序一致。
 */
uint32_t bl_crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    if ((data == NULL) || (crc_table_ready == 0U))
    {
        return crc;
    }
    for (i = 0U; i < len; i++)
    {
        crc = crc_table[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8);
    }
    return crc;
}

/* 收尾: 标准 CRC-32 需要在最后异或 0xFFFFFFFF */
uint32_t bl_crc32_final(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFUL;
}
