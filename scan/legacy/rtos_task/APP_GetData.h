#ifndef __APP_GETDATA_H
#define __APP_GETDATA_H

#include "MQ_2.h"
#include "Dht11.h"
#include "Buzzer.h"
#include "BH1750.h"

typedef struct 
{
    float Temp;
    float Hum;
    uint16_t light;
    float Smoke;
    uint8_t alarm_flag;
}APP_ALL_DATA_type;
#define TEMP_ALARM_C 35.0f
#define SMOKE_ALARM_PPM 65.0f

uint8_t app_get_data_all(APP_ALL_DATA_type *app_all_data);


#endif
