# SCAN Bootloader (STM32F103C8T6)

基于原 `scan` 环境监测项目编写的 IAP Bootloader。Bootloader 负责：
1. 上电检查 W24C02 EEPROM 中的升级状态；
2. 有新固件(已由 App 下载到 W25Q32 暂存区)时, 先备份当前 App, 再烧写新固件并校验;
3. App 无法正常启动时自动回滚到上一个可用版本;
4. 无有效 App 时进入串口控制台, 方便恢复/调试。

## 1. Flash 分区(与 App 项目约定)

| 区域         | 地址范围                  | 大小  | 说明                         |
|--------------|---------------------------|-------|------------------------------|
| Bootloader   | 0x08000000 - 0x08003FFF   | 16KB  | 本工程, 固定从 0x08000000 启动 |
| App          | 0x08004000 - 0x0800FFFF   | 48KB  | 主工程 scan(已改为 IAP 链接) |
| RAM          | 0x20000000 - 0x20004FFF   | 20KB  |                              |

## 2. 外部存储器布局

### W25Q32(4MB SPI NOR)
| 槽位    | 地址       | 说明                         |
|---------|------------|------------------------------|
| 暂存区  | 0x000000   | App 下载的新固件镜像(带16B头) |
| 备份区  | 0x100000   | 烧写前对当前 App 的完整备份   |

### W24C02(256B I2C EEPROM)
| 地址 | 名称                | 说明                         |
|------|---------------------|------------------------------|
| 0x00 | ADDR_EEPROM_INIT_FLAG | 出厂初始化标记 0xAA          |
| 0x01 | FW_VER_MAJOR        | 当前 App 主版本               |
| 0x02 | FW_VER_MINOR        | 当前 App 次版本               |
| 0x03 | ADDR_UPGRADE_FLAG   | Boot 状态机字节(见下)         |
| 0x04 | NEW_VER_MAJOR       | 新版本主号                    |
| 0x05 | NEW_VER_MINOR       | 新版本次号                    |
| 0x06 | BOOT_ATTEMPT_CNT    | 连续启动失败计数               |
| 0x07 | FW_LEN_H            | 新固件 payload 长度(高)        |
| 0x08 | FW_LEN_L            | 新固件 payload 长度(低)        |

Boot 状态值:
| 值    | 含义                                   |
|-------|----------------------------------------|
| 0xFF  | 正常运行 / App 已确认                  |
| 0xAA  | 新固件已暂存, 待烧写                   |
| 0xCC  | 已烧写, 等待 App 确认(最多重试3次后回滚)|
| 0x5A  | App 请求进入 Bootloader 控制台         |

## 3. OTA 镜像格式

新固件 = 16 字节头 + App 二进制(payload)。

```
偏移  长度  字段
0x00  4     magic = "SCAN"(0x4E414353)
0x04  1     ver_major
0x05  1     ver_minor
0x06  2     reserved = 0
0x08  4     payload 长度(小端)
0x0C  4     payload 的 CRC-32(与 zlib/python 一致)
0x10  N     payload(App bin, 链接地址 0x08004000)
```

生成工具: `tools/make_ota_image.py`
```bat
python tools\make_ota_image.py ..\..\z\scan\MDK-ARM\scan\scan_iap.bin scan_ota.bin 1 0
```
(也支持在 App 工程 `MDK-ARM/scan` 下用 fromelf 先生成 bin:
`C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe --bin -o scan_iap.bin scan.axf`)

## 4. 编译

- Keil MDK-ARM V5.32 + ARMCC V5.06(与原工程一致), 打开 `MDK-ARM/bootloader.uvprojx` 直接 Build。
- 输出: `MDK-ARM/bootloader.hex`(0x08000000 起, 约 6.4KB)。

## 5. 首次烧录(出厂)

1. 用 ST-Link 把 `bootloader.hex` 烧到 0x08000000(全片擦除后再烧, 一次即可);
2. 用 ST-Link 把 App 工程生成的 `scan.hex`(0x08004000 起)烧到 0x08004000。
   **注意**: Keil App 工程下载时选择 "Erase Sectors"(不要选 Erase Full Chip), 以免擦掉 Bootloader。
3. 复位后 Bootloader 检测到有效 App 会立即跳转。

## 6. OTA 升级流程

1. 云端 MQTT 下发 `{"ota_enable":1,"fw_url":"...","fw_len":N,"fw_crc":CRC32,"new_major":1,"new_minor":1}`;
2. App 用 ESP8266 HTTP 分块下载固件到 W25Q32 暂存区, 边下载边计算 CRC-32;
3. App 校验通过后写 EEPROM 状态=0xAA(STAGED), 软件复位;
4. Bootloader 启动: 备份当前 App 到 W25Q32 备份区 → 擦除 App 区 → 烧写新固件 → 逐块比对 + 整体 CRC 校验 → 写状态=0xCC(FLASHED) → 跳转新 App;
5. 新 App 第一次采集成功时写状态=0xFF(NORMAL) 完成确认;
6. 若新 App 连续 3 次启动都未确认(崩溃/死机), Bootloader 自动从备份区回滚旧 App。

## 7. 串口控制台(USART1 @ PA9/PA10, 115200 8N1)

进入方式:
- 上电后 300ms 内发送字符 `b`(适合接 USB-TTL 调试);
- App 端调用 `App_RequestBootloader()`(EEPROM 写 0x5A 后复位);
- 芯片内没有有效 App 时自动进入。

| 命令 | 功能                     |
|------|--------------------------|
| H/?  | 帮助                     |
| V    | 版本/状态信息            |
| S    | 详细状态(暂存区/备份区)  |
| U    | 跳转 App                 |
| F    | 立即用暂存区固件升级     |
| B    | 立即备份当前 App         |
| E    | 擦除暂存区               |
| X    | 擦除备份区               |
| R    | 软件复位                 |

## 8. 健壮性设计

- 独立看门狗 IWDG(~5s): Bootloader 与 App 都喂狗, 任何死循环都会自动复位;
- 所有 SPI/I2C/Flash 操作带超时, 总线异常不会卡死;
- 烧写前备份 + 烧写后逐块验证 + 整体 CRC-32 校验;
- 断电恢复: 备份槽已写入后即使中途断电, 重新上电会从暂存区继续;
- App 启动确认机制防止"变砖"(最多 3 次失败自动回滚);
- 内部 Flash 操作只允许在 App 区域(0x08004000-0x0800FFFF), 误操作不会破坏 Bootloader。

## 9. 与原工程的差异说明

App 工程(scan)需要同步修改, 详见 `C:\Users\zhanw\Desktop\z\scan\README_IAP.md`。
核心: 链接地址改为 0x08004000、main() 最先重定位向量表、OTA 状态机真正实现、看门狗/FreeRTOS 配置加固。
