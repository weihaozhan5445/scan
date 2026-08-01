/*
 * ============================================================================
 * 文件: Int_w25q32.c
 * 功能: W25Q32 SPI NOR Flash 驱动实现(基于 HAL SPI1)。
 *
 * 通用时序:
 *   读 : CS低 → 发指令+地址 → 连续读 → CS高
 *   写 : 写使能 → CS低 → 发指令+地址+数据 → CS高 → 等忙清
 *   擦除: 写使能 → CS低 → 发指令+地址 → CS高 → 等忙清
 * ============================================================================
 */
#include "Int_w25q32.h"

/* 拉低片选: 选中 W25Q32, 开始一次指令事务 */
void Int_w25q32_start(void)
{
    HAL_GPIO_WritePin(W25Q32_CS_GPIO_Port, W25Q32_CS_Pin, GPIO_PIN_RESET);
}

/* 拉高片选: 结束事务 */
void Int_w25q32_stop(void)
{
    HAL_GPIO_WritePin(W25Q32_CS_GPIO_Port, W25Q32_CS_Pin, GPIO_PIN_SET);
}

/* SPI 发送一个字节(HAL 阻塞发送) */
void Int_w25q32_write_byte(uint8_t data)
{
    HAL_SPI_Transmit(&hspi1, &data, 1, 100);
}

/* SPI 接收一个字节(HAL 阻塞接收) */
uint8_t Int_w25q32_read_byte(void)
{
    uint8_t data;

    HAL_SPI_Receive(&hspi1, &data, 1, 100);
    return data;
}

/*
 * 读取芯片 ID: 发 0x9F 后读回 3 字节(厂商 + 器件ID高/低)。
 * W25Q32 应为 0xEF 0x40 0x16。
 */
void Int_w25q32_read_id(uint8_t *mf_id, uint16_t *device_id)
{
    Int_w25q32_start();
    Int_w25q32_write_byte(W25Q32_READ_ID);
    *mf_id = Int_w25q32_read_byte();
    {
        uint8_t high = Int_w25q32_read_byte();
        uint8_t low = Int_w25q32_read_byte();
        *device_id = (uint16_t)((high << 8) | low);
    }
    Int_w25q32_stop();
}

/* 内部: 等待芯片不忙(状态寄存器 bit0=0)。擦除/编程后调用。 */
static void Int_w25q32_wait_busy(void)
{
    Int_w25q32_start();
    while (1)
    {
        Int_w25q32_write_byte(W25Q32_READ_STATUS_REG);
        uint8_t status = Int_w25q32_read_byte();
        if ((status & 0x01) == 0)
        {
            break;
        }
    }
    Int_w25q32_stop();
}

/* 旧接口: 按 block(64KB)/sector(4KB)/page(256B)/addr(页内偏移)读数据 */
void Int_w25q32_read_data(uint8_t block, uint8_t sector, uint8_t page, uint8_t addr, uint8_t *data, uint16_t len)
{
    Int_w25q32_wait_busy();
    Int_w25q32_start();

    Int_w25q32_write_byte(W25Q32_READ_DATA);
    uint32_t addr_24 = (uint32_t)block << 16 | (uint32_t)sector << 12 | (uint32_t)page << 8 | addr;
    Int_w25q32_write_byte(addr_24 >> 16);
    Int_w25q32_write_byte(addr_24 >> 8);
    Int_w25q32_write_byte(addr_24);

    for (uint16_t i = 0; i < len; i++)
    {
        data[i] = Int_w25q32_read_byte();
    }
    Int_w25q32_stop();
}

/* 使用 32 位绝对地址读数据(读不受页边界限制) */
void Int_w25q32_read_data_with_32addr(uint32_t addr, uint8_t *data, uint16_t len)
{
    Int_w25q32_wait_busy();
    Int_w25q32_start();

    Int_w25q32_write_byte(W25Q32_READ_DATA);
    Int_w25q32_write_byte((addr >> 16) & 0xff);
    Int_w25q32_write_byte((addr >> 8) & 0xff);
    Int_w25q32_write_byte(addr & 0xff);

    for (uint16_t i = 0; i < len; i++)
    {
        data[i] = Int_w25q32_read_byte();
    }
    Int_w25q32_stop();
}

/* 内部: 发送写使能(擦除/编程前必须执行) */
static void Int_w25q32_write_enable(void)
{
    Int_w25q32_wait_busy();
    Int_w25q32_start();
    Int_w25q32_write_byte(W25Q32_WRITE_ENABLE);
    Int_w25q32_stop();
}

/* 旧接口: 按拆分地址写数据(假设不超出 1 页范围) */
void Int_w25q32_write_data(uint8_t block, uint8_t sector, uint8_t page, uint8_t addr, uint8_t *data, uint16_t len)
{
    Int_w25q32_write_enable();
    Int_w25q32_start();
    uint32_t addr_24 = (uint32_t)block << 16 | (uint32_t)sector << 12 | (uint32_t)page << 8 | addr;
    Int_w25q32_write_byte(W25Q32_WRITE_DATA);
    Int_w25q32_write_byte(addr_24 >> 16);
    Int_w25q32_write_byte(addr_24 >> 8);
    Int_w25q32_write_byte(addr_24);
    for (uint16_t i = 0; i < len; i++)
    {
        Int_w25q32_write_byte(data[i]);
    }
    Int_w25q32_stop();
}

/* 使用 32 位绝对地址写数据(1 次最多 1 页, 假设不跨页) */
void Int_w25q32_write_data_with_32addr(uint32_t addr, uint8_t *data, uint16_t len)
{
    Int_w25q32_write_enable();
    Int_w25q32_start();

    Int_w25q32_write_byte(W25Q32_WRITE_DATA);
    Int_w25q32_write_byte((addr >> 16) & 0xff);
    Int_w25q32_write_byte((addr >> 8) & 0xff);
    Int_w25q32_write_byte(addr & 0xff);

    for (uint16_t i = 0; i < len; i++)
    {
        Int_w25q32_write_byte(data[i]);
    }
    Int_w25q32_stop();
}

/* 旧接口: 按 block/sector 擦除 1 个 4KB 扇区 */
void Int_w25q32_erase_sector(uint8_t block, uint8_t sector)
{
    Int_w25q32_write_enable();
    Int_w25q32_start();
    uint32_t addr = (uint32_t)block * 65536 + (uint32_t)sector * 4096;
    Int_w25q32_write_byte(W25Q32_ERASE_SECTOR);
    Int_w25q32_write_byte((addr >> 16) & 0xff);
    Int_w25q32_write_byte((addr >> 8) & 0xff);
    Int_w25q32_write_byte(addr & 0xff);
    Int_w25q32_stop();
}

/* 使用 32 位地址擦除 1 个 4KB 扇区(OTA 暂存区按需擦除用) */
void Int_w25q32_erase_4k(uint32_t addr)
{
    Int_w25q32_write_enable();
    Int_w25q32_start();
    Int_w25q32_write_byte(W25Q32_ERASE_SECTOR);
    Int_w25q32_write_byte((addr >> 16) & 0xff);
    Int_w25q32_write_byte((addr >> 8) & 0xff);
    Int_w25q32_write_byte(addr & 0xff);
    Int_w25q32_stop();
}

/*
 * 跨页安全写入(OTA 下载固件使用):
 * 由于页编程不能跨 256B 页边界, 这里把一次写入按页边界自动拆成多段,
 * 每段一页, 段与段之间等忙。这样调用方无需关心地址是否对齐。
 */
void Int_w25q32_write_data_safe(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t done = 0U;

    while (done < len)
    {
        /* 当前页剩余空间 */
        uint32_t page_left = 256U - ((addr + done) % 256U);
        uint32_t chunk = len - done;
        uint32_t i;

        if (chunk > page_left)
        {
            chunk = page_left;
        }

        Int_w25q32_write_enable();
        Int_w25q32_start();
        Int_w25q32_write_byte(W25Q32_WRITE_DATA);
        {
            uint32_t a = addr + done;
            Int_w25q32_write_byte((a >> 16) & 0xff);
            Int_w25q32_write_byte((a >> 8) & 0xff);
            Int_w25q32_write_byte(a & 0xff);
        }
        for (i = 0U; i < chunk; i++)
        {
            Int_w25q32_write_byte(data[done + i]);
        }
        Int_w25q32_stop();
        done += chunk;              /* 下一次循环先等上一段编程完成 */
    }
}
