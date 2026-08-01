/*
 * ============================================================================
 * 文件: Dht11.c
 * 功能: DHT11 温湿度传感器驱动(单总线协议, PB15)。
 *
 * 时序要点(数据位):
 *   主机拉低 >=18ms 启动信号 → 释放 → DHT11 拉低响应(80us) → 拉高(80us)
 *   → 开始发 40 位数据(5 字节: 湿度整/小、温度整/小、校验和)。
 *   每位: 50us 低电平 + 26~28us(0) 或 70us(1) 高电平。
 *
 * 健壮性(相对原版修复):
 *   1) dht11_init 原来在调度器启动前调用 vTaskDelay(2000) 会崩溃,
 *      已改为 HAL_Delay;
 *   2) 位同步/响应等待循环全部加了超时保护, 传感器异常不会死循环卡死任务。
 * ============================================================================
 */
#include "Dht11.h"

/* 全局数组固定地址存放 5 字节原始数据(调试方便, 地址稳定) */
uint8_t data[5] = {0};

/*
 * 初始化: 把数据线置高, 等待传感器上电稳定。
 * 注意: 调度器启动前不能调用 vTaskDelay, 这里用 HAL_Delay。
 */
void dht11_init(void)
{
    DHT11_DATA_H;
    HAL_Delay(2000);
}

/*
 * 读取一次温湿度。
 * 输出参数: *temperature=温度, *humidity=湿度(读取失败时保持原值)。
 * 数据有效性由 5 字节校验和(前4字节之和==第5字节)保证。
 */
void dht11_get_date(float *temperature, float *humidity)
{
    float temp = 0;
    float hum = 0;

    /* 1. 发起启动信号: 拉低 >=18ms, 再拉高, 让 DHT11 准备应答 */
    DHT11_DATA_L;
    HAL_Delay(20);
    DHT11_DATA_H;

    /* 2. 进入临界区: 后续为微秒级时序, 不能被高优先级任务抢占 */
    taskENTER_CRITICAL();

    uint32_t count_max = 0xffffff;
    /* 2.1 等待 DHT11 拉低(应答开始) */
    while (DHT11_DATA_Read == GPIO_PIN_SET && count_max--)
    {
    }
    /* 2.2 等待拉高(应答结束) */
    while (DHT11_DATA_Read == GPIO_PIN_RESET && count_max--)
    {
    }
    /* 2.3 等待再拉低(数据开始) */
    while (DHT11_DATA_Read == GPIO_PIN_SET && count_max--)
    {
    }
    if (count_max == 0)
    {
        /* 响应超时: 传感器未接入或损坏 */
        debug_printf("DHT11响应超时");
        taskEXIT_CRITICAL();
        return;
    }

    /* 3. 逐位接收 40 位数据(5 字节) */
    for (uint8_t i = 0; i < 5; i++)
    {
        data[i] = 0;
        for (uint8_t j = 0; j < 8; j++)
        {
            /* 每个数据位先是一段低电平(50us), 带超时保护 */
            count_max = 0xffffff;
            while (DHT11_DATA_Read == GPIO_PIN_RESET && count_max--)
            {
            }
            if (count_max == 0)
            {
                taskEXIT_CRITICAL();
                debug_printf("DHT11位同步超时");
                return;
            }
            delay_us(40);                     /* 高电平已持续40us */
            /* 若仍为高电平 => '1'; 否则为 '0' */
            if (DHT11_DATA_Read == GPIO_PIN_SET)
            {
                data[i] |= (uint8_t)(1U << (7 - j));   /* 高位在前 */
                /* 等待该位高电平结束(带超时) */
                count_max = 0xffffff;
                while (DHT11_DATA_Read == GPIO_PIN_SET && count_max--)
                {
                }
            }
        }
    }

    /* 4. 校验和: data[0]+data[1]+data[2]+data[3] == data[4] */
    uint32_t sum = data[0] + data[1] + data[2] + data[3];
    if ((uint8_t)sum == data[4])
    {
        hum = (float)data[0];                /* 湿度整数部分 */
        temp = (float)data[2];               /* 温度整数部分 */
        if (data[3] & 0x80)                  /* 温度负值标志 */
        {
            temp = -temp;
        }
        *humidity = hum;
        *temperature = temp;
    }
    else
    {
        /* 校验失败: 丢弃本次数据(保持上次输出值) */
    }
    taskEXIT_CRITICAL();
}
