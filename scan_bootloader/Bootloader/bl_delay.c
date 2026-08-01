/*
 * ============================================================================
 * 文件: bl_delay.c
 * 功能: 基于 SysTick 的毫秒延时。
 *
 * 实现原理:
 *   SysTick 是一个 24 位递减计数器。把它配置为"每 1ms 从 LOAD 值倒数到 0",
 *   每倒完一次 COUNTFLAG 位置 1。延时就是"等待 COUNTFLAG 置位 ms 次"。
 *
 * 注意:
 *   1) 这里刻意【不使能】SysTick 中断, 因为 Bootloader 不需要任何定时中断;
 *   2) 看门狗 IWDG 是独立于 SysTick 的硬件, 延时期间需要喂狗由调用方负责。
 * ============================================================================
 */
#include "bl_delay.h"
#include "stm32f1xx.h"

/*
 * 初始化 SysTick:
 *   LOAD  = 72MHz/1000 - 1 = 71999  → 每 71999+1=72000 个时钟周期 = 1ms 溢出一次
 *   VAL   = 0                        → 清零当前计数值
 *   CTRL  = CLKSOURCE(内核时钟72MHz) | ENABLE(启动计数), 不开 TICKINT 中断
 */
void bl_delay_init(void)
{
    SysTick->LOAD = (SystemCoreClock / 1000U) - 1U;
    SysTick->VAL  = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

/*
 * 阻塞延时 ms 毫秒。
 * 每次循环先读 CTRL(该操作会清除 COUNTFLAG 标志), 再等 COUNTFLAG 重新置位,
 * 即"等满 1ms"后把剩余时间减 1, 如此重复 ms 次。
 */
void bl_delay_ms(uint32_t ms)
{
    while (ms > 0U)
    {
        /* 读取 CTRL 会硬件自动清除 COUNTFLAG, 为下一次判断做准备 */
        (void)SysTick->CTRL;
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U)
        {
        }
        ms--;
    }
}
