#include "stm32f1xx_hal.h"
#include "BH1750.h"

extern I2C_HandleTypeDef hi2c1;


#define BH_ADDR 0x46

void BH1750_init(void)
{
        //0x10为连续高分辨率采用模式
    uint8_t cmd=0x10;
    HAL_I2C_Master_Transmit(&hi2c1,BH_ADDR,&cmd,1,100);
    HAL_Delay(180);
}

uint16_t BH1750_Readlight(void)
{
    uint8_t dat[2];
    HAL_I2C_Master_Receive(&hi2c1,BH_ADDR,dat,2,100);
    //高低字节拼接
    return (dat[0]<<8|dat[1])/1.2f;
}


