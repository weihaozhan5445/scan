/*
 * ============================================================================
 * 文件:    bl_config.h
 * 功能:    Bootloader 全局配置 —— 内存分区、外部存储器布局、OTA 镜像格式、
 *          看门狗/串口参数等所有"约定常量"的唯一出处。
 *
 * 芯片:    STM32F103C8T6  (64KB Flash / 20KB RAM, 72MHz)
 *
 * Flash 分区(必须与 App 工程保持一致):
 *   Bootloader : 0x08000000 - 0x08003FFF   (16KB)  本工程, 上电固定从此启动
 *   App        : 0x08004000 - 0x0800FFFF   (48KB)  主工程 scan, 由 Bootloader 跳转进入
 *
 * 外部器件:
 *   W25Q32  : 4MB SPI NOR Flash —— 存放"新固件暂存区"与"旧固件备份区"
 *   W24C02  : 256B I2C EEPROM   —— 存放升级状态机/版本号/启动失败计数
 * ============================================================================
 */
#ifndef BL_CONFIG_H
#define BL_CONFIG_H

#include <stdint.h>
#include <stddef.h>

/* ------------------------- 内部 Flash 分区 ------------------------- */
#define BL_BASE             0x08000000UL            /* Bootloader 起始地址        */
#define BL_SIZE             0x00004000UL            /* Bootloader 大小 = 16KB     */
#define APP_BASE            0x08004000UL            /* App 向量表起始地址         */
#define APP_MAX_SIZE        0x0000C000UL            /* App 最大大小 = 48KB        */
#define APP_END             (APP_BASE + APP_MAX_SIZE) /* App 区域结束地址(不包含) */

/* ------------------------- W25Q32 外部 Flash 布局 ------------------------- */
#define W25Q_SECTOR_SIZE    4096UL                  /* W25Q32 一个扇区 4KB        */
#define W25Q_PAGE_SIZE      256UL                   /* W25Q32 一页 256B(写限制)   */
#define W25Q_CAPACITY       0x400000UL              /* 总容量 4MB                 */

#define W25Q_STAGING_ADDR   0x000000UL              /* 暂存区: 新固件镜像(带16B头) */
#define W25Q_BACKUP_ADDR    0x100000UL              /* 备份区: 烧写前的旧 App 备份 */

/* ------------------------- W24C02 EEPROM 地址表 -------------------------
 * 与 App 工程 BSP/Storage/Int_w24c02.h 中的定义保持一致, 修改时两边要同步。 */
#define EE_INIT_FLAG        0x00U   /* 出厂初始化标记(0xAA=已初始化)            */
#define EE_FW_VER_MAJOR     0x01U   /* 当前运行 App 主版本号                    */
#define EE_FW_VER_MINOR     0x02U   /* 当前运行 App 次版本号                    */
#define EE_BOOT_STATE       0x03U   /* Boot 状态机字节(见下方取值)              */
#define EE_NEW_VER_MAJOR    0x04U   /* 新固件主版本号(暂存用)                   */
#define EE_NEW_VER_MINOR    0x05U   /* 新固件次版本号(暂存用)                   */
#define EE_BOOT_ATTEMPT_CNT 0x06U   /* 连续启动失败计数(升级确认机制)            */
#define EE_FW_LEN_H         0x07U   /* 新固件 payload 长度高字节                 */
#define EE_FW_LEN_L         0x08U   /* 新固件 payload 长度低字节                 */

#define EE_INIT_MAGIC       0xAAU   /* 出厂初始化完成的魔数                      */

/* ------------------------- Boot 状态机取值 -------------------------
 * 状态流转:
 *   0xFF(NORMAL) --App下载完固件并写0xAA--> 0xAA(STAGED)
 *   0xAA(STAGED) --Bootloader烧写成功--> 0xCC(FLASHED)
 *   0xCC(FLASHED) --App首次采集成功写0xFF--> 0xFF(NORMAL)
 *   0xCC(FLASHED) --连续3次启动未确认--> 自动回滚, 回 0xFF
 *   0x5A(ENTER_BL) --App请求进入Bootloader控制台--> 清0后进控制台 */
#define BOOT_STATE_NORMAL   0xFFU   /* 正常运行 / App 已确认                    */
#define BOOT_STATE_STAGED   0xAAU   /* 新固件已暂存到 W25Q32, 待烧写             */
#define BOOT_STATE_FLASHED  0xCCU   /* 已烧写到内部Flash, 等待 App 确认          */
#define BOOT_STATE_ENTER_BL 0x5AU   /* App 请求本次复位进入 Bootloader 控制台    */

/* 新固件烧写后, App 有 MAX_BOOT_ATTEMPTS 次启动机会来写 NORMAL 确认。
 * 若一直未确认(说明 App 崩溃/无法启动), 则从备份区回滚旧固件。 */
#define MAX_BOOT_ATTEMPTS   3U

/* ------------------------- OTA 镜像头格式 -------------------------
 * 新固件 = 16 字节头 + payload(App 二进制)。
 *   偏移 0x00 : magic     4B  "SCAN"(0x4E414353) / 备份用 "SCAB"(0x42414353)
 *   偏移 0x04 : ver_major 1B  版本主号
 *   偏移 0x05 : ver_minor 1B  版本次号
 *   偏移 0x06 : reserved  2B  保留(0)
 *   偏移 0x08 : length    4B  payload 字节数(小端)
 *   偏移 0x0C : crc32     4B  payload 的 CRC-32(与 zlib 一致, 小端)
 *   偏移 0x10 : payload    N  App 二进制(链接地址 0x08004000)              */
#define OTA_MAGIC_STAGING   0x4E414353UL            /* 暂存区镜像魔数 "SCAN"    */
#define OTA_MAGIC_BACKUP    0x42414353UL            /* 备份区镜像魔数 "SCAB"    */
#define OTA_HEADER_SIZE     16UL                    /* 镜像头固定 16 字节       */

/* OtaImageHeader: 用 __attribute__((packed)) 保证结构体与磁盘/Flash 上的
 * 字节布局完全一致(不做任何对齐填充), 便于直接整块读写。 */
typedef struct __attribute__((packed))
{
    uint32_t magic;      /* 魔数: OTA_MAGIC_STAGING / OTA_MAGIC_BACKUP          */
    uint8_t  ver_major;  /* 版本主号                                           */
    uint8_t  ver_minor;  /* 版本次号                                           */
    uint16_t reserved;   /* 保留字段, 恒为 0                                   */
    uint32_t length;     /* payload 大小(字节)                                 */
    uint32_t crc32;      /* 仅对 payload 计算的 CRC-32                          */
} OtaImageHeader;

/* ------------------------- 外设参数 ------------------------- */
#define BL_UART_BAUD       115200UL                 /* 调试串口波特率            */
#define BL_I2C_SPEED_HZ    100000UL                 /* I2C 时钟 100kHz           */
#define BL_SPI_PRESCALER   4U                       /* SPI 分频 PCLK2/4=18MHz    */

/* ------------------------- 独立看门狗 IWDG -------------------------
 * LSI 约 40kHz, 预分频 /64 => 1.6ms/计数, 重装值 3125 => 约 5 秒超时。
 * 注意: IWDG 一旦启动无法关闭, 复位后仍会继续运行, 所以 App 也要喂狗。 */
#define BL_WDG_PRESCALER   0x06U                    /* PR=0110 => /64            */
#define BL_WDG_RELOAD      3125U                    /* 约 5 秒                   */

/* 串口控制台开关: 1=启用(可用于恢复/调试), 0=关闭(完全静默)               */
#define BL_CONSOLE_ENABLE  1U

#endif /* BL_CONFIG_H */
