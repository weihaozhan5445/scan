/*
 * 文件: delay.c
 * 功能: 微秒级延时(供 DHT11 等需要精确时序的外设使用)。
 */
#include "delay.h"
#include "stm32f1xx_hal.h"

/*
 * 基于 DWT(数据观察点与跟踪单元)周期计数器的微秒延时。
 *
 * 为什么不用 SysTick:
 *   FreeRTOS 运行后 SysTick 被 RTOS 占用(1ms tick), 若再用它做微秒延时
 *   会与调度器冲突; DWT->CYCCNT 是独立的内核周期计数器, 不受影响。
 *
 * 实现: 使能 CYCCNT → 读起始值 → 忙等 (当前值-起始值) >= nus*72 个周期。
 * 用无符号减法处理 32 位回绕, 最长可延时约 59 秒。
 */
void delay_us(uint32_t nus)
{
    uint32_t start;
    uint32_t ticks;

    if (nus == 0U)
    {
        return;
    }

    /* 1. 打开 DWT: TRCENA 必须先置位才能访问 CYCCNT */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;          /* 使能周期计数器 */

    /* 2. 换算目标周期数(72MHz => 每微秒 72 个周期)并忙等 */
    start = DWT->CYCCNT;
    ticks = nus * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks)
    {
    }
}
