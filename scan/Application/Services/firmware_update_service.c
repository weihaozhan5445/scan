/*
 * ============================================================================
 * 文件: firmware_update_service.c
 * 功能: OTA 固件升级服务 —— 升级状态机 + 下载/校验/置升级标记。
 *
 * 升级流水线(配合 Bootloader):
 *   1. UPDATE_RECV_SEND_CMD : 云端下发 OTA 指令后, 用 ESP8266 的 HTTP
 *                             AT 指令分块下载固件, 写入 W25Q32 暂存区,
 *                             同时边收边累计 CRC-32;
 *   2. UPDATE_RECV_CHECK_DATA: 从 W25Q32 重新读出固件, 整体 CRC-32 复核;
 *   3. UPDATE_RECV_BOOT_UPDATE: 把升级状态/版本/长度写入 W24C02 EEPROM;
 *   4. UPDATE_END            : 软件复位, 进入 Bootloader 完成烧写。
 *
 * 状态定义见 legacy/rtos_task/APP_update.h 的 UpdateStateDef。
 * ============================================================================
 */
#include "app_update.h"
#include "ota_image.h"
#include "crc32.h"
#include "app_hooks.h"
#include <string.h>
#include <stdio.h>

/* ---- 全局 OTA 状态(由 MQTT 解析/状态机共同操作) ---- */
UpdateStateDef update_state = UPDATE_IDLE;          /* 当前升级状态           */
uint8_t app_data_buff[APP_DATA_MAX_LEN];            /* 下载分包缓冲(256B)     */
uint32_t recv_fw_total_len;                         /* 已接收固件总长度       */

/* 边下载边累计的 CRC(分段调用 CRC32_Update) */
static uint32_t crc_stream;

/*
 * 通过 MQTT 向云端上报 OTA 状态(例如 download_fail / crc_check_fail)。
 * 注意: 下载进行中不要调用(ESP 模块忙于 HTTP, 插发 AT 会打断数据流)。
 */
static void publish_status(const char *status)
{
    char json[96];

    (void)snprintf(json, sizeof(json), "{\"ota_status\":\"%s\"}", status);
    ESP_MQTT_Publish(MQTT_UP_TOPIC, json);
}

/*
 * 分块 HTTP 下载固件到 W25Q32 暂存区。
 *
 * 流程:
 *   1. 参数合法性检查(url 非空、长度在 256B~48KB 且 4 字节对齐);
 *   2. 按需擦除暂存区的扇区(只擦会用到的扇区, 避免全片 15 秒擦除);
 *   3. 发 AT+HTTPCGET 启动 HTTP 下载, 循环取回 256B 分包;
 *   4. 每包写入 W25Q32(跨页安全写), 并累加 CRC-32;
 *   5. 收满后把"镜像头(魔数/版本/长度/CRC)"写入暂存区起始位置。
 *
 * 返回: 0=成功, 1=失败(参数非法/超时/长度不符)。
 */
uint8_t App_DownloadFwByUrl(char *url, uint32_t total_len, uint32_t expect_crc)
{
    uint32_t write_offset = 0U;                     /* 已写入 W25Q32 的字节数 */
    uint32_t sectors;
    uint32_t i;
    uint16_t pkg_len;
    OtaImageHeader hdr;
    char at_cmd[288];

    (void)expect_crc;                               /* CRC 以实际接收内容为准 */

    /* ---- 1. 输入校验, 防止垃圾参数破坏 Flash ---- */
    if ((url == NULL) || (url[0] == '\0'))
    {
        return 1U;
    }
    if ((total_len < 0x100U) || (total_len > APP_MAX_SIZE) || ((total_len & 0x3U) != 0U))
    {
        publish_status("param_invalid");
        return 1U;
    }

    /* ---- 2. 只擦除需要用到的扇区(每个 4KB) ---- */
    sectors = (total_len + OTA_HEADER_SIZE + W25Q_SECTOR_SIZE - 1U) / W25Q_SECTOR_SIZE;
    for (i = 0U; i < sectors; i++)
    {
        Int_w25q32_erase_4k(W25Q_STAGING_ADDR + i * W25Q_SECTOR_SIZE);
        App_Watchdog_Feed();
    }
    HAL_Delay(5U);

    crc_stream = 0xFFFFFFFFUL;                      /* CRC 初值 */
    write_offset = 0U;

    /* ---- 3. 启动 HTTP 分块下载 ---- */
    (void)snprintf(at_cmd, sizeof(at_cmd), "AT+HTTPCGET=\"%s\",256\r\n", url);
    ESP_SendAT(at_cmd, 300U);

    while (write_offset < total_len)
    {
        /* 取回一个分包(最多 256B); 返回 0 表示超时 */
        pkg_len = ESP_Http_Download_Block(app_data_buff, 2000U);
        if (pkg_len == 0U)
        {
            publish_status("download_fail");
            return 1U;
        }
        /* 防止服务端多发的尾部数据越界 */
        if (pkg_len > (total_len - write_offset))
        {
            pkg_len = (uint16_t)(total_len - write_offset);
        }
        /* ---- 4. 写入暂存区(payload 从 16B 头之后开始) + 累加 CRC ---- */
        Int_w25q32_write_data_safe(W25Q_STAGING_ADDR + OTA_HEADER_SIZE + write_offset,
                                   app_data_buff, pkg_len);
        crc_stream = CRC32_Update(crc_stream, app_data_buff, pkg_len);
        write_offset += pkg_len;
        App_Watchdog_Feed();                        /* 长下载期间喂狗 */
    }

    if (write_offset != total_len)
    {
        publish_status("download_fail");
        return 1U;
    }
    recv_fw_total_len = write_offset;

    /* ---- 5. 写镜像头(此时 CRC 才最终确定) ---- */
    hdr.magic = OTA_MAGIC_STAGING;
    hdr.ver_major = g_mqtt_ota_param.new_major;
    hdr.ver_minor = g_mqtt_ota_param.new_minor;
    hdr.reserved = 0U;
    hdr.length = recv_fw_total_len;
    hdr.crc32 = CRC32_Final(crc_stream);
    Int_w25q32_write_data_safe(W25Q_STAGING_ADDR, (const uint8_t *)&hdr, OTA_HEADER_SIZE);

    App_Watchdog_Feed();
    return 0U;
}

/*
 * 从 W25Q32 重新读取完整固件做整体 CRC-32 复核(下载阶段的边收边算
 * 不能替代"读回再验", 这里直接面对最终落盘的数据)。
 * 校验通过 → 状态切到 UPDATE_RECV_BOOT_UPDATE; 失败 → 回到 IDLE。
 */
void App_CheckFwCrc(void)
{
    OtaImageHeader hdr;
    uint8_t rd_buf[W25Q_PAGE_SIZE];
    uint32_t addr;
    uint32_t remain;
    uint32_t crc;
    uint32_t done;

    /* 读回镜像头并做基本检查 */
    Int_w25q32_read_data_with_32addr(W25Q_STAGING_ADDR, (uint8_t *)&hdr, OTA_HEADER_SIZE);
    if ((hdr.magic != OTA_MAGIC_STAGING) || (hdr.length != recv_fw_total_len) ||
        (hdr.length > APP_MAX_SIZE) || ((hdr.length & 0x3U) != 0U))
    {
        publish_status("crc_check_fail");
        update_state = UPDATE_IDLE;
        return;
    }

    /* 分页读出 payload 并重算 CRC */
    crc = 0xFFFFFFFFUL;
    addr = W25Q_STAGING_ADDR + OTA_HEADER_SIZE;
    remain = hdr.length;
    done = 0U;
    while (remain > 0U)
    {
        uint16_t rd_len = (remain > W25Q_PAGE_SIZE) ? W25Q_PAGE_SIZE : (uint16_t)remain;

        Int_w25q32_read_data_with_32addr(addr + done, rd_buf, rd_len);
        crc = CRC32_Update(crc, rd_buf, rd_len);
        done += rd_len;
        remain -= rd_len;
        App_Watchdog_Feed();
    }

    if (CRC32_Final(crc) != hdr.crc32)
    {
        publish_status("crc_check_fail");
        update_state = UPDATE_IDLE;
        return;
    }

    /* 校验通过 → 进入"写升级标记"阶段 */
    update_state = UPDATE_RECV_BOOT_UPDATE;
}

/*
 * 写入升级标记: 把 EEPROM 状态置为 STAGED(0xAA)+新版本+长度+清空失败计数。
 * Bootloader 复位后看到 0xAA 就会执行"备份→烧写→跳转"。
 */
void App_SetBootUpgradeFlag(void)
{
    Int_w24c02_write_byte(ADDR_UPGRADE_FLAG, BOOT_STATE_STAGED);
    Int_w24c02_write_byte(ADDR_NEW_VER_MAJ, g_mqtt_ota_param.new_major);
    Int_w24c02_write_byte(ADDR_NEW_VER_MIN, g_mqtt_ota_param.new_minor);
    Int_w24c02_write_byte(ADDR_FW_LEN_H, (uint8_t)(recv_fw_total_len >> 8));
    Int_w24c02_write_byte(ADDR_FW_LEN_L, (uint8_t)(recv_fw_total_len & 0xFFU));
    Int_w24c02_write_byte(ADDR_BOOT_ATTEMPT_CNT, 0U);

    publish_status("ready_reboot_to_update");
}

/*
 * OTA 状态机主逻辑(由 OTA 任务每 100ms 调用一次)。
 * 各状态含义见文件头; 下载阶段会阻塞较长时间, 由 OTA 任务单独承载。
 */
void App_update_work(void)
{
    switch (update_state)
    {
        case UPDATE_IDLE:
            /* 空闲: 正常业务, 什么都不做 */
            break;

        case UPDATE_RECV_SEND_CMD:
            /* 开始下载; 成功 → 进入复核, 失败 → 回空闲 */
            if (App_DownloadFwByUrl(g_mqtt_ota_param.fw_url,
                                    g_mqtt_ota_param.fw_len,
                                    g_mqtt_ota_param.fw_crc) == 0U)
            {
                update_state = UPDATE_RECV_CHECK_DATA;
            }
            else
            {
                update_state = UPDATE_IDLE;
            }
            break;

        case UPDATE_RECV_CHECK_DATA:
            /* 整体 CRC 复核(内部决定下一个状态) */
            App_CheckFwCrc();
            break;

        case UPDATE_RECV_BOOT_UPDATE:
            /* 写升级标记, 然后准备复位 */
            App_SetBootUpgradeFlag();
            update_state = UPDATE_END;
            break;

        case UPDATE_END:
            /* 等 EEPROM 写完, 复位进入 Bootloader */
            App_Watchdog_Feed();
            HAL_Delay(500U);
            HAL_NVIC_SystemReset();
            break;

        default:
            /* 未知状态兜底 */
            update_state = UPDATE_IDLE;
            break;
    }
}
