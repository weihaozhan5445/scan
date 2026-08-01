#include "BUZZER.h"
#include "stm32f1xx_hal.h"

void BUZZER_ON(void)
{
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_SET);
}
void BUZZER_OFF(void)
{
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_RESET);
}
