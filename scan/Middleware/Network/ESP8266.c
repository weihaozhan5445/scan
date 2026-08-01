/*
 * ============================================================================
 * 文件: ESP8266.c
 * 功能: ESP8266 AT 指令驱动实现。
 *
 * 接收模型:
 *   USART1 中断把数据收进 esp_rx_buf, 空闲中断(HAL_UARTEx_RxEventCallback)
 *   记录有效长度 esp_rx_idx 并重新挂接收。上层解析函数只在
 *   [0, esp_rx_idx) 范围内查找(见 find_str_bounded), 避免读到旧数据;
 *   解析完成把 esp_rx_idx 清零表示"已消费"(不 memset, 避免与 DMA/中断竞争)。
 *
 * 相对原版修复:
 *   1) 增加 USART1_IRQHandler + NVIC 使能(原工程缺失, ESP 收不到数据);
 *   2) JSON 数字解析改为 32 位, 修复 >255 值溢出;
 *   3) HTTP 分包解析按长度字段解析, 不再依赖 '\r' 截断;
 *   4) 串口错误回调重新挂接收, 防止一次溢出后永久失联。
 * ============================================================================
 */
#include "ESP8266.h"
#include "usart.h"

/* ESP 返回数据缓冲区与有效长度(由空闲中断更新) */
uint8_t esp_rx_buf[ESP_RX_BUF_LEN] = {0};
uint16_t esp_rx_idx = 0;

/*
 * 在 [0, idx) 范围内查找目标字符串(不越界读旧数据)。
 * 返回匹配位置指针, 未找到返回 NULL。
 */
static char *find_str_bounded(uint16_t idx, const char *needle)
{
    uint16_t i;
    uint16_t nlen = (uint16_t)strlen(needle);

    if ((nlen == 0U) || (nlen > idx))
    {
        return NULL;
    }
    for (i = 0U; i + nlen <= idx; i++)
    {
        if (memcmp(&esp_rx_buf[i], needle, nlen) == 0)
        {
            return (char *)&esp_rx_buf[i];
        }
    }
    return NULL;
}

/* 发送AT指令并延时等待模组处理 */
void ESP_SendAT(char *cmd, uint32_t wait_ms)
{
    if (cmd != NULL)
    {
        HAL_UART_Transmit(&ESP_USART_HANDLE, (uint8_t *)cmd, strlen(cmd), 100);
    }
    HAL_Delay(wait_ms);
}

/* 循环等待目标字符串返回: 1=成功 0=超时 */
uint8_t ESP_WaitResp(uint32_t timeout_ms, uint8_t *target_str)
{
    uint32_t tick_start = HAL_GetTick();

    while (HAL_GetTick() - tick_start < timeout_ms)
    {
        if (find_str_bounded(esp_rx_idx, (const char *)target_str) != NULL)
        {
            esp_rx_idx = 0;                 /* 标记已消费, 不 memset 避免与中断竞争 */
            return 1;
        }
    }
    return 0;
}

/*
 * 完整初始化: 复位模组 → STA模式 → 连WiFi → 打开MQTT通道 → 连接 → 订阅。
 * 任一步失败返回 0, 调用方稍后重试。
 */
uint8_t ESP_MQTT_Init(void)
{
    char at_buf[256] = {0};

    /* 1. 复位ESP模组 */
    ESP_SendAT("AT+RST\r\n", 500);
    esp_rx_idx = 0;
    HAL_Delay(800);
    /* 2. 设置WiFi为STA模式 */
    ESP_SendAT("AT+CWMODE=1\r\n", 200);
    /* 3. 连接WiFi(等待获取到IP) */
    sprintf(at_buf, "AT+CWJAP=\"" WIFI_NAME "\",\"" WIFI_PWD "\"\r\n");
    ESP_SendAT(at_buf, 1500);
    if (!ESP_WaitResp(3000, (uint8_t *)"WIFI GOT IP"))
        return 0;
    /* 4. 关闭旧MQTT通道 */
    ESP_SendAT("AT+QMTCLOSE=0\r\n", 200);
    /* 5. 打开阿里云MQTT TCP通道 */
    sprintf(at_buf, "AT+QMTOPEN=0,\"iot-as-mqtt.cn-shanghai.aliyuncs.com\",1883\r\n");
    ESP_SendAT(at_buf, 500);
    if (!ESP_WaitResp(1000, (uint8_t *)"+QMTOPEN:0,0"))
        return 0;
    /* 6. MQTT连接(模组自动用DEV_SECRET生成签名密码, MCU无需加密) */
    sprintf(at_buf, "AT+QMTCONN=0,\"%s\",\"%s\",\"%s\"\r\n", MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
    ESP_SendAT(at_buf, 500);
    if (!ESP_WaitResp(1000, (uint8_t *)"+QMTCONN:0,0"))
        return 0;
    /* 7. 订阅下行指令主题 */
    sprintf(at_buf, "AT+QMTSUB=0,1,\"%s\",1\r\n", MQTT_DOWN_TOPIC);
    ESP_SendAT(at_buf, 500);
    if (!ESP_WaitResp(1000, (uint8_t *)"+QMTSUB:0,1,0"))
        return 0;
    return 1;
}

/* MQTT发布JSON消息(AT+QMTPUB) */
void ESP_MQTT_Publish(char *topic, char *msg)
{
    char at_buf[512] = {0};

    if ((topic == NULL) || (msg == NULL))
    {
        return;
    }
    sprintf(at_buf, "AT+QMTPUB=0,0,0,0,\"%s\",\"%s\"\r\n", topic, msg);
    ESP_SendAT(at_buf, 200);
}

/*
 * 读取云端下发的MQTT JSON指令。
 * 在缓冲中定位 "+QMTRECV:" 之后第一个 "{...}" 并拷出。
 * 返回 1=取到一条指令, 0=暂无数据。
 */
uint8_t ESP_MQTT_GetRecvMsg(char *out_buf, uint16_t buf_len)
{
    char *p_msg;

    if ((out_buf == NULL) || (buf_len == 0U))
    {
        return 0;
    }
    p_msg = find_str_bounded(esp_rx_idx, "+QMTRECV:");
    if (p_msg == NULL)
    {
        return 0;
    }
    /* 跳到 JSON 对象起始 '{' */
    p_msg = strstr(p_msg, "\"{");
    if (p_msg == NULL)
    {
        esp_rx_idx = 0;
        return 0;
    }
    {
        uint16_t i = 0;

        /* 拷贝到 '}' 结束, 并防溢出 */
        while ((*p_msg != '}') && (i < buf_len - 1U))
        {
            out_buf[i++] = *p_msg++;
        }
        out_buf[i] = '\0';
    }
    esp_rx_idx = 0;                          /* 标记已消费 */
    return 1;
}

/*
 * HTTP GET 下载单段固件分包。
 * ESP AT 响应格式: "+HTTPCGET:<长度>\r\n<数据>"。
 * 这里先解析长度字段, 再按长度拷贝数据(最多 255B, 防缓冲溢出)。
 * 返回实际拷贝字节数; 0=超时。
 */
uint16_t ESP_Http_Download_Block(uint8_t *recv_buf, uint32_t timeout)
{
    uint32_t tick_start = HAL_GetTick();
    char *p_data;
    uint16_t plen = 0U;
    uint16_t len = 0U;

    if (recv_buf == NULL)
    {
        return 0U;
    }
    while (HAL_GetTick() - tick_start < timeout)
    {
        p_data = find_str_bounded(esp_rx_idx, "+HTTPCGET:");
        if (p_data != NULL)
        {
            p_data += 10;                     /* 跳过 "+HTTPCGET:" */
            /* 解析长度字段 */
            while ((*p_data >= '0') && (*p_data <= '9'))
            {
                plen = (uint16_t)(plen * 10U + (uint16_t)(*p_data - '0'));
                p_data++;
            }
            if (*p_data == '\r') p_data++;    /* 跳过 CRLF */
            if (*p_data == '\n') p_data++;
            /* 拷贝数据(不超过调用方缓冲 255B) */
            while ((len < plen) && (len < 255U))
            {
                recv_buf[len++] = (uint8_t)*p_data++;
            }
            esp_rx_idx = 0;                   /* 标记已消费 */
            return len;
        }
    }
    return 0U;
}

/* 提取JSON中数字类型字段(32位, 修复原8位溢出问题) */
uint32_t GetJsonNum32(char *json, const char *key)
{
    char find[32];
    char *p;
    uint32_t num = 0U;

    if ((json == NULL) || (key == NULL))
    {
        return 0U;
    }
    /* 拼接检索标记 "key": */
    sprintf(find, "\"%s\":", key);
    p = strstr(json, find);
    if (p == NULL)
    {
        return 0U;
    }
    p += strlen(find);
    /* 连续读取数字字符拼成十进制数 */
    while ((*p >= '0') && (*p <= '9'))
    {
        num = num * 10U + (uint32_t)(*p - '0');
        p++;
    }
    return num;
}

/* 提取JSON中字符串字段(如 fw_url), 带缓冲溢出保护 */
void GetJsonStr(char *json, const char *key, char *dst, uint16_t dst_len)
{
    char find[32];
    char *p;
    uint16_t i = 0U;

    if ((json == NULL) || (key == NULL) || (dst == NULL) || (dst_len == 0U))
    {
        return;
    }
    /* 拼接检索标记 "key":" */
    sprintf(find, "\"%s\":\"", key);
    p = strstr(json, find);
    if (p == NULL)
    {
        dst[0] = '\0';
        return;
    }
    p += strlen(find);
    /* 读取直到下一个双引号, 防止缓冲区溢出 */
    while ((*p != '"') && (*p != '\0') && (i < dst_len - 1U))
    {
        dst[i++] = *p++;
    }
    dst[i] = '\0';
}

/*
 * USART1 空闲中断回调: 收到一段完整数据时被 HAL_UART_IRQHandler 调用。
 * 记录本次有效长度 Size, 然后重新挂接接收(继续收下一段)。
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        esp_rx_idx = Size;
        HAL_UARTEx_ReceiveToIdle_IT(&ESP_USART_HANDLE, esp_rx_buf, ESP_RX_BUF_LEN);
    }
}

/*
 * 串口错误回调: 发生溢出/帧错/噪声错误后, 清错误标志并重新挂接收,
 * 避免一次错误导致 ESP 数据永久丢失。
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        HAL_UARTEx_ReceiveToIdle_IT(&ESP_USART_HANDLE, esp_rx_buf, ESP_RX_BUF_LEN);
    }
}
