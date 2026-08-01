/*
 * ============================================================================
 * 文件: bl_iap.c
 * 功能: App 有效性校验 + 跳转。
 *
 * App 有效性判断依据(Cortex-M3 向量表规则):
 *   向量表第 1 个字 = 初始 MSP(必须落在 20KB SRAM 内);
 *   向量表第 2 个字 = 复位向量(必须落在 App 区且为 Thumb 地址, 最低位=1);
 *   前 16 个字不能全为 0xFF(全 0xFF 说明 Flash 是空白/未烧写)。
 *
 * 跳转步骤(顺序很重要):
 *   1. 关全局中断(__disable_irq);
 *   2. 停 SysTick, 清空 NVIC 使能与挂起;
 *   3. 复位 Bootloader 用过的外设, 让 App 从干净状态初始化;
 *   4. 重定位向量表 SCB->VTOR = APP_BASE;
 *   5. 把主栈指针切到 App 的初始 MSP, 再调用 App 复位向量(不返回)。
 * ============================================================================
 */
#include "bl_iap.h"
#include "bl_config.h"
#include "bl_uart.h"
#include "stm32f1xx.h"

/*
 * 校验 App 镜像是否有效。返回 1=有效(可以跳转), 0=无效/空白。
 */
uint8_t bl_app_is_valid(void)
{
    const uint32_t *vt = (const uint32_t *)APP_BASE;   /* 指向 App 向量表 */
    uint32_t sp;
    uint32_t pc;
    uint32_t i;
    uint32_t nonzero = 0U;

    /* 1. 空白 Flash 读出全 0xFFFFFFFF; 若前 16 个字全为空白则视为无 App */
    for (i = 0U; i < 16U; i++)
    {
        if (vt[i] != 0xFFFFFFFFUL)
        {
            nonzero++;
        }
    }
    if (nonzero == 0U)
    {
        return 0U;
    }

    sp = vt[0];   /* 初始主栈指针 */
    pc = vt[1];   /* 复位向量地址 */

    /* 2. 初始 SP 必须落在 20KB SRAM(0x20000000 - 0x20004FFF)内 */
    if ((sp < 0x20000000UL) || (sp >= 0x20000000UL + 0x5000UL))
    {
        return 0U;
    }
    /* 3. 复位向量必须是 Thumb 地址(最低位=1) */
    if ((pc & 0x1UL) == 0UL)
    {
        return 0U;
    }
    /* 4. 复位向量必须落在 App Flash 区内 */
    if ((pc < APP_BASE) || (pc >= APP_END))
    {
        return 0U;
    }
    return 1U;
}

/*
 * 跳转到 App。此函数设计为"永不返回"。
 */
void bl_jump_to_app(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_BASE;        /* App 初始 MSP   */
    uint32_t pc = *(volatile uint32_t *)(APP_BASE + 4U); /* App 复位向量   */
    void (*app_reset)(void) = (void (*)(void))pc;

    bl_uart_write("\r\n[BL] jump to app @0x08004000\r\n");

    /* 1. 关闭全局中断, 跳转期间不允许被打断 */
    __disable_irq();

    /* 2. 停止并清零 SysTick(其配置由 App 重新建立) */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    /* 3. 关闭所有 NVIC 中断并使能位, 清空挂起位 */
    NVIC->ICER[0] = 0xFFFFFFFFUL;
    NVIC->ICPR[0] = 0xFFFFFFFFUL;

    /* 4. 复位 Bootloader 用过的外设(GPIO/SPI/I2C/USART/ADC/DMA 等),
     *    确保 App 初始化时寄存器是干净状态 */
    RCC->APB2RSTR = 0xFFFFFFFFUL;
    RCC->APB2RSTR = 0x00000000UL;
    RCC->APB1RSTR = 0xFFFFFFFFUL;
    RCC->APB1RSTR = 0x00000000UL;

    /* 5. 重定位向量表到 App 起始地址(STM32F1 的 Cortex-M3 支持 VTOR) */
    SCB->VTOR = APP_BASE;

    /* 6. 切换主栈指针为 App 的初始 MSP, 然后跳转到 App 复位向量 */
    __set_MSP(sp);
    app_reset();

    /* 正常不会执行到这里; 万一 App 复位向量返回, 死循环兜底 */
    for (;;)
    {
    }
}
