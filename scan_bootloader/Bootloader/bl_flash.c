/*
 * ============================================================================
 * 文件: bl_flash.c
 * 功能: 内部 Flash 驱动 —— 只允许操作 App 区域。
 *
 * STM32F103(中容量)Flash 特性:
 *   - 扇区大小 1KB;
 *   - 编程宽度为 16 位半字(32 位字 = 连续写两个半字, 与 HAL 行为一致);
 *   - 编程前必须先擦除(擦除后所有位为 1);
 *   - 操作流程: 解锁(KEYR) → 置 PER/PG → 操作 → 等 BSY=0 → 清标志 → 清 PER/PG。
 *
 * 安全: bl_flash_erase_range / bl_flash_write 都会检查地址是否落在
 *       [APP_BASE, APP_END) 内, 防止越界破坏 Bootloader。
 * ============================================================================
 */
#include "bl_flash.h"
#include "bl_crc32.h"
#include "bl_config.h"
#include "stm32f1xx.h"

/* 等待 Flash 忙标志清除(擦除/编程期间 BSY=1) */
static void flash_wait_busy(void)
{
    while ((FLASH->SR & FLASH_SR_BSY) != 0U)
    {
    }
}

/*
 * 解锁 Flash: 依次写入两个密钥, 使 FLASH_CR 可写。
 * 同时清掉历史错误标志(EOP/PGERR/WRPRTERR)。
 */
void bl_flash_init(void)
{
    FLASH->KEYR = 0x45670123UL;
    FLASH->KEYR = 0xCDEF89ABUL;
    /* 清除所有标志位(写 1 清零) */
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
}

/*
 * 按 1KB 扇区擦除 [addr, addr+len)。
 * 每个扇区: 置 PER → 写扇区地址 AR → 置 STRT 启动 → 等 BSY → 清标志 → 清 PER。
 * 擦除 48 个扇区约需 2~3 秒, 循环内喂一次看门狗, 防止超时复位。
 */
uint8_t bl_flash_erase_range(uint32_t addr, uint32_t len)
{
    uint32_t end = addr + len;
    uint32_t a;

    /* 安全校验: 起始地址必须在 App 区且 1KB 对齐, 结束不越过 App 区末尾 */
    if ((addr < APP_BASE) || (end > APP_END) || ((addr & 0x3FFU) != 0U))
    {
        return 0U;
    }
    for (a = addr; a < end; a += 0x400UL)
    {
        flash_wait_busy();
        FLASH->CR |= FLASH_CR_PER;              /* 选择"扇区擦除"模式 */
        FLASH->AR = a;                          /* 要擦除的扇区地址 */
        FLASH->CR |= FLASH_CR_STRT;             /* 启动擦除 */
        flash_wait_busy();
        FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;  /* 清标志 */
        FLASH->CR &= ~FLASH_CR_PER;
        /* 擦除出错(编程错误/写保护)立即返回失败 */
        if ((FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0U)
        {
            return 0U;
        }
        /* 48 个 1KB 扇区擦除约需 2-3 秒, 逐个喂狗防止看门狗复位 */
        IWDG->KR = 0xAAAAU;
    }
    return 1U;
}

/*
 * 编程一个半字: 置 PG → 写半字 → 等 BSY → 清标志 → 清 PG。
 * 返回 1=成功, 0=出现编程错误或写保护错误。
 */
static uint8_t flash_program_halfword(uint32_t addr, uint16_t data)
{
    flash_wait_busy();
    FLASH->CR |= FLASH_CR_PG;                   /* 选择"编程"模式 */
    *(volatile uint16_t *)addr = data;          /* 写入半字(触发编程) */
    flash_wait_busy();
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;     /* 清标志 */
    FLASH->CR &= ~FLASH_CR_PG;
    return ((FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) == 0U) ? 1U : 0U;
}

/*
 * 写数据: 要求 addr 与 len 都是 4 的倍数。
 * 每个 32 位字拆成"低半字 + 高半字"两次编程(与 HAL 的 FLASH_Program_32 一致)。
 */
uint8_t bl_flash_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    /* 安全与对齐校验 */
    if ((buf == NULL) || (addr < APP_BASE) || (addr + len > APP_END) ||
        ((addr & 0x3U) != 0U) || ((len & 0x3U) != 0U))
    {
        return 0U;
    }
    for (i = 0U; i < len; i += 4U)
    {
        uint32_t w;

        /* 拼出 32 位字(小端: buf[0] 是低字节) */
        w  = (uint32_t)buf[i];
        w |= (uint32_t)buf[i + 1U] << 8;
        w |= (uint32_t)buf[i + 2U] << 16;
        w |= (uint32_t)buf[i + 3U] << 24;
        /* 先写低半字, 再写高半字 */
        if (!flash_program_halfword(addr + i, (uint16_t)(w & 0xFFFFU)))
        {
            return 0U;
        }
        if (!flash_program_halfword(addr + i + 2U, (uint16_t)(w >> 16)))
        {
            return 0U;
        }
    }
    return 1U;
}

/* 逐字节比对: Flash 内容是否与 buf 完全一致(烧写后验证用) */
uint8_t bl_flash_verify(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    if ((buf == NULL) || (addr + len > APP_END))
    {
        return 0U;
    }
    for (i = 0U; i < len; i++)
    {
        if (*(volatile uint8_t *)(addr + i) != buf[i])
        {
            return 0U;
        }
    }
    return 1U;
}

/*
 * 计算 Flash 上某段区域的 CRC-32(整体校验用)。
 * 逐 4 字节读取并累加进 CRC, 可检测"逐块比对"漏掉的一致性错误。
 */
uint8_t bl_flash_crc(uint32_t addr, uint32_t len, uint32_t *out)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i;

    if ((out == NULL) || (addr + len > APP_END))
    {
        return 0U;
    }
    for (i = 0U; i < len; i += 4U)
    {
        uint8_t b[4];

        b[0] = *(volatile uint8_t *)(addr + i);
        b[1] = *(volatile uint8_t *)(addr + i + 1U);
        b[2] = *(volatile uint8_t *)(addr + i + 2U);
        b[3] = *(volatile uint8_t *)(addr + i + 3U);
        crc = bl_crc32_update(crc, b, 4U);
    }
    *out = bl_crc32_final(crc);
    return 1U;
}
