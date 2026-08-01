/*
 * ============================================================================
 * 文件: bl_spi.c
 * 功能: SPI1 主模式寄存器级驱动。
 *
 * 时序参数(与 App 工程的 MX_SPI1_Init 保持一致):
 *   - 主模式, 双向 2 线, 8 位数据;
 *   - CPOL=0(空闲时钟为低), CPHA=0(第一个边沿采样) —— W25Q 系列标准模式;
 *   - MSB 先行, NSS 由软件控制(SSM+SSI), 片选引脚 PA4 手动拉高/拉低;
 *   - 波特率分频 /4: PCLK2=72MHz => SPI 时钟 18MHz(在 W25Q32 25MHz 上限内)。
 *
 * W25Q32 片选时序约定: 拉低 CS 开始一次指令周期, 拉高 CS 结束。
 * 除"等待忙"状态寄存器轮询外, 每个指令都以 CS 低→高为一个完整事务。
 * ============================================================================
 */
#include "bl_spi.h"
#include "bl_config.h"
#include "stm32f1xx.h"

/*
 * 初始化 SPI1:
 *   RCC 打开 GPIOA 与 SPI1 时钟;
 *   PA5(SCK)/PA7(MOSI): CNF=10 复用推挽, MODE=11 50MHz;
 *   PA6(MISO)        : CNF=01 浮空输入;
 *   CR1 = MSTR(主模式) | SSM|SSI(软件NSS) | BR1(分频/4=18MHz) | SPE(使能)。
 */
void bl_spi_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_SPI1EN;

    /* PA5 SCK、PA7 MOSI: 复用推挽 50MHz; PA6 MISO: 浮空输入 */
    GPIOA->CRL &= ~(GPIO_CRL_CNF5_Msk | GPIO_CRL_MODE5_Msk |
                    GPIO_CRL_CNF6_Msk | GPIO_CRL_MODE6_Msk |
                    GPIO_CRL_CNF7_Msk | GPIO_CRL_MODE7_Msk);
    GPIOA->CRL |= (0x02UL << GPIO_CRL_CNF5_Pos) | (0x03UL << GPIO_CRL_MODE5_Pos);   /* SCK  */
    GPIOA->CRL |= (0x04UL << GPIO_CRL_CNF6_Pos);                                    /* MISO */
    GPIOA->CRL |= (0x02UL << GPIO_CRL_CNF7_Pos) | (0x03UL << GPIO_CRL_MODE7_Pos);   /* MOSI */

    /* 配置 SPI1 控制寄存器: 主模式 + 软件NSS + 分频/4 + 立即使能 */
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                (SPI_CR1_BR_1) |                       /* BR=010 => /4 = 18 MHz */
                SPI_CR1_SPE;
}

/*
 * 全双工收发一个字节:
 *   1. 等 TXE(发送缓冲空)后把数据写入 DR, SPI 硬件开始移位输出;
 *   2. 等 RXNE(接收缓冲非空), 即 8 个时钟完成、对方数据已收到;
 *   3. 读 DR 得到对方返回的字节。
 * 两个等待均有计数器保护, 防止总线异常时死循环。
 */
uint8_t bl_spi_xfer(uint8_t byte)
{
    uint32_t guard = 100000UL;

    while (((SPI1->SR & SPI_SR_TXE) == 0U) && (--guard != 0U))
    {
    }
    SPI1->DR = byte;

    guard = 100000UL;
    while (((SPI1->SR & SPI_SR_RXNE) == 0U) && (--guard != 0U))
    {
    }
    return (uint8_t)SPI1->DR;
}
