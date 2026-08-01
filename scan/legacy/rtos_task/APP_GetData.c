#include "APP_GetData.h"

uint8_t app_get_data_all(APP_ALL_DATA_type *app_all_data)
{
    if(app_all_data == NULL) return 0;

    float t,h;
    //1.DHT22温湿度
  
    dht11_get_date(&t,&h);
    
    app_all_data->Temp = (float)t;
    app_all_data->Hum = (float)h;
    
    //2.BH1750光照
    app_all_data->light = BH1750_Readlight();

    //3.MQ2烟雾
    app_all_data->Smoke = MQ2_GetPPM();

    //4.自动判断报警标志
    app_all_data->alarm_flag = 0;
    if(app_all_data->Temp > temp_alarm || app_all_data->Smoke > smoke_alarm)
    {
        app_all_data->alarm_flag = 1;
    }

    return 1;
}
