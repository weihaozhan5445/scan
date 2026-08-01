#include "ESP8266.h"

uint8_t con_buf[150] = {0};
uint16_t pos = 0;

// 串口中断回调，接收AT回复
uint8_t one_byte;
uint8_t rx_buf[128];
uint16_t rx_len = 0;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)
    {
        rx_buf[rx_len++] = one_byte;
        // 防溢出
        if(rx_len >= sizeof(rx_buf))
        rx_len = 0;
        HAL_UART_Receive_IT(&huart1, &one_byte, 1);
    }
}
// 发送AT指令
uint8_t ESP_Send_AT(uint8_t *cmd)
{
    rx_len = 0;
    memset(rx_buf,0,sizeof(rx_buf));
    HAL_UART_Transmit(&huart1, cmd, strlen((char*)cmd), 500);
    HAL_Delay(50);
    HAL_UART_Receive_IT(&huart1, &one_byte, 1);
    return 1;
}

// 等待期望应答字符串，返回1成功，0超时失败
uint8_t ESP_Wait_Resp(uint32_t timeout_ms, uint8_t *resp_str)
{
    uint32_t tick_start = HAL_GetTick();
    while(HAL_GetTick() - tick_start < timeout_ms)
    {
        //strstr是在rxbuf里找respstr
        if(strstr((char*)rx_buf, (char*)resp_str) != NULL)
        {
            return 1;
        }
    }
    return 0;
}

// 连接WiFi
uint8_t ESP8266_Connect_Wifi(char *ssid, char *pwd)
{
    char tmp[128];
    ESP_Send_AT((uint8_t*)"AT+RST\r\n");
    HAL_Delay(1000);
    if(!ESP_Wait_Resp(1000, (uint8_t*)"OK"))
        return 0;

    ESP_Send_AT((uint8_t*)"AT+CWMODE=1\r\n");
    if(!ESP_Wait_Resp(500, (uint8_t*)"OK"))
        return 0;
//sprintf作用是拼接两个字符串
    sprintf(tmp, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
    ESP_Send_AT((uint8_t*)tmp);
    if(!ESP_Wait_Resp(3500, (uint8_t*)"WIFI CONNECTED"))
        return 0;

    return 1;
}

// TCP连接MQTT服务器（固定TCP，不许修改）
uint8_t ESP8266_Connect_TCP(char *ip, char *port)
{
    uint8_t cmd_buf[128] = {0};
    ESP_Send_AT((uint8_t*)"AT+CIPMUX=0\r\n");
    if(!ESP_Wait_Resp(1000, (uint8_t*)"OK"))
        return 0;

    sprintf((char*)cmd_buf, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", ip, port);
    ESP_Send_AT(cmd_buf);
    if(!ESP_Wait_Resp(3000, (uint8_t*)"CONNECT"))
        return 0;

    return 1;
}

// MQTT CONNECT报文组装（你原来移位组包代码完整保留）
void ESP_MQTT_Create_Connect_Packet(void)
{
    pos = 0;
    memset(con_buf, 0, sizeof(con_buf));

    const char *cid=DEV_ID,*user=PRO_ID"@"DEV_ID,*pwd=DEV_SECRET;

    con_buf[pos++] = 0x10;
    con_buf[pos++] = 12 + strlen(cid)+strlen(user)+strlen(pwd);
    con_buf[pos++] = 0; con_buf[pos++] = 4;
    memcpy(&con_buf[pos], "MQTT", 4);
    pos += 4;

    con_buf[pos++] = 0x04;
    con_buf[pos++] = 0; con_buf[pos++] = 0x3C;

    // ClientID长度+内容
    con_buf[pos++] = (strlen(cid)>>8)&0xFF;
    con_buf[pos++] = strlen(cid)&0xFF;
    memcpy(&con_buf[pos],cid,strlen(cid));
    pos += strlen(cid);

    // Username长度+内容
    con_buf[pos++] = (strlen(user)>>8)&0xFF;
    con_buf[pos++] = strlen(user)&0xFF;
    memcpy(&con_buf[pos],user,strlen(user));
    pos += strlen(user);

    // Password长度+内容
    con_buf[pos++] = (strlen(pwd)>>8)&0xFF;
    con_buf[pos++] = strlen(pwd)&0xFF;
    memcpy(&con_buf[pos],pwd,strlen(pwd));
    pos += strlen(pwd);
}

// MQTT PUBLISH 上报JSON数据
uint8_t ESP8266_MQTT_Publish(char *json_str)
{
    uint8_t at_buf[32] = {0};
    uint8_t mqtt_buf[256] = {0};
    uint16_t idx = 0;
    uint16_t topic_len = strlen(MQTT_PUB_TOPIC);
    uint16_t pay_len = strlen(json_str);
    uint16_t rem_len = 2 + topic_len + pay_len;

    mqtt_buf[idx++] = 0x30;
    mqtt_buf[idx++] = rem_len;

    mqtt_buf[idx++] = (topic_len>>8)&0xFF;
    mqtt_buf[idx++] = topic_len&0xFF;
    memcpy(&mqtt_buf[idx], MQTT_PUB_TOPIC, topic_len);
    idx += topic_len;

    memcpy(&mqtt_buf[idx], json_str, pay_len);
    idx += pay_len;

    sprintf((char*)at_buf, "AT+CIPSEND=%d\r\n", idx);
    ESP_Send_AT(at_buf);
    HAL_Delay(100);
    if(!ESP_Wait_Resp(500, (uint8_t*)">"))
        return 0;

    HAL_UART_Transmit(&huart1, mqtt_buf, idx, 200);
    return 1;
}