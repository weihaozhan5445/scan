#include "APP_rtos.h"

/* 队列：存放10组环境数据 */
#define QUEUE_ITEM_NUM    10
QueueHandle_t xEnvDataQueue = NULL;

TaskHandle_t xGetDataTaskHandle = NULL;
TaskHandle_t xShowTaskHandle = NULL;
TaskHandle_t xAlarmTaskHandle = NULL;
TaskHandle_t xUploadTaskHandle = NULL;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(hadc->Instance == ADC1)
    {
        /* 中断中任务通知，不能用阻塞函数 */
        vTaskNotifyGiveFromISR(xGetDataTaskHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/*==================== 1.传感器采集任务(优先级3，栈512) ====================*/
static void vGetDatatTask(void *pvParameters)
{
   APP_ALL_DATA_type data_type;
    uint32_t notify;
    uint16_t adc_raw;

    while(1)
    {
        /* 阻塞等待ADC中断通知，没数据就挂起任务 */
        notify = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if(notify > 0)
        {
            uint16_t adc_avg = MQ2_GetAdc_Avg();; //直接读DMA缓存，硬件自动更新
            data_type.Smoke = MQ2_GetPPM(adc_avg);; //AD转烟雾浓度

        if(app_get_data_all(&data_type) == 1)
        {
            /* 入队，阻塞等待 */
            xQueueSend(xEnvDataQueue, &data_type, pdMS_TO_TICKS(20));
        }
        }   
        /* 500ms采集一次 */
      vTaskDelay(pdMS_TO_TICKS(500));
  }
}

/*==================== 2.OLED显示任务(优先级2，栈512) ====================*/
static void vShowTask(void *pvParameters)
{
    APP_ALL_DATA_type data_type;
    while(1)
    {
        /* 阻塞等待队列数据 */
        if(xQueueReceive(xEnvDataQueue, &data_type, portMAX_DELAY) == pdPASS)
        {
            OLED_RefreshAll(&data_type);
        }
    }
}

/*==================== 3.报警判断任务(优先级2，栈256) ====================*/
static void vAlarmTask(void *pvParameters)
{
    APP_ALL_DATA_type data_type;
    while(1)
    {
        if(xQueueReceive(xEnvDataQueue, &data_type, portMAX_DELAY) == pdPASS)
        {

            if(data_type.alarm_flag == 1)
            {
                BUZZER_ON();
            }
            else
            {
                BUZZER_OFF();
            }
        }
    }
}

/*==================== 4.串口/WiFi上传任务(优先级1，栈512) ====================*/

void vUploadTask(void *pvParameters)
{
    static uint8_t is_connected = 0;  // 联网成功标记，只初始化一次
    char json_buf[128];
    uint16_t humi, smoke;

    vTaskDelay(pdMS_TO_TICKS(800));

    while(1)
    {
        if(is_connected == 0)
        {
            // ========== 只执行一次：联网全套流程 ==========
            // 1、连接WiFi
            if(!ESP8266_Connect_Wifi(WIFI_NAME, WIFI_PWD))
            {
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue; // 联网失败，2秒后重试
            }
            // 2、TCP连接MQTT服务器
            if(!ESP8266_Connect_TCP("mqtt.cmiot.cn","1883"))
            {
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
            // 3、拼装MQTT CONNECT鉴权包、上线平台
            ESP_MQTT_Create_Connect_Packet();
            HAL_UART_Transmit(&huart1, con_buf, pos, 200);

            is_connected = 1;
            // 联网成功，后续不再走联网代码
        }
        else
        {
            // ========== 循环执行：周期上报数据 ==========
            // 读取传感器数据（换成你自己的读取函数）
            app_get_data_all(APP_ALL_DATA_type *app_all_data);

            // 打包OneNET物模型JSON
            snprintf(json_buf, sizeof(json_buf),
            "{\"$datastreams\":{"
            "\"mhumi\":{\"value\":%d},"
            "\"PM25\":{\"value\":%d}"
            "}}", hum, somke);

            // MQTT发布上云
            ESP8266_MQTT_Publish(json_buf);

            // 上报间隔，30秒一次
            vTaskDelay(pdMS_TO_TICKS(30000));
        }
    }
}

/*==================== 5.MQTT下发数据任务(优先级1，栈512) ====================*/
extern MqttOtaDef g_mqtt_ota_param;
void vOtaTask(void *pvParameters)
{
    char json_buf[256] = {0};
    for(;;)
    {
        
        // 1. 检测是否收到云端OTA下行JSON
        if(ESP_MQTT_GetRecvMsg(json_buf, sizeof(json_buf)) == 1)
        {
            // 解析所有OTA参数存入全局结构体
            g_mqtt_ota_param.ota_enable = GetJsonNum(json_buf, "ota_enable");
            GetJsonStr(json_buf, "fw_url", g_mqtt_ota_param.fw_url, 128);
            g_mqtt_ota_param.fw_len = GetJsonNum(json_buf, "fw_len");
            g_mqtt_ota_param.fw_crc = GetJsonNum(json_buf, "fw_crc");
            g_mqtt_ota_param.new_major = GetJsonNum(json_buf, "new_maj");
            g_mqtt_ota_param.new_minor = GetJsonNum(json_buf, "new_min");
        }

        // 2. 执行OTA下载、校验状态机
        App_update_work();

        // 100ms轮询一次，兼顾响应速度与CPU占用
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
/*==================== 任务统一创建入口（main调用） ====================*/
void App_CreateAllTask(void)
{
	 APP_ALL_DATA_type data_type;
    /* 1.创建消息队列 */
    xEnvDataQueue = xQueueCreate(QUEUE_ITEM_NUM, sizeof(data_type));

    if(xEnvDataQueue != NULL)
    {
        /* 优先级：采集>显示=报警>上传 */
        xTaskCreate(vGetDatatTask,"GetData",512,NULL,3,&xGetDataTaskHandle);
        xTaskCreate(vShowTask,"Show",512,NULL,2,&xShowTaskHandle);
        xTaskCreate(vAlarmTask,"Alarm",256,NULL,2,&xAlarmTaskHandle);
        xTaskCreate(vUploadTask,"Upload",512,NULL,1,&xUploadTaskHandle);
        xTaskCreate(vOtaTask,"OtaTask",512,NULL,1,NULL,xOtaTaskTaskHandle);
        
    }
}
