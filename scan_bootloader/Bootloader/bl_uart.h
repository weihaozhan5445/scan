/*
 * 文件: bl_uart.h
 * 功能: Bootloader 调试串口(USART1)接口。
 * 引脚: PA9=TX, PA10=RX, 115200 8N1。
 * 说明: 全部使用查询方式(阻塞发送/带超时接收), 不依赖中断。
 */
#ifndef BL_UART_H
#define BL_UART_H

#include <stdint.h>

/* 初始化 USART1(115200 8N1) */
void     bl_uart_init(void);

/* 发送一个字符(带超时保护) */
void     bl_uart_putc(char c);

/* 发送字符串(遇到 '\0' 结束) */
void     bl_uart_write(const char *s);

/* 带超时接收一个字符: 1=成功收到, 0=超时 */
uint8_t  bl_uart_getc(uint32_t timeout_ms, char *c);

/* 以十六进制打印一个 32 位整数(带 "0x" 前缀), 用于调试输出地址/CRC */
void     bl_uart_write_hex32(uint32_t v);

#endif
