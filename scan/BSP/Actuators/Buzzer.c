/*
 * 文件: Buzzer.c
 * 功能: 蜂鸣器控制。
 * 引脚: PB0(BEER_CTRL), 开漏输出。
 * 注意: 开漏输出时"拉低=导通(响)"、"拉高/释放=截止(静)",
 *       若实际接的是高电平驱动蜂鸣器, 只需对调 ON/OFF 的引脚电平。
 */
#include "BUZZER.h"
#include "stm32f1xx_hal.h"

/* 打开蜂鸣器(按当前接线约定为输出高电平) */
void BUZZER_ON(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
}

/* 关闭蜂鸣器 */
void BUZZER_OFF(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}
