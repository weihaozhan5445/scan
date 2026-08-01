/*
 * 文件: OLED_DATA.h
 * 功能: SSD1306 8x16 ASCII 字库(F8X16)的外部声明。
 * 说明: 字库内容见 OLED_DATA.c, 共 95 个可打印 ASCII 字符(0x20-0x7E),
 *       每个字符 16 字节(上半页 8 列 + 下半页 8 列, LSB=页内最上行)。
 */
#ifndef __FONT_H
#define __FONT_H

extern const unsigned char F8X16[];

#endif
