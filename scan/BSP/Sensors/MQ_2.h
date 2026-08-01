#ifndef __MQ2_H
#define __MQ2_H

#include <stdint.h>


uint16_t MQ2_GetAdc_Avg(void);
float MQ2_GetPPM(void);

#define MQ_SAMPLE_NUM 8U
extern volatile uint16_t adc_dma_buf[MQ_SAMPLE_NUM];

#endif
