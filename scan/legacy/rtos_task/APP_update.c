#include "app_update.h"
#include <string.h>
#include <stdio.h>

UpdateStateDef update_state = UPDATE_IDLE;
uint8_t app_data_buff[APP_DATA_MAX_LEN] = {0};
uint32_t recv_fw_total_len = 0;

// OTA状态机主逻辑，FreeRTOS周期调用
void App_update_work(void)
{
    switch (update_state)
    {
        case UPDATE_IDLE:
            break;
        case UPDATE_RECV_SEND_CMD:
            printf("MQTT OTA: start download firmware\r\n");
            recv_fw_total_len = g_mqtt_ota_param.fw_len;
            uint8_t ret = App_DownloadFwByUrl(g_mqtt_ota_param.fw_url, g_mqtt_ota_param.fw_len, g_mqtt_ota_param.fw_crc);
            if (ret == 0)
            {
                update_state = UPDATE_RECV_CHECK_DATA;
            }
            else
            {
                // 下载失败上报云端
                char err_json[128];
                sprintf(err_json, "{\"ota_status\":\"download_fail\"}");
                ESP_MQTT_Publish(MQTT_UP_TOPIC, err_json);
                W25Q_SectorErase(W25Q_FW_CACHE_ADDR);
                recv_fw_total_len = 0;
                update_state = UPDATE_IDLE;
            }
            memset(app_data_buff, 0, APP_DATA_MAX_LEN);
            break;
        case UPDATE_RECV_CHECK_DATA:
            App_CheckFwCrc();
            break;
        case UPDATE_RECV_BOOT_UPDATE:
            App_SetBootUpgradeFlag();
            update_state = UPDATE_END;
            break;
        case UPDATE_END:
            HAL_Delay(1000); // 等待W24C02写入完成
            HAL_NVIC_SystemReset(); // 复位进入Bootloader
            break;
        default:
            update_state = UPDATE_IDLE;
            break;
    }
}

// 分段HTTP下载固件，写入W25Q32外部Flash缓存
uint8_t App_DownloadFwByUrl(char *url, uint32_t total_len, uint16_t expect_crc)
{
    uint32_t write_offset = 0;
    uint16_t pkg_len = 0;
    W25Q_SectorErase(W25Q_FW_CACHE_ADDR); // 清空旧固件缓存
    char at_cmd[256];
    sprintf(at_cmd, "AT+HTTPCGET=\"%s\",256\r\n", url);
    ESP_SendAT(at_cmd, 300);
    // 循环接收所有分包
    while (write_offset < total_len)
    {
        pkg_len = ESP_Http_Download_Block(app_data_buff, 2000);
        if (pkg_len == 0)
            return 1; // 超时下载失败
        W25Q_PageWrite(W25Q_FW_CACHE_ADDR + write_offset, app_data_buff, pkg_len);
        write_offset += pkg_len;
        memset(app_data_buff, 0, APP_DATA_MAX_LEN);
    }
    return 0;
}

// 读取W25Q内完整固件，分段计算全局CRC
void App_CheckFwCrc(void)
{
    uint8_t rd_buf[256] = {0};
    uint32_t addr = W25Q_FW_CACHE_ADDR;
    uint32_t remain = recv_fw_total_len;
    uint16_t crc_all = 0xFFFF;
    uint16_t exp_crc = g_mqtt_ota_param.fw_crc;

    while (remain > 0)
    {
        uint16_t rd_len = remain > 256 ? 256 : remain;
        W25Q_ReadData(addr, rd_buf, rd_len);
        crc_all ^= CRC16_Calc(rd_buf, rd_len);
        addr += rd_len;
        remain -= rd_len;
    }

    if (crc_all == exp_crc)
    {
        // CRC校验通过，存储固件信息到W24C02供Boot读取
        W24C02_WriteByte(ADDR_FW_CRC_H, crc_all >> 8);
        W24C02_WriteByte(ADDR_FW_CRC_L, crc_all & 0xFF);
        W24C02_WriteByte(ADDR_FW_LEN_H, (recv_fw_total_len >> 8) & 0xFF);
        W24C02_WriteByte(ADDR_FW_LEN_L, recv_fw_total_len & 0xFF);
        //写入新版本号//
        W24C02_WriteByte(ADDR_NEW_VER_MAJ, g_mqtt_ota_param.new_major);
        W24C02_WriteByte(ADDR_NEW_VER_MIN, g_mqtt_ota_param.new_minor);
        
        update_state = UPDATE_RECV_BOOT_UPDATE;
    }
    else
    {
        // 校验失败清空缓存，切回空闲状态
        W25Q_SectorErase(W25Q_FW_CACHE_ADDR);
        recv_fw_total_len = 0;
        char err_json[128];
        sprintf(err_json, "{\"ota_status\":\"crc_check_fail\"}");
        ESP_MQTT_Publish(MQTT_UP_TOPIC, err_json);
        update_state = UPDATE_IDLE;
    }
}

// W24C02写入0xAA升级标记，告知Boot复位后更新固件
void App_SetBootUpgradeFlag(void)
{
    // 仅写入Bootloader升级触发标记0xAA
    W24C02_WriteByte(ADDR_UPGRADE_FLAG, 0xAA);

    // 上报云端即将重启升级
    char ok_json[128];
    sprintf(ok_json, "{\"ota_status\":\"ready_reboot_to_update\"}");
    ESP_MQTT_Publish(MQTT_UP_TOPIC, ok_json);
}