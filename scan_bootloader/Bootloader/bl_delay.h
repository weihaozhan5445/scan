/*
 * 文件: bl_delay.h
 * 功能: Bootloader 毫秒级延时接口。
 * 说明: Bootloader 不使用 RTOS 也不依赖任何中断, 延时就基于 SysTick 轮询实现。
 */
#ifndef BL_DELAY_H
#define BL_DELAY_H

#include <stdint.h>

/* 初始化 SysTick 为 1ms 轮询时基(不使能中断) */
void bl_delay_init(void);

/* 阻塞延时 ms 毫秒 */
void bl_delay_ms(uint32_t ms);

#endif
