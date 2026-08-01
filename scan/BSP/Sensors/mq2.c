/*
 * ============================================================================
 * 文件: mq2.c
 * 功能: MQ-2 烟雾/可燃气体传感器驱动。
 * 采集: ADC1 通道 0(PA0) + DMA 循环采样 8 次, 见 Core/Src/adc.c。
 * 说明: MQ-2 输出为模拟电压, 与气体浓度的关系是非线性的;
 *       这里用"ADC 均值 x0.1"做粗略线性近似, 需要精确浓度时
 *       应根据传感器手册拟合曲线或实测标定后替换 MQ2_GetPPM()。
 * ============================================================================
 */
#include "MQ_2.h"

/* ADC DMA 采样缓冲(DMA 自动刷新, 见 main.c 的 HAL_ADC_Start_DMA) */
volatile uint16_t adc_dma_buf[MQ_SAMPLE_NUM];

/*
 * 取 ADC 采样平均值: 去掉一个最大值和一个最小值(抗干扰),
 * 再对剩余 6 个采样求平均。
 */
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

/* 粗略换算烟雾浓度(ppm), 见文件头说明 */
float MQ2_GetPPM(void)
{
    return (float)MQ2_GetAdc_Avg() * 0.1f;
}
