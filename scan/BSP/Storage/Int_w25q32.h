/*
 * ============================================================================
 * 文件: Int_w25q32.h
 * 功能: W25Q32(4MB SPI NOR Flash)驱动 —— 基于 HAL SPI1。
 * 用途: OTA 固件的暂存区与备份区存储。
 *
 * W25Q32 要点:
 *   - 扇区 4KB、页 256B; 写之前必须先擦除;
 *   - 页编程一次最多 256B 且不能跨页(跨页会回绕);
 *   - 片选低电平选中芯片, 高电平释放; 平时片选保持高。
 * ============================================================================
 */
#ifndef __INT_W25Q32__
#define __INT_W25Q32__

#include "spi.h"

/* ---- W25Q32 常用指令 ---- */
#define W25Q32_READ_ID 0x9F         /* 读 JEDEC ID                    */
#define W25Q32_READ_STATUS_REG 0x05 /* 读状态寄存器(bit0=忙)          */
#define W25Q32_READ_DATA 0x03       /* 读数据                         */
#define W25Q32_WRITE_DATA 0x02      /* 页编程(256B)                   */
#define W25Q32_ERASE_SECTOR 0x20    /* 扇区擦除(4KB)                  */
#define W25Q32_WRITE_ENABLE 0X06    /* 写使能                         */

/* 拉低片选(选中芯片) */
void Int_w25q32_start(void);

/* 拉高片选(释放芯片) */
void Int_w25q32_stop(void);

/* SPI 发送一个字节 */
void Int_w25q32_write_byte(uint8_t data);

/* SPI 接收一个字节 */
uint8_t Int_w25q32_read_byte(void);

/* 读取芯片 ID(厂商ID + 16位器件ID) */
void Int_w25q32_read_id(uint8_t *mf_id, uint16_t *device_id);

/* 按 block/sector/page/addr 拆分地址读数据(旧接口, 兼容保留) */
void Int_w25q32_read_data(uint8_t block,uint8_t sector,uint8_t page,uint8_t addr, uint8_t *data, uint16_t len);

/* 使用 32 位绝对地址读数据 */
void Int_w25q32_read_data_with_32addr(uint32_t addr, uint8_t *data, uint16_t len);

/* 按 block/sector/page/addr 拆分地址写数据(旧接口, 兼容保留) */
void Int_w25q32_write_data(uint8_t block,uint8_t sector,uint8_t page,uint8_t addr, uint8_t *data, uint16_t len);

/* 使用 32 位绝对地址写数据(1 次最多 1 页, 且不能跨页) */
void Int_w25q32_write_data_with_32addr(uint32_t addr, uint8_t *data, uint16_t len);

/* 按 block/sector 擦除 1 个 4KB 扇区(旧接口) */
void Int_w25q32_erase_sector(uint8_t block,uint8_t sector);

/* ---- 新增(OTA 使用) ---- */

/* 使用 32 位地址擦除 1 个 4KB 扇区(addr 必须 4KB 对齐) */
void Int_w25q32_erase_4k(uint32_t addr);

/* 跨页安全写入(自动拆分 256B 页边界), 推荐用于固件缓存写入 */
void Int_w25q32_write_data_safe(uint32_t addr, const uint8_t *data, uint32_t len);

#endif /* __INT_W25Q32__ */
