/*
 * ============================================================================
 * 文件: Int_w24c02.c
 * 功能: W24C02 EEPROM 的 I2C 读写实现(基于 HAL I2C2)。
 *
 * I2C 读流程(寄存器寻址读):
 *   起始 → 写器件地址(W) → 写内部字节地址 → 重复起始 → 读器件地址(R)
 *   → 连续读数据 → NACK + 停止。
 * I2C 写流程(寄存器寻址写):
 *   起始 → 写器件地址(W) → 写内部字节地址 → 写数据 → 停止。
 *
 * 注意: EEPROM 每完成一次写操作需要约 5ms 内部写周期, 连续写之间
 *       必须等待, 否则数据会丢(见 write_byte 内的 HAL_Delay)。
 * ============================================================================
 */
#include "Int_w24c02.h"

/*
 * 读一个字节:
 *   HAL_I2C_Mem_Read 参数依次为 句柄(I2C2)、从机读地址、内部地址、
 *   内部地址宽度(8位)、数据缓冲、长度(1)、超时。
 */
uint8_t Int_w24c02_read_byte(uint8_t byte_addr)
{
    uint8_t data;

    HAL_I2C_Mem_Read(&hi2c2, W24C02_ADDR_R, byte_addr, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    return data;
}

/*
 * 写一个字节:
 *   写完立即延时 6ms(EEPROM 写周期约 5ms), 保证下一次读写不会因
 *   内部"正在编程"而被 NACK。
 */
void Int_w24c02_write_byte(uint8_t byte_addr, uint8_t data)
{
    (void)HAL_I2C_Mem_Write(&hi2c2, W24C02_ADDR, byte_addr, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    HAL_Delay(6U);
}

/* 连续读: EEPROM 读操作可以跨页连续读, 无需分页 */
void Int_w24c02_read_bytes(uint8_t byte_addr, uint8_t *data, uint16_t len)
{
    HAL_I2C_Mem_Read(&hi2c2, W24C02_ADDR_R, byte_addr, I2C_MEMADD_SIZE_8BIT, data, len, 1000);
}

/*
 * 连续写(自动分页):
 *   EEPROM 一次连续写不能超过一页(16B), 跨页会回绕覆盖页首,
 *   因此按"当前页剩余空间"拆成多段, 段与段之间等待写周期。
 *   健壮性: 地址越界(byte_addr+len>255)时直接拒绝并打印提示。
 */
void Int_w24c02_write_bytes(uint8_t byte_addr, uint8_t *data, uint16_t len)
{
    /* 1.0 健壮性判断: 目标范围不能超过 EEPROM 容量 */
    if (byte_addr + len > 255)
    {
        printf("写入的地址值超过EEPROM的地址值\n");
        return;
    }

    /* 1.1 当前页剩余空间 = 页大小 - 页内偏移 */
    uint8_t page_remain_len = 16 - byte_addr % 16;

    if (len <= page_remain_len)
    {
        /* 可以一次写完 */
        HAL_I2C_Mem_Write(&hi2c2, W24C02_ADDR, byte_addr, I2C_MEMADD_SIZE_8BIT, data, len, 1000);
    }
    else
    {
        uint8_t start_page_addr = byte_addr;   /* 本次待写起始地址 */
        uint8_t page_count = 0;                /* 已写段计数       */

        /* 循环: 每段写满当前页剩余空间, 再进入下一页 */
        while (len > page_remain_len)
        {
            HAL_I2C_Mem_Write(&hi2c2, W24C02_ADDR, start_page_addr, I2C_MEMADD_SIZE_8BIT,
                              data + page_count * 16, page_remain_len, 1000);
            page_count++;
            start_page_addr += page_remain_len;
            len -= page_remain_len;
            page_remain_len = 16;
            /* 每段写完等待 EEPROM 内部写周期 */
            HAL_Delay(10);
        }
        /* 最后一页(不足一页)收尾 */
        if (len != 0)
        {
            HAL_I2C_Mem_Write(&hi2c2, W24C02_ADDR, start_page_addr, I2C_MEMADD_SIZE_8BIT,
                              data + page_count * 16, len, 1000);
        }
    }
}
