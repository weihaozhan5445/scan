/*
 * ============================================================================
 * 文件: OLED.c
 * 功能: 0.96 寸 SSD1306 OLED(I2C) 驱动。
 * 接口: I2C1, 器件地址 0x78(7 位地址 0x3C)。
 *
 * 显示原理(SSD1306 页寻址模式):
 *   屏幕 128x64 被分为 8 页(Page0-7), 每页 8 行像素;
 *   写入一字节 = 某列在该页的 8 个垂直像素, bit0(LSB) = 页内最上行;
 *   一个 8x16 字符 = 上下 2 页 x 8 列 = 16 字节(格式见 OLED_DATA.c)。
 *
 * 相对原版修复:
 *   1) OLED_ShowString 原实现每字符只写 4 字节(2 列), 显示残缺;
 *      已改为写满 16 字节(8 列 x 上下两页);
 *   2) 原字库仅 12 字符且索引越界, 已替换为完整 95 字符 ASCII 字库,
 *      并做越界保护(不可打印字符显示为空格)。
 * ============================================================================
 */
#include "stm32f1xx_hal.h"
#include "OLED.h"

extern I2C_HandleTypeDef hi2c1;

/*
 * 向 OLED 写一字节。
 * cmd=0: 写命令; cmd=1: 写数据(GDDRAM)。
 * 控制字节: 0x00 表示后续为命令, 0x40 表示后续为数据。
 */
void OLED_WR_Byte(uint8_t dat, uint8_t cmd)
{
    uint8_t buf[2];

    buf[0] = (cmd != 0U) ? 0x40U : 0x00U;   /* 控制字节 */
    buf[1] = dat;                           /* 命令或数据 */
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, 2, 100);
}

/*
 * 设置显示坐标(页寻址模式):
 *   0xb0+y : 页地址(0-7);
 *   后两字节: 列地址高 4 位 | 0x10, 列地址低 4 位。
 */
void OLED_SetPos(uint8_t x, uint8_t y)
{
    OLED_WR_Byte(0xb0 + y, 0);                        /* 设置页 */
    OLED_WR_Byte(((x & 0xf0) >> 4) | 0x10, 0);        /* 列高 4 位 */
    OLED_WR_Byte(x & 0x0f, 0);                        /* 列低 4 位 */
}

/* 清屏: 逐页逐列写 0 */
void OLED_Clear(void)
{
    uint8_t y, x;

    for (y = 0; y < 8; y++)
    {
        OLED_WR_Byte(0xb0 + y, 0);
        OLED_WR_Byte(0x00, 0);
        OLED_WR_Byte(0x10, 0);
        for (x = 0; x < 128; x++)
            OLED_WR_Byte(0, 1);
    }
}

/*
 * OLED 初始化: 延时等上电稳定, 依次发送 SSD1306 配置命令序列
 * (开关显示/时钟/复用比/显示偏移/起始行/段重映射/COM扫描方向/对比度/预充电等)。
 */
void OLED_Init(void)
{
    HAL_Delay(100);
    OLED_WR_Byte(0xAE, 0);                       /* 关闭显示 */
    OLED_WR_Byte(0xD5, 0); OLED_WR_Byte(0x80, 0); /* 时钟分频 */
    OLED_WR_Byte(0xA8, 0); OLED_WR_Byte(0x3F, 0); /* 复用比 1/64 */
    OLED_WR_Byte(0xD3, 0); OLED_WR_Byte(0x00, 0); /* 显示偏移 0 */
    OLED_WR_Byte(0x40, 0);                       /* 起始行 0 */
    OLED_WR_Byte(0xA1, 0);                       /* 段重映射(左右镜像校正) */
    OLED_WR_Byte(0xC8, 0);                       /* COM 扫描方向反转 */
    OLED_WR_Byte(0xDA, 0); OLED_WR_Byte(0x12, 0); /* COM 引脚配置 */
    OLED_WR_Byte(0x81, 0); OLED_WR_Byte(0xCF, 0); /* 对比度 */
    OLED_WR_Byte(0xD9, 0); OLED_WR_Byte(0xF1, 0); /* 预充电周期 */
    OLED_WR_Byte(0xDB, 0); OLED_WR_Byte(0x30, 0); /* VCOMH */
    OLED_WR_Byte(0xA4, 0);                       /* 全屏点亮关闭 */
    OLED_WR_Byte(0xA6, 0);                       /* 正常显示(非反色) */
    OLED_WR_Byte(0xAF, 0);                       /* 打开显示 */
    OLED_Clear();
}

/*
 * 在 (x,y) 位置显示字符串。
 * y 为页(0-7), 每行字符占 2 页, 所以下一行应传 y+2(由调用方控制)。
 * 每个字符: 上半页写 8 列, 换到 y+1 页再写 8 列(与字库格式对应)。
 * 越界保护: 非 0x20-0x7E 的字符按空格显示, 防止字库索引越界。
 */
void OLED_ShowString(uint8_t x, uint8_t y, char *str)
{
    while ((str != NULL) && (*str != '\0'))
    {
        uint8_t idx;
        uint8_t i;

        /* 计算字符在字库中的索引; 越界字符回退为空格 */
        if ((*str < 0x20) || (*str > 0x7E))
        {
            idx = 0U;
        }
        else
        {
            idx = (uint8_t)(*str - 0x20);
        }

        /* 上半页: 列 x..x+7 */
        OLED_SetPos(x, y);
        for (i = 0U; i < 8U; i++)
        {
            OLED_WR_Byte(F8X16[idx * 16U + i], 1);
        }
        /* 下半页: 列 x..x+7 */
        OLED_SetPos(x, y + 1U);
        for (i = 0U; i < 8U; i++)
        {
            OLED_WR_Byte(F8X16[idx * 16U + 8U + i], 1);
        }

        x += 8U;                          /* 每字符占 8 列 */
        if (x > 120U)                     /* 超出一行则换行(下移 2 页) */
        {
            x = 0U;
            y += 2U;
        }
        str++;
    }
}

/* 显示数字(无符号整数): 内部转为字符串后调用 OLED_ShowString */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len)
{
    char buf[10];

    (void)len;
    sprintf(buf, "%lu", (unsigned long)num);
    OLED_ShowString(x, y, buf);
}
