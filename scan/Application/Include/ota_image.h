/*
 * ============================================================================
 * 文件: ota_image.h
 * 功能: App 侧的 OTA 镜像格式与 W25Q32 布局定义。
 *
 * 重要: 本文件必须与 Bootloader 工程中的 bl_config.h 保持"值一致",
 *       它们是同一套 Flash 分区 / 镜像格式约定在两端的各自副本。
 *
 * STM32F103C8T6 分区:
 *   Bootloader : 0x08000000 - 0x08003FFF  (16KB)
 *   App        : 0x08004000 - 0x0800FFFF  (48KB, 本工程)
 * ============================================================================
 */
#ifndef OTA_IMAGE_H
#define OTA_IMAGE_H

#include <stdint.h>

/* ---- 内部 Flash App 区域 ---- */
#define APP_FLASH_BASE       0x08004000UL            /* App 向量表起始地址      */
#define APP_MAX_SIZE         0x0000C000UL            /* App 最大 48KB           */

/* ---- W25Q32(4MB)槽位 ---- */
#define W25Q_SECTOR_SIZE     4096UL                  /* 扇区 4KB                */
#define W25Q_PAGE_SIZE       256UL                   /* 页 256B                 */
#define W25Q_STAGING_ADDR    0x000000UL              /* 新固件暂存区(带16B头)    */
#define W25Q_BACKUP_ADDR     0x100000UL              /* 旧固件备份区            */

/* ---- OTA 镜像头(16 字节) ---- */
#define OTA_MAGIC_STAGING    0x4E414353UL            /* "SCAN": 暂存区镜像魔数  */
#define OTA_MAGIC_BACKUP     0x42414353UL            /* "SCAB": 备份区镜像魔数  */
#define OTA_HEADER_SIZE      16UL

/* 与 Bootloader 端完全相同的紧凑结构体(packed 保证无填充) */
typedef struct __attribute__((packed))
{
    uint32_t magic;      /* 魔数(区分暂存/备份)                                  */
    uint8_t  ver_major;  /* 版本主号                                            */
    uint8_t  ver_minor;  /* 版本次号                                            */
    uint16_t reserved;   /* 保留, 恒为 0                                        */
    uint32_t length;     /* payload 字节数(小端)                                */
    uint32_t crc32;      /* payload 的 CRC-32(与 zlib 一致)                      */
} OtaImageHeader;

#endif /* OTA_IMAGE_H */
