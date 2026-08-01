/*
 * 文件: bl_spi.h
 * 功能: Bootloader 的 SPI1 主模式驱动(用于读写 W25Q32 外部 Flash)。
 * 引脚: PA5=SCK, PA6=MISO, PA7=MOSI, PA4=W25Q32 片选(软件控制)。
 */
#ifndef BL_SPI_H
#define BL_SPI_H

#include <stdint.h>

/* 初始化 SPI1: 主模式、8位、CPOL=0/CPHA=0、MSB先行、软件NSS */
void     bl_spi_init(void);

/* 全双工收发一个字节(发送 byte, 返回同时收到的字节) */
uint8_t  bl_spi_xfer(uint8_t byte);

#endif
