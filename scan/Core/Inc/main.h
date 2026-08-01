/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define W25Q32_CS_GPIO_Port_Pin GPIO_PIN_4   /* W25Q32 片选 */
#define W25Q32_CS_GPIO_Port_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */
/*
 * 引脚映射总览(本项目自定义部分):
 *   PA4  = W25Q32 SPI Flash 片选
 *   PB0  = 蜂鸣器控制(BEER_CTRL, 开漏)
 *   PB15 = DHT11 温湿度数据线
 *   PA11 = ESP8266 使能(CH-PD)
 *   PA9/PA10 = USART1(TX/RX) -> ESP8266
 *   PA5/PA6/PA7 = SPI1(SCK/MISO/MOSI) -> W25Q32
 *   PB6/PB7 = I2C1(SCL/SDA) -> OLED + BH1750
 *   PB10/PB11 = I2C2(SCL/SDA) -> W24C02 EEPROM
 *   PA0  = ADC1_IN0 -> MQ-2 烟雾传感器(PA0-WKUP 复用)
 */
/* Compatibility aliases used by the board-support W25Q32 driver. */
#define W25Q32_CS_Pin W25Q32_CS_GPIO_Port_Pin
#define W25Q32_CS_GPIO_Port W25Q32_CS_GPIO_Port_GPIO_Port
/* USER CODE END Private defines */
#define BEER_CTRL_Pin GPIO_PIN_0             /* 蜂鸣器控制 */
#define BEER_CTRL_GPIO_Port GPIOB
#define DHT11_DATA_Pin GPIO_PIN_15           /* DHT11 数据线 */
#define DHT11_DATA_GPIO_Port GPIOB
#define CH_PD_Pin GPIO_PIN_11                /* ESP8266 使能 */
#define CH_PD_GPIO_Port GPIOA


#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
