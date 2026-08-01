/*
 * ============================================================================
 * 文件: bl_i2c.c
 * 功能: I2C2 主模式寄存器级驱动(100kHz), 服务对象是 W24C02 EEPROM。
 *
 * STM32F1 的 I2C 外设事件模型(参考参考手册 RM0008):
 *   EV5 : SB 置位 —— 起始条件已发出
 *   EV6 : ADDR 置位 —— 从机地址已发出并收到应答(读 SR1 再读 SR2 清除)
 *   EV7 : RXNE 置位 —— 收到一个字节, 可读 DR
 *   EV8 : TXE 置位 —— DR 空, 可写下一个字节
 *   EV8_2: BTF 置位 —— 发送完成(DR 与移位寄存器都空)
 *
 * 健壮性设计:
 *   - 所有事件等待都带超时(50ms), 超时或出现 BERR/ARLO/AF/OVR 错误即返回失败;
 *   - 出错后对 I2C 外设做软复位(SWRST), 保证总线状态不会被"卡死"的从机拖住;
 *   - 寄存器寻址读: 先"伪写"设置内部地址, 再用重复起始位切换为读。
 * ============================================================================
 */
#include "bl_i2c.h"
#include "bl_delay.h"
#include "stm32f1xx.h"

/* 每个 I2C 事件的等待超时(毫秒) */
#define I2C_TIMEOUT_MS  50U

/*
 * 等待 SR1 中某个标志位置位。
 * clear_sr2=1 时额外读一次 SR2(读 SR2 会清除 ADDR 标志)。
 * 返回 1=等待成功; 0=超时或总线错误。
 */
static uint8_t i2c_wait_flag(uint32_t flag, uint8_t clear_sr2)
{
    uint32_t t = 0U;

    while (t < I2C_TIMEOUT_MS)
    {
        if ((I2C2->SR1 & flag) != 0U)
        {
            if (clear_sr2 != 0U)
            {
                (void)I2C2->SR2;      /* 读 SR2 清除 ADDR, 完成 EV6 处理 */
            }
            return 1U;
        }
        /* 出现任何错误标志(总线错误/仲裁丢失/应答失败/溢出)立即判失败 */
        if ((I2C2->SR1 & (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR)) != 0U)
        {
            return 0U;
        }
        bl_delay_ms(1U);
        t++;
    }
    return 0U;
}

/*
 * 软复位 I2C 外设: 置 SWRST 再清除, 最后重新使能 PE。
 * 用于错误恢复, 保证外设状态机回到空闲。
 */
static void i2c_reset(void)
{
    I2C2->CR1 = 0U;
    I2C2->CR1 = I2C_CR1_SWRST;
    I2C2->CR1 = 0U;
    I2C2->CR1 = I2C_CR1_PE;
}

/* 发送起始条件, 等待 EV5(SB=1) */
static uint8_t i2c_start(void)
{
    I2C2->CR1 |= I2C_CR1_START;
    return i2c_wait_flag(I2C_SR1_SB, 0U);
}

/* 发送停止条件(硬件自动完成) */
static void i2c_stop(void)
{
    I2C2->CR1 |= I2C_CR1_STOP;
}

/*
 * 初始化 I2C2:
 *   RCC 打开 GPIOB 与 I2C2 时钟, 先对 I2C2 复位得到干净状态;
 *   PB10/PB11: CNF=11 复用开漏, MODE=11 50MHz(I2C 引脚必须是开漏);
 *   CR2.FREQ  = 36  (PCLK1=36MHz);
 *   CCR       = 180 (36MHz / (2*100kHz) => 100kHz);
 *   TRISE     = 37  (1000ns / 27.8ns 上升时间换算);
 *   最后使能 PE。
 */
void bl_i2c_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C2EN;

    /* 复位 I2C2 外设, 清除上电残留状态 */
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C2RST;

    /* PB10 = SCL, PB11 = SDA: 复用开漏输出 */
    GPIOB->CRH &= ~(GPIO_CRH_CNF10_Msk | GPIO_CRH_MODE10_Msk |
                    GPIO_CRH_CNF11_Msk | GPIO_CRH_MODE11_Msk);
    GPIOB->CRH |= (0x03UL << GPIO_CRH_CNF10_Pos) | (0x03UL << GPIO_CRH_MODE10_Pos);
    GPIOB->CRH |= (0x03UL << GPIO_CRH_CNF11_Pos) | (0x03UL << GPIO_CRH_MODE11_Pos);

    /* 配置时钟频率/时序参数并使能 */
    I2C2->CR1 = 0U;
    I2C2->CR2 = 36U;                          /* FREQ = PCLK1(36MHz)/1MHz    */
    I2C2->CCR = 180U;                         /* 100kHz: 36M/(2*100k)=180    */
    I2C2->TRISE = 37U;                        /* 1000ns / tPCLK1             */
    I2C2->CR1 = I2C_CR1_PE;
}

/*
 * 不带寄存器地址的连续写(发送方向):
 *   起始 → 写从机地址(WR) → EV6 → 逐字节写(每字节等 TXE) → BTF → 停止。
 */
uint8_t bl_i2c_write(uint8_t dev_addr, const uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if (!i2c_start())
    {
        i2c_reset();
        return 0U;
    }
    /* 写方向: 地址最低位为 0 */
    I2C2->DR = dev_addr & 0xFEU;
    if (!i2c_wait_flag(I2C_SR1_ADDR, 1U))       /* EV6: 从机已应答地址 */
    {
        i2c_reset();
        return 0U;
    }
    for (i = 0U; i < len; i++)
    {
        if (!i2c_wait_flag(I2C_SR1_TXE, 0U))    /* EV8: DR 空, 可写 */
        {
            i2c_reset();
            return 0U;
        }
        I2C2->DR = buf[i];
    }
    if (!i2c_wait_flag(I2C_SR1_BTF, 0U))        /* EV8_2: 全部发完 */
    {
        i2c_reset();
        return 0U;
    }
    i2c_stop();
    return 1U;
}

/*
 * 寄存器寻址写(EEPROM 写):
 *   起始 → 从机地址(WR) → EV6 → 写内部寄存器地址 → 写数据(逐字节等TXE) → 停止。
 */
uint8_t bl_i2c_mem_write(uint8_t dev_addr, uint8_t mem_addr, const uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if (!i2c_start())
    {
        i2c_reset();
        return 0U;
    }
    I2C2->DR = dev_addr & 0xFEU;                /* 写方向 */
    if (!i2c_wait_flag(I2C_SR1_ADDR, 1U))
    {
        i2c_reset();
        return 0U;
    }
    /* 先写入内部寄存器/内存地址(EEPROM 的字节地址) */
    if (!i2c_wait_flag(I2C_SR1_TXE, 0U))
    {
        i2c_reset();
        return 0U;
    }
    I2C2->DR = mem_addr;
    /* 再逐个写入数据字节 */
    for (i = 0U; i < len; i++)
    {
        if (!i2c_wait_flag(I2C_SR1_TXE, 0U))
        {
            i2c_reset();
            return 0U;
        }
        I2C2->DR = buf[i];
    }
    if (!i2c_wait_flag(I2C_SR1_BTF, 0U))
    {
        i2c_reset();
        return 0U;
    }
    i2c_stop();
    return 1U;
}

/*
 * 寄存器寻址读(EEPROM 读):
 *   阶段1(伪写): 起始 → 从机地址(WR) → EV6 → 写内部地址 → BTF;
 *   阶段2(读)  : 重复起始 → 从机地址(RD) → EV6 → 按字节数决定 ACK/STOP 时序。
 *
 * 接收字节数不同, 主收模式时序不同(参考手册"主接收器"章节):
 *   len==1 : 清 ADDR 后禁止 ACK、置 STOP, 再收 1 字节;
 *   len>1  : 先收 len-2 字节(保持 ACK), 倒数第 2 字节前禁止 ACK,
 *            读走倒数第 2 字节后置 STOP, 再读最后 1 字节。
 */
uint8_t bl_i2c_mem_read(uint8_t dev_addr, uint8_t mem_addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if (buf == NULL)
    {
        return 0U;
    }

    /* ---------- 阶段1: 伪写, 把内部地址指针指到 mem_addr ---------- */
    if (!i2c_start())
    {
        i2c_reset();
        return 0U;
    }
    I2C2->DR = dev_addr & 0xFEU;
    if (!i2c_wait_flag(I2C_SR1_ADDR, 1U))
    {
        i2c_reset();
        return 0U;
    }
    if (!i2c_wait_flag(I2C_SR1_TXE, 0U))
    {
        i2c_reset();
        return 0U;
    }
    I2C2->DR = mem_addr;
    /* 等 BTF: 地址字节真正移出后, 才能发起重复起始 */
    if (!i2c_wait_flag(I2C_SR1_BTF, 0U))
    {
        i2c_reset();
        return 0U;
    }

    /* ---------- 阶段2: 重复起始 + 读方向 ---------- */
    I2C2->CR1 |= I2C_CR1_START;                 /* 重复起始条件 */
    if (!i2c_wait_flag(I2C_SR1_SB, 0U))
    {
        i2c_reset();
        return 0U;
    }
    I2C2->DR = dev_addr | 0x01U;                /* 读方向: 地址最低位为 1 */
    if (!i2c_wait_flag(I2C_SR1_ADDR, 1U))       /* EV6: 从机应答 */
    {
        i2c_reset();
        return 0U;
    }

    /* ---------- 按字节数分支处理 ACK/STOP 时序 ---------- */
    if (len == 1U)
    {
        /* 只读 1 字节: 禁止 ACK + 置 STOP, 然后读走该字节 */
        I2C2->CR1 &= ~I2C_CR1_ACK;              /* 对最后字节回 NACK */
        i2c_stop();
        if (!i2c_wait_flag(I2C_SR1_RXNE, 0U))
        {
            i2c_reset();
            return 0U;
        }
        buf[0] = (uint8_t)I2C2->DR;
    }
    else
    {
        /* 先收 len-2 字节, 期间保持 ACK */
        for (i = 0U; i < len - 2U; i++)
        {
            if (!i2c_wait_flag(I2C_SR1_RXNE, 0U))
            {
                i2c_reset();
                return 0U;
            }
            buf[i] = (uint8_t)I2C2->DR;
        }
        /* 倒数第 2 字节前: 禁止 ACK */
        I2C2->CR1 &= ~I2C_CR1_ACK;
        if (!i2c_wait_flag(I2C_SR1_RXNE, 0U))
        {
            i2c_reset();
            return 0U;
        }
        buf[i++] = (uint8_t)I2C2->DR;
        /* 读走倒数第 2 字节后立即置 STOP */
        i2c_stop();
        /* 最后一个字节: 从机发完即释放总线, 读走即可 */
        if (!i2c_wait_flag(I2C_SR1_RXNE, 0U))
        {
            i2c_reset();
            return 0U;
        }
        buf[i] = (uint8_t)I2C2->DR;
    }

    /* 重新使能 ACK, 供下一次传输使用 */
    I2C2->CR1 |= I2C_CR1_ACK;
    return 1U;
}
