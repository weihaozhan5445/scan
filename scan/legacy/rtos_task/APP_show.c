#include "APP_GetData.h"
#include "OLED.h"
#include <stdio.h>

void OLED_RefreshAll(APP_ALL_DATA_type *app_all_data)
{
    char buf[24];
    OLED_Clear();
//sprintf将数字转成字符串
    sprintf(buf,"Temp:%.1f C",app_all_data->Temp);
    OLED_ShowString(0,0,buf);

    sprintf(buf,"Hum:%.1f %%RH",app_all_data->Hum);
    OLED_ShowString(0,2,buf);

    sprintf(buf,"light:%d lx",app_all_data->light);
    OLED_ShowString(0,4,buf);

    if(app_all_data->alarm_flag)
        sprintf(buf,"Smoke:%.1f ALARM!",app_all_data->Smoke);
    else
        sprintf(buf,"Smoke:%.1f Normal",app_all_data->Smoke);
    OLED_ShowString(0,6,buf);
}
