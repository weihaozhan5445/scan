/*
 * 文件: ota_service.h
 * 功能: OTA 升级服务接口(状态机 + MQTT 指令解析)。
 */
#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

/* 解析云端 MQTT 下发的 JSON 指令(OTA/阈值), 见 ota_command_service.c */
void MQTT_ParseDownMsg(char *message);
/* OTA 升级状态机, 由 OTA 任务周期调用, 见 firmware_update_service.c */
void App_update_work(void);

#endif
