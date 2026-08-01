/*
 * 文件: app_hooks.h
 * 功能: 应用级"系统钩子"接口 —— 独立看门狗 IWDG 的初始化与喂狗。
 *       (FreeRTOS 的钩子函数实现见 app_hooks.c)
 */
#ifndef APP_HOOKS_H
#define APP_HOOKS_H

/* 初始化独立看门狗(与 Bootloader 相同参数, 约 5 秒超时) */
void App_Watchdog_Init(void);

/* 喂狗: 重载 IWDG 计数器 */
void App_Watchdog_Feed(void);

#endif
