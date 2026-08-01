#include "MQ_2.h"

volatile uint16_t adc_dma_buf[MQ_SAMPLE_NUM];

uint16_t MQ2_GetAdc_Avg(void)
{
    uint32_t sum = 0U;
    uint16_t minimum = adc_dma_buf[0];
    uint16_t maximum = adc_dma_buf[0];
    uint8_t index;

    for (index = 0U; index < MQ_SAMPLE_NUM; ++index)
    {
        uint16_t sample = adc_dma_buf[index];
        sum += sample;
        if (sample < minimum) minimum = sample;
        if (sample > maximum) maximum = sample;
    }

    return (uint16_t)((sum - minimum - maximum) / (MQ_SAMPLE_NUM - 2U));
}

float MQ2_GetPPM(void)
{
    return (float)MQ2_GetAdc_Avg() * 0.1f;
}
