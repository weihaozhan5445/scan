/*
 * ============================================================================
 * 文件: stm32f1xx_it.c
 * 功能: Bootloader 的中断处理函数。
 *
 * 说明:
 *   - Bootloader 只用查询方式访问外设, 不依赖任何外设中断;
 *   - 故障类处理函数(硬错误/总线错误等)故意死循环:
 *     由于独立看门狗 IWDG 在运行(无论 App 还是 Bootloader 启动的),
 *     死循环会触发看门狗复位, 从而自动恢复;
 *   - startup 汇编里这些处理函数都是 WEAK 弱定义, 这里重写覆盖默认的
 *     空转循环, 目的是在硬错误时打印一条提示。
 * ============================================================================
 */
#include "stm32f1xx.h"
#include "bl_uart.h"

/* 不可屏蔽中断: 死循环等待看门狗复位 */
void NMI_Handler(void)
{
    for (;;)
    {
    }
}

/* 硬错误: 打印提示后死循环, 由看门狗复位恢复 */
void HardFault_Handler(void)
{
    bl_uart_write("\r\n[BL] HARD FAULT\r\n");
    for (;;)
    {
    }
}

/* 存储器管理错误: 死循环等看门狗复位 */
void MemManage_Handler(void)
{
    for (;;)
    {
    }
}

/* 总线错误: 死循环等看门狗复位 */
void BusFault_Handler(void)
{
    for (;;)
    {
    }
}

/* 用法错误: 死循环等看门狗复位 */
void UsageFault_Handler(void)
{
    for (;;)
    {
    }
}

/* 系统服务调用(SVC), Bootloader 未使用 */
void SVC_Handler(void)
{
}

/* 调试监视, 未使用 */
void DebugMon_Handler(void)
{
}

/* PendSV, Bootloader 未使用 */
void PendSV_Handler(void)
{
}

/* SysTick: Bootloader 用轮询方式计时, 不使能中断, 这里留空即可 */
void SysTick_Handler(void)
{
}
