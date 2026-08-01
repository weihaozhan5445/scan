#include "display_service.h"
#include "OLED.h"
#include <stdio.h>

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
