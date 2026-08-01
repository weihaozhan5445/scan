/*
 * ============================================================================
 * 文件: display_service.c
 * 功能: 显示服务 —— 把环境数据渲染到 OLED 屏。
 * 布局: 4 行(每行 8x16 字符占 2 页, 页偏移 0/2/4/6):
 *       温度 / 湿度 / 光照 / 烟雾+报警状态。
 * ============================================================================
 */
#include "display_service.h"
#include "OLED.h"
#include <stdio.h>

/*
 * 渲染一帧环境数据到 OLED。
 * 参数 data: 环境数据结构(见 environment_data.h), 为 NULL 时直接返回。
 */
void DisplayService_Render(const EnvironmentData *data)
{
    char text[24];

    if (data == NULL) return;

    OLED_Clear();
    (void)snprintf(text, sizeof(text), "Temp:%.1f C", data->temperature_c);
    OLED_ShowString(0U, 0U, text);
    (void)snprintf(text, sizeof(text), "Hum:%.1f %%RH", data->humidity_percent);
    OLED_ShowString(0U, 2U, text);
    (void)snprintf(text, sizeof(text), "Light:%u lx", data->illuminance_lux);
    OLED_ShowString(0U, 4U, text);
    (void)snprintf(text, sizeof(text), "Smoke:%.1f %s", data->smoke_ppm,
                   data->alarm_active != 0U ? "ALARM" : "Normal");
    OLED_ShowString(0U, 6U, text);
}
