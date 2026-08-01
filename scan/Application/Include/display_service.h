/*
 * 文件: display_service.h
 * 功能: OLED 显示服务接口。
 */
#ifndef DISPLAY_SERVICE_H
#define DISPLAY_SERVICE_H

#include "environment_data.h"

/* 把环境数据渲染到 OLED 屏(4 行布局) */
void DisplayService_Render(const EnvironmentData *data);

#endif
