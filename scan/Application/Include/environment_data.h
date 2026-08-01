#ifndef ENVIRONMENT_DATA_H
#define ENVIRONMENT_DATA_H

#include <stdint.h>

typedef struct
{
    float temperature_c;
    float humidity_percent;
    uint16_t illuminance_lux;
    float smoke_ppm;
    uint8_t alarm_active;
} EnvironmentData;

uint8_t EnvironmentData_Read(EnvironmentData *data);

#endif
