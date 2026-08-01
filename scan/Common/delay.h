/*
 * 文件: delay.h
 * 功能: 微秒级延时接口(实现见 delay.c, 基于 DWT 周期计数器)。
 */
#ifndef __DELAY_H
#define __DELAY_H

#include <stdint.h>

/* 阻塞延时 nus 微秒(FreeRTOS 运行时可安全使用) */
void delay_us(uint32_t nus);

#endif
