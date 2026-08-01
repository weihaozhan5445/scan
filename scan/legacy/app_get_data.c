#include "APP_GetData.h"

uint8_t app_get_data_all(APP_ALL_DATA_type *app_all_data)
{
    float temperature;
    float humidity;

    if (app_all_data == NULL)
    {
        return 0U;
    }

    dht11_get_date(&temperature, &humidity);
    app_all_data->Temp = temperature;
    app_all_data->Hum = humidity;
    app_all_data->light = BH1750_Readlight();
    app_all_data->Smoke = MQ2_GetPPM();
    app_all_data->alarm_flag = (app_all_data->Temp > TEMP_ALARM_C ||
                                app_all_data->Smoke > SMOKE_ALARM_PPM) ? 1U : 0U;
    return 1U;
}
