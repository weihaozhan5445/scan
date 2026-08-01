#ifndef __OLED_H
#define __OLED_H


#include "OLED_DATA.h"
#include <stdint.h>
#include <stdio.h>


#define OLED_ADDR 0x78

void OLED_WR_Byte(uint8_t dat,uint8_t cmd);
void OLED_Clear(void);
void OLED_SetPos(uint8_t x,uint8_t y);
void OLED_ShowString(uint8_t x,uint8_t y,char *str);
void OLED_ShowNum(uint8_t x,uint8_t y,uint32_t num,uint8_t len);
void OLED_Init(void);

#endif
