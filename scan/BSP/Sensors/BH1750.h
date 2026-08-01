/*
 * 文件: BH1750.h
 * 功能: BH1750 光照传感器接口(I2C1)。
 */
#ifndef __BH1750_H
#define __BH1750_H

#include <stdint.h>

void BH1750_init(void);              /* 初始化为连续高分辨率模式 */
uint16_t BH1750_Readlight(void);     /* 读取光照强度(单位 lx) */

#endif
