/*
 * 文件: com_debug.h
 * 功能: 串口调试打印 —— printf 重定向到 USART1 + 带 文件名/行号 的调试宏。
 */
#ifndef __COM_DEBUG_H
#define __COM_DEBUG_H

#include "usart.h"
#include "stdio.h"
#include "stdarg.h"

/* 日志打印总开关: 1=使能, 0=全部关闭(省资源) */
#define DEBUG_ENABLE 1
#ifdef DEBUG_ENABLE

/* 带 文件:行号 前缀的调试打印, 例如: debug_printf("val=%d", x); */
#define debug_printf(format,...) printf("[%s:%d]" format "\r\n",__FILE__,__LINE__, ##__VA_ARGS__)

#else
#define debug_printf(...)
#endif

#endif
