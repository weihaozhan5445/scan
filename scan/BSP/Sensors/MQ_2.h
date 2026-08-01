/*
 * 文件: MQ_2.h
 * 功能: MQ-2 烟雾传感器接口(ADC1+PA0, DMA 采样)。
 */
#ifndef __MQ2_H
#define __MQ2_H

#include <stdint.h>

#define MQ_SAMPLE_NUM 8U                      /* 每次采集的 ADC 采样个数 */
extern volatile uint16_t adc_dma_buf[MQ_SAMPLE_NUM]; /* DMA 采样缓冲 */

uint16_t MQ2_GetAdc_Avg(void);    /* 去极值平均后的 ADC 原始值 */
float MQ2_GetPPM(void);           /* 粗略烟雾浓度(ppm) */

#endif
