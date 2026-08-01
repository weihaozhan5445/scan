# scan 工程 IAP 适配说明(Bootloader 配套)

本工程与桌面 `scan_bootloader` 配套使用: 本工程为 App(0x08004000, 48KB),
Bootloader 位于 0x08000000(16KB)。本 README 记录对原工程的修改点。

## 1. 编译链接
- `MDK-ARM/scan.uvprojx`:
  - IROM 起始 0x08004000, 大小 0xC000(48KB); IRAM 0x20000000, 0x5000;
  - 编译器定义保持 `USE_HAL_DRIVER,STM32F103xB`(向量表重定位由 main() 完成);
  - 新增源文件: Application/Services/iap.c、app_hooks.c、ota_command_service.c(已实现)、
    firmware_update_service.c(已实现), Common/crc32.c, 以及对应头文件。
- `MDK-ARM/scan/scan.sct` 由 Keil 按内存对话框自动生成, 不要手动删除。

## 2. 关键代码修改
| 文件 | 修改内容 |
|------|----------|
| Core/Src/main.c | 1) main() 第一条语句调用 `App_SetVectorTable()`(SCB->VTOR=0x08004000); 2) HAL_Init 后调用 `App_Watchdog_Init()` |
| Application/Tasks/environment_tasks.c | 采集任务第一次成功采集后调用 `App_Boot_Confirm()`; 增加 ESP 互斥锁; 上传固定 30s 周期; OTA 任务持锁执行状态机 |
| Application/Services/firmware_update_service.c | 完整 OTA 状态机: HTTP 下载→W25Q32 暂存→CRC-32 校验→写升级标记→复位 |
| Application/Services/ota_command_service.c | 解析云端 OTA 指令(fw_url/fw_len/fw_crc/new_major/new_minor)与阈值指令 |
| Middleware/Network/ESP8266.c/.h | 修复: USART1 IRQ 处理缺失导致 ESP 数据无法接收; JSON 数字 8 位溢出; HTTP 分包解析加固; 串口错误重挂接收 |
| Core/Src/usart.c | HAL_UART_MspInit 使能 USART1 全局中断(优先级6) |
| Core/Src/stm32f1xx_it.c | 新增 USART1_IRQHandler -> HAL_UART_IRQHandler |
| BSP/Storage/Int_w25q32.c/.h | 新增 `Int_w25q32_erase_4k(addr)` 与 `Int_w25q32_write_data_safe(addr,data,len)`(跨页安全写) |
| BSP/Storage/Int_w24c02.c/.h | 单字节写后增加 6ms 等待; EEPROM 地址表扩展(升级状态机/版本/长度) |
| BSP/Sensors/Dht11.c | 调度器启动前用 HAL_Delay 代替 vTaskDelay; 位同步循环加超时保护 |
| Common/delay.c | delay_us 改用 DWT 周期计数器, FreeRTOS 下不再依赖 SysTick |
| Application/Include/FreeRTOSConfig.h | tick=1ms(HAL_Delay 精度修复); 关闭 Tickless; 开启 idle/栈溢出/malloc 失败钩子; configASSERT |
| Application/Services/app_hooks.c | IWDG 初始化/喂狗; FreeRTOS 钩子(溢出/分配失败/断言) |

## 3. 重要修复(原工程 BUG)
1. **SysTick 50ms 而 HAL 按 1ms 计时**: 原 `configTICK_RATE_HZ=20`, SysTick 每 50ms 触发一次,
   `HAL_IncTick` 每次 +1, 导致 `HAL_Delay(800)` 实际 40s。已改为 1ms tick。
2. **USART1 中断未使能/无 IRQHandler**: ESP8266 收不到任何数据, MQTT/OTA 无法工作。已修复。
3. **FreeRTOS 启动前调用 vTaskDelay**: `dht11_init()` 在调度器启动前调用 `vTaskDelay(2000)` 会崩溃。已改用 HAL_Delay。
4. **上传任务 0.5s 一次上报**: 原 refactored 版本每次循环都发布(队列始终有数据), 已固定 30s 周期。
5. **ESP 访问无互斥**: 上传任务与 OTA 任务并发操作 USART1 会串数据, 已加互斥锁。
6. **看门狗缺失**: 任何任务死循环都会永久卡死, 已加入 IWDG(~5s) + 空闲钩子喂狗。

## 4. 生成 OTA 固件
```bat
:: 1) Keil 编译后生成 bin(0x08004000 起)
C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe --bin -o MDK-ARM\scan\scan_iap.bin MDK-ARM\scan\scan.axf
:: 2) 打包成 OTA 镜像(16B 头 + CRC32)
python C:\Users\zhanw\Desktop\scan_bootloader\tools\make_ota_image.py MDK-ARM\scan\scan_iap.bin MDK-ARM\scan\scan_ota.bin 1 0
:: 把 scan_ota.bin 放到 HTTP 服务器, 云端下发的 fw_url 指向它, fw_crc 填文件头里的 CRC32
```

## 5. 云平台参数
WiFi/阿里云三元组配置在 `Middleware/Network/wifi_config.h`(该文件不入库, 模板为 `wifi_config.h.example`)。克隆仓库后请先复制模板: `copy wifi_config.h.example wifi_config.h` 并填入真实值。

