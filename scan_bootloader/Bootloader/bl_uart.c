/*
 * ============================================================================
 * 文件: bl_uart.c
 * 功能: USART1 查询式驱动, 用作 Bootloader 的调试/控制台串口。
 *
 * 引脚分配:
 *   PA9  = USART1_TX (复用推挽输出)
 *   PA10 = USART1_RX (浮空输入)
 *
 * 实现说明:
 *   - 波特率计算: BRR = 72MHz / 115200 = 625;
 *   - 发送: 轮询 TXE(发送数据寄存器空)再写 DR; 发完后轮询 TC(移位寄存器空),
 *     确保最后一个字节真正发完, 避免跳转/复位时被截断;
 *   - 接收: 轮询 RXNE(收到数据), 配合超时计数防止无限等待。
 * ============================================================================
 */
#include "bl_uart.h"
#include "bl_delay.h"
#include "bl_config.h"
#include "stm32f1xx.h"

/*
 * 初始化 USART1:
 *   RCC 打开 GPIOA 与 USART1 时钟;
 *   PA9  : CNF=10(复用推挽) MODE=11(50MHz)  —— TX
 *   PA10 : CNF=01(浮空输入)  MODE=00         —— RX
 *   BRR  = 72000000/115200 = 625(四舍五入后)
 *   CR1  = UE(使能) | TE(发送) | RE(接收)
 */
void bl_uart_init(void)
{
    uint32_t brr;

    /* 1. 打开 GPIOA 和 USART1 的外设时钟 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /* 2. 配置 PA9 为 USART1_TX: 复用推挽输出, 50MHz */
    GPIOA->CRH &= ~(GPIO_CRH_CNF9_Msk | GPIO_CRH_MODE9_Msk);
    GPIOA->CRH |= (0x02UL << GPIO_CRH_CNF9_Pos) | (0x03UL << GPIO_CRH_MODE9_Pos);
    /* 3. 配置 PA10 为 USART1_RX: 浮空输入 */
    GPIOA->CRH &= ~(GPIO_CRH_CNF10_Msk | GPIO_CRH_MODE10_Msk);
    GPIOA->CRH |= (0x04UL << GPIO_CRH_CNF10_Pos);

    /* 4. 计算并写入波特率分频值(带四舍五入) */
    brr = (SystemCoreClock + BL_UART_BAUD / 2U) / BL_UART_BAUD;
    USART1->BRR = brr;

    /* 5. 使能 USART、发送器、接收器 */
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

/*
 * 发送一个字符。
 * 先等 TXE(发送数据寄存器空, 即 DR 可写), 写入后:
 * 再等 TC(发送完成, 移位寄存器也空了), 确保字节已从引脚发完。
 * 两处等待都有计数器保护, 即使外设异常也不会死循环。
 */
void bl_uart_putc(char c)
{
    uint32_t guard = 1000000UL;

    /* 等 TXE=1: 发送数据寄存器为空, 可以写入新数据 */
    while (((USART1->SR & USART_SR_TXE) == 0U) && (--guard != 0U))
    {
    }
    USART1->DR = (uint8_t)c;

    /* 等 TC=1: 当前字节(含移位寄存器)已全部发出。
     * 跳转 App 或复位前必须等这一步, 否则最后一个字符会被截断。 */
    guard = 1000000UL;
    while (((USART1->SR & USART_SR_TC) == 0U) && (--guard != 0U))
    {
    }
}

/* 发送以 '\0' 结尾的字符串 */
void bl_uart_write(const char *s)
{
    while ((s != NULL) && (*s != '\0'))
    {
        bl_uart_putc(*s++);
    }
}

/*
 * 带超时接收一个字符。
 * 每轮先查 RXNE(接收数据寄存器非空), 有数据就读 DR 返回 1;
 * 否则延时 1ms 再查, 直到超时返回 0。
 * 返回值: 1=收到一个字符(存入 *c), 0=超时无数据。
 */
uint8_t bl_uart_getc(uint32_t timeout_ms, char *c)
{
    uint32_t elapsed = 0U;

    if (c == NULL)
    {
        return 0U;
    }
    while (elapsed < timeout_ms)
    {
        if ((USART1->SR & USART_SR_RXNE) != 0U)
        {
            *c = (char)(USART1->DR & 0xFFU);   /* 读 DR 会自动清除 RXNE */
            return 1U;
        }
        bl_delay_ms(1U);
        elapsed++;
    }
    return 0U;
}

/* 十六进制查表字符 */
static const char hex_digit[] = "0123456789ABCDEF";

/* 以 "0xXXXXXXXX" 格式打印 32 位整数(调试时打印地址/CRC 用) */
void bl_uart_write_hex32(uint32_t v)
{
    char buf[11];
    int i;

    buf[0] = '0';
    buf[1] = 'x';
    /* 从最高 4 位到最低 4 位依次取一个十六进制字符 */
    for (i = 0; i < 8; i++)
    {
        buf[2 + i] = hex_digit[(v >> (28 - 4 * i)) & 0x0FUL];
    }
    buf[10] = '\0';
    bl_uart_write(buf);
}
