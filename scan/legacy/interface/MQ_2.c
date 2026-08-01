#include "stm32f1xx_hal.h"
#include "MQ_2.h"

#define MQ_SAMPLE_NUM    8U
extern ADC_HandleTypeDef hadc1;
// DMA搬运目标缓存，全局/静态数组
uint16_t adc_dma_buf[MQ_SAMPLE_NUM];

// DMA采集+滤波，返回平滑后的ADC值
uint16_t MQ2_GetAdc_Avg(void)
{
    uint32_t sum = 0U;
    uint16_t max_val, min_val;

    //  查找最大、最小值，同时累加总和
    max_val = min_val = adc_dma_buf[0];
    sum = adc_dma_buf[0];
    for(uint8_t i = 1; i < MQ_SAMPLE_NUM; i++)
    {
        if(adc_dma_buf[i] > max_val)
            max_val = adc_dma_buf[i];
        if(adc_dma_buf[i] < min_val)
            min_val = adc_dma_buf[i];
        sum += adc_dma_buf[i];
    }

    // 剔除极值求平均
    sum = sum - max_val - min_val;
    return (uint16_t)(sum / (MQ_SAMPLE_NUM - 2U));
}

uint16_t MQ2_GetPPM(void)
{
        // 先获取滤波后的ADC均值
    uint16_t adc_avg = MQ2_GetAdc_Avg();
    // 原始公式 adc电压 * 0.1 得到ppm，浮点运算转uint16
    return (uint16_t)((float)adc_avg * 0.1f);
}
