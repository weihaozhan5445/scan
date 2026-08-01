#include "stm32f1xx_hal.h"
#include "OLED.h"

extern I2C_HandleTypeDef hi2c1;

void OLED_WR_Byte(uint8_t dat,uint8_t cmd)
{
	uint8_t buf[2];
	if(cmd) buf[0]=0x40;
	else    buf[0]=0x00;
	buf[1]=dat;
	HAL_I2C_Master_Transmit(&hi2c1,OLED_ADDR,buf,2,100);
}

void OLED_SetPos(uint8_t x,uint8_t y)
{
	OLED_WR_Byte(0xb0+y,0);
	OLED_WR_Byte(((x&0xf0)>>4)|0x10,0);
	OLED_WR_Byte(x&0x0f,0);
}

void OLED_Clear(void)
{
	uint8_t y,x;
	for(y=0;y<8;y++)
	{
		OLED_WR_Byte(0xb0+y,0);
		OLED_WR_Byte(0x00,0);
		OLED_WR_Byte(0x10,0);
		for(x=0;x<128;x++)
			OLED_WR_Byte(0,1);
	}
}

void OLED_Init(void)
{
	HAL_Delay(100);
	OLED_WR_Byte(0xAE,0);
	OLED_WR_Byte(0xD5,0);OLED_WR_Byte(0x80,0);
	OLED_WR_Byte(0xA8,0);OLED_WR_Byte(0x3F,0);
	OLED_WR_Byte(0xD3,0);OLED_WR_Byte(0x00,0);
	OLED_WR_Byte(0x40,0);
	OLED_WR_Byte(0xA1,0);
	OLED_WR_Byte(0xC8,0);
	OLED_WR_Byte(0xDA,0);OLED_WR_Byte(0x12,0);
	OLED_WR_Byte(0x81,0);OLED_WR_Byte(0xCF,0);
	OLED_WR_Byte(0xD9,0);OLED_WR_Byte(0xF1,0);
	OLED_WR_Byte(0xDB,0);OLED_WR_Byte(0x30,0);
	OLED_WR_Byte(0xA4,0);
	OLED_WR_Byte(0xA6,0);
	OLED_WR_Byte(0xAF,0);
	OLED_Clear();
}

extern unsigned char F8X16[];
void OLED_ShowString(uint8_t x,uint8_t y,char *str)
{
	uint8_t i=0;
	while(str[i]!='\0')
	{
		OLED_SetPos(x,y);
		OLED_WR_Byte(F8X16[(str[i]-' ')*16],1);
		OLED_WR_Byte(F8X16[(str[i]-' ')*16+1],1);
		OLED_SetPos(x,y+1);
		OLED_WR_Byte(F8X16[(str[i]-' ')*16+8],1);
		OLED_WR_Byte(F8X16[(str[i]-' ')*16+9],1);
		x+=8;
		if(x>120){x=0;y+=2;}
		i++;
	}
}

void OLED_ShowNum(uint8_t x,uint8_t y,uint32_t num,uint8_t len)
{
	char buf[10];
	sprintf(buf,"%lu",(unsigned long)num);
	OLED_ShowString(x,y,buf);
}
