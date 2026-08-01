/*
 * ============================================================================
 * 文件: OLED.h
 * 功能: 0.96 寸 SSD1306 OLED(I2C) 驱动接口。
 * 接线: I2C1(SDA=PB7, SCL=PB6), 器件地址 0x78(7 位地址 0x3C)。
 * 坐标: x=列(0-127), y=页(0-7); 8x16 字符每行占 2 页, 行距用 y+2。
 * ============================================================================
 */
#ifndef __OLED_H
#define __OLED_H

#include "OLED_DATA.h"
#include <stdint.h>
#include <stdio.h>

#define OLED_ADDR 0x78                     /* I2C 器件地址(8位形式) */

void OLED_WR_Byte(uint8_t dat, uint8_t cmd);            /* 写命令(0)/数据(1) */
void OLED_Clear(void);                                   /* 清屏 */
void OLED_SetPos(uint8_t x, uint8_t y);                  /* 定位到 列x/页y */
void OLED_ShowString(uint8_t x, uint8_t y, char *str);   /* 显示字符串 */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len); /* 显示数字 */
void OLED_Init(void);                                    /* 初始化 */

#endif
