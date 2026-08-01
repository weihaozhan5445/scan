/*
 * 文件: iap.h
 * 功能: App 与 Bootloader 之间的"启动协同"接口。
 *       - 向量表重定位(必须在 main 最开头调用)
 *       - 升级成功确认(Bootloader 靠它判断是否回滚)
 *       - 主动请求进入 Bootloader 控制台
 */
#ifndef IAP_H
#define IAP_H

#include <stdint.h>

/* 必须在 main() 第一条语句调用: 把向量表重定位到 0x08004000 */
void App_SetVectorTable(void);

/* App 正常运行后调用: 向 EEPROM 写确认标记, 通知 Bootloader 不要回滚 */
void App_Boot_Confirm(void);

/* 请求下一次复位进入 Bootloader 控制台(写 0x5A 后复位) */
void App_RequestBootloader(void);

#endif
