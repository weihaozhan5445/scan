/*
 * ============================================================================
 * 文件: BH1750.c
 * 功能: BH1750 数字光照强度传感器驱动(I2C1)。
 * 接线: SDA=PB7, SCL=PB6, ADDR 接地时器件地址 0x46(8 位形式)。
 * 量程: 高分辨率模式 1lx 分辨率, 量程约 0-65535lx(除以 1.2 为校准系数)。
 * ============================================================================
 */
#include "stm32f1xx_hal.h"
#include "BH1750.h"

extern I2C_HandleTypeDef hi2c1;

#define BH_ADDR 0x46                 /* BH1750 I2C 器件地址(8位写地址) */

/*
 * 初始化: 发送 0x10 = 连续高分辨率测量模式(1lx 分辨率)。
 * 上电默认是掉电模式, 发完命令后需等待内部测量(约 180ms)再读。
 */
void BH1750_init(void)
{
    uint8_t cmd = 0x10;
    HAL_I2C_Master_Transmit(&hi2c1, BH_ADDR, &cmd, 1, 100);
    HAL_Delay(180);
}

/*
 * 读取当前光照强度(单位 lx)。
 * 高分辨率模式下测量值为 16 位: 高字节在前, 拼接后除以 1.2 得到 lx。
 */
uint16_t BH1750_Readlight(void)
{
    uint8_t dat[2];

    HAL_I2C_Master_Receive(&hi2c1, BH_ADDR, dat, 2, 100);
    return (uint16_t)((float)(((uint16_t)dat[0] << 8) | dat[1]) / 1.2f);
}
