/*
 * ============================================================================
 * 文件: bl_w24c02.c
 * 功能: W24C02 EEPROM 驱动。
 *
 * 特性:
 *   - 256 字节容量, 地址 0x00-0xFF;
 *   - 页大小 16 字节: 一次连续写不能跨页(跨页会回绕);
 *   - 每次写操作后芯片内部要约 5ms 写周期, 期间不接受新写命令,
 *     因此连续写之间必须等待, 否则数据会丢。
 * ============================================================================
 */
#include "bl_w24c02.h"
#include "bl_i2c.h"
#include "bl_delay.h"
#include "bl_config.h"

/* 读一个字节: 内部寄存器寻址读(伪写设置地址 + 重复起始读 1 字节) */
uint8_t bl_w24c02_read_byte(uint8_t addr, uint8_t *out)
{
    if ((addr > 255U) || (out == NULL))
    {
        return 0U;
    }
    return bl_i2c_mem_read(W24C02_DEV_ADDR, addr, out, 1U);
}

/* 写一个字节: 寄存器寻址写 1 字节, 随后等待 EEPROM 内部写周期 */
uint8_t bl_w24c02_write_byte(uint8_t addr, uint8_t data)
{
    uint8_t ok;

    if (addr > 255U)
    {
        return 0U;
    }
    ok = bl_i2c_mem_write(W24C02_DEV_ADDR, addr, &data, 1U);
    bl_delay_ms(6U);          /* EEPROM 写周期约 5ms, 留 1ms 余量 */
    return ok;
}

/* 连续读(EEPROM 读支持跨页, 无需分页处理) */
uint8_t bl_w24c02_read_bytes(uint8_t addr, uint8_t *buf, uint16_t len)
{
    if ((addr + len) > 256U)
    {
        return 0U;
    }
    return bl_i2c_mem_read(W24C02_DEV_ADDR, addr, buf, len);
}

/*
 * 连续写: 按 16 字节页边界拆段, 每段一次页写, 段之间等待写周期。
 * 地址范围 [addr, addr+len) 必须落在 256B 内。
 */
uint8_t bl_w24c02_write_bytes(uint8_t addr, const uint8_t *buf, uint16_t len)
{
    uint16_t done = 0U;

    if ((buf == NULL) || ((uint16_t)addr + len) > 256U)
    {
        return 0U;
    }
    while (done < len)
    {
        /* 当前页剩余空间 */
        uint16_t page_left = W24C02_PAGE_SIZE - ((addr + done) % W24C02_PAGE_SIZE);
        uint16_t chunk = len - done;

        if (chunk > page_left)
        {
            chunk = page_left;
        }
        if (!bl_i2c_mem_write(W24C02_DEV_ADDR, (uint8_t)(addr + done), buf + done, chunk))
        {
            return 0U;
        }
        bl_delay_ms(6U);      /* 页写周期等待 */
        done += chunk;
    }
    return 1U;
}
