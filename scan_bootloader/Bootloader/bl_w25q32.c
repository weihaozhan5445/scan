/*
 * ============================================================================
 * 文件: bl_w25q32.c
 * 功能: W25Q32 SPI NOR Flash 驱动。
 *
 * W25Q32 关键特性:
 *   - 容量 4MB(0x000000-0x3FFFFF), 扇区 4KB, 页 256B;
 *   - 写入只能把 1 改成 0, 所以写之前必须先擦除(擦除后全为 0xFF);
 *   - 页编程: 一次最多写 256B, 且【不能跨页】(跨页会自动回绕到页首);
 *   - 每次擦除/编程前需要发写使能(0x06), 之后轮询状态寄存器忙位。
 *
 * 常用指令:
 *   0x06 写使能      0x05 读状态寄存器
 *   0x03 读数据      0x02 页编程(256B)
 *   0x20 扇区擦除    0x9F 读 JEDEC ID
 * ============================================================================
 */
#include "bl_w25q32.h"
#include "bl_spi.h"
#include "bl_delay.h"
#include "bl_crc32.h"
#include "bl_config.h"
#include "stm32f1xx.h"

/* ---- W25Q32 指令码 ---- */
#define W25Q_CMD_WRITE_ENABLE  0x06U
#define W25Q_CMD_READ_STATUS   0x05U
#define W25Q_CMD_READ_DATA     0x03U
#define W25Q_CMD_PAGE_PROGRAM  0x02U
#define W25Q_CMD_SECTOR_ERASE  0x20U
#define W25Q_CMD_READ_ID       0x9FU

/* 片选: PA4 低电平选中芯片, 高电平释放 */
#define W25Q_CS_LOW()  GPIOA->BRR = GPIO_BRR_BR4
#define W25Q_CS_HIGH() GPIOA->BSRR = GPIO_BSRR_BS4

/*
 * 轮询状态寄存器忙位, 直到芯片空闲或超时(约 2 秒, 扇区擦除最坏约 400ms)。
 * 长时间等待时每 100 轮释放一次片选并延时 1ms, 避免总线长时间占用。
 */
static uint8_t w25_busy_wait(void)
{
    uint32_t t;

    W25Q_CS_LOW();
    (void)bl_spi_xfer(W25Q_CMD_READ_STATUS);
    t = 0U;
    while (t < 2000U)
    {
        uint8_t status = bl_spi_xfer(0xFFU);

        /* 状态寄存器 bit0=忙标志, 0 表示空闲 */
        if ((status & 0x01U) == 0U)
        {
            W25Q_CS_HIGH();
            return 1U;
        }
        t++;
        if ((t % 100U) == 0U)
        {
            W25Q_CS_HIGH();
            bl_delay_ms(1U);
            W25Q_CS_LOW();
            (void)bl_spi_xfer(W25Q_CMD_READ_STATUS);
        }
    }
    W25Q_CS_HIGH();
    return 0U;
}

/* 发送写使能命令(擦除/编程前必须执行) */
static void w25_write_enable(void)
{
    W25Q_CS_LOW();
    (void)bl_spi_xfer(W25Q_CMD_WRITE_ENABLE);
    W25Q_CS_HIGH();
}

/*
 * 初始化: 配置 PA4 为推挽输出片选(默认高), 读取 JEDEC ID 判断芯片。
 * W25Q32 的 ID 为 0xEF 0x40 0x16; 0x15/0x14 是更小的 W25Q16/W25Q08, 也兼容接受。
 */
uint8_t bl_w25q32_init(void)
{
    uint8_t  mf = 0U;
    uint8_t  id_hi = 0U;
    uint8_t  id_lo = 0U;

    /* PA4 = W25Q32 CS, 推挽输出, 默认高电平(未选中) */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL &= ~(GPIO_CRL_CNF4_Msk | GPIO_CRL_MODE4_Msk);
    GPIOA->CRL |= (0x02UL << GPIO_CRL_CNF4_Pos) | (0x01UL << GPIO_CRL_MODE4_Pos);
    W25Q_CS_HIGH();

    /* 读 JEDEC ID: 0x9F 后跟 3 个空时钟读回 3 字节 */
    (void)w25_busy_wait();
    W25Q_CS_LOW();
    (void)bl_spi_xfer(W25Q_CMD_READ_ID);
    mf = bl_spi_xfer(0xFFU);
    id_hi = bl_spi_xfer(0xFFU);
    id_lo = bl_spi_xfer(0xFFU);
    W25Q_CS_HIGH();

    /* W25Q32: 厂商 0xEF, 设备 0x4016 */
    return (mf == 0xEFU) && (id_hi == 0x40U) && ((id_lo == 0x16U) || (id_lo == 0x15U) || (id_lo == 0x14U));
}

/* 读取 len 字节: 发 0x03 + 24 位地址, 然后连续读(读操作不跨页限制) */
uint8_t bl_w25q32_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t i;

    if ((buf == NULL) || (addr + len > W25Q_CAPACITY))
    {
        return 0U;
    }
    if (!w25_busy_wait())
    {
        return 0U;
    }
    W25Q_CS_LOW();
    (void)bl_spi_xfer(W25Q_CMD_READ_DATA);
    (void)bl_spi_xfer((uint8_t)(addr >> 16));   /* 地址高 8 位 */
    (void)bl_spi_xfer((uint8_t)(addr >> 8));    /* 地址中 8 位 */
    (void)bl_spi_xfer((uint8_t)addr);           /* 地址低 8 位 */
    for (i = 0U; i < len; i++)
    {
        buf[i] = bl_spi_xfer(0xFFU);            /* 每发一个空字节收回一个数据字节 */
    }
    W25Q_CS_HIGH();
    return 1U;
}

/*
 * 页安全写入: 自动把数据按 256B 页边界拆段, 每段一页。
 * 不跨页是 W25Q 的硬性要求(否则会回绕覆盖页首), 所以这里必须逐段处理。
 */
uint8_t bl_w25q32_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t done = 0U;

    if ((buf == NULL) || (addr + len > W25Q_CAPACITY))
    {
        return 0U;
    }
    while (done < len)
    {
        uint32_t page_left = W25Q_PAGE_SIZE - ((addr + done) % W25Q_PAGE_SIZE);
        uint32_t chunk = len - done;
        uint32_t i;

        /* 本段长度 = min(剩余数据, 当前页剩余空间) */
        if (chunk > page_left)
        {
            chunk = page_left;
        }
        if (!w25_busy_wait())
        {
            return 0U;
        }
        w25_write_enable();                     /* 编程前必须写使能 */
        W25Q_CS_LOW();
        (void)bl_spi_xfer(W25Q_CMD_PAGE_PROGRAM);
        {
            uint32_t a = addr + done;
            (void)bl_spi_xfer((uint8_t)(a >> 16));
            (void)bl_spi_xfer((uint8_t)(a >> 8));
            (void)bl_spi_xfer((uint8_t)a);
        }
        for (i = 0U; i < chunk; i++)
        {
            (void)bl_spi_xfer(buf[done + i]);
        }
        W25Q_CS_HIGH();
        done += chunk;                          /* 芯片内部开始编程, 下次循环等忙 */
    }
    return 1U;
}

/* 擦除一个 4KB 扇区(addr 必须 4KB 对齐) */
uint8_t bl_w25q32_erase_sector(uint32_t addr)
{
    if ((addr % W25Q_SECTOR_SIZE) != 0U)
    {
        return 0U;
    }
    if (!w25_busy_wait())
    {
        return 0U;
    }
    w25_write_enable();
    W25Q_CS_LOW();
    (void)bl_spi_xfer(W25Q_CMD_SECTOR_ERASE);
    (void)bl_spi_xfer((uint8_t)(addr >> 16));
    (void)bl_spi_xfer((uint8_t)(addr >> 8));
    (void)bl_spi_xfer((uint8_t)addr);
    W25Q_CS_HIGH();
    return 1U;
}

/* 擦除覆盖 [addr, addr+len) 的所有扇区; 每擦一个扇区喂一次看门狗 */
uint8_t bl_w25q32_erase_range(uint32_t addr, uint32_t len)
{
    uint32_t end = addr + len;
    uint32_t a;

    if ((addr % W25Q_SECTOR_SIZE) != 0U)
    {
        return 0U;
    }
    for (a = addr; a < end; a += W25Q_SECTOR_SIZE)
    {
        if (!bl_w25q32_erase_sector(a))
        {
            return 0U;
        }
        IWDG->KR = 0xAAAAU;      /* 长时间擦除期间保持看门狗喂狗 */
    }
    return 1U;
}

/* 计算 W25Q32 上某段区域的 CRC-32(分页读取, 供镜像校验) */
uint32_t bl_w25q32_crc(uint32_t addr, uint32_t len)
{
    uint8_t  buf[W25Q_PAGE_SIZE];
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t done = 0U;
    uint32_t chunk;

    while (done < len)
    {
        chunk = len - done;
        if (chunk > W25Q_PAGE_SIZE)
        {
            chunk = W25Q_PAGE_SIZE;
        }
        if (!bl_w25q32_read(addr + done, buf, chunk))
        {
            return 0UL;
        }
        crc = bl_crc32_update(crc, buf, chunk);
        done += chunk;
    }
    return bl_crc32_final(crc);
}
