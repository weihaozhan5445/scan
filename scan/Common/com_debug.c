/*
 * 文件: com_debug.c
 * 功能: printf 重定向 —— 把标准库 printf 输出到 USART1(ESP8266/调试串口)。
 * 说明: 重写 fputc 即可让 printf/串口调试信息走串口打印。
 */
#include "com_debug.h"

/* 标准库 printf 的底层输出函数: 每个字符通过 USART1 阻塞发送 */
int fputc(int ch, FILE *f)
{
    (void)f;
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xffff);
    return ch;
}
