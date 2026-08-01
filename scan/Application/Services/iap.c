/*
 * ============================================================================
 * 文件: iap.c
 * 功能: 实现 App 与 Bootloader 的启动协同逻辑(见 iap.h)。
 *
 * 背景: App 被链接到 0x08004000, 上电先执行 0x08000000 处的 Bootloader,
 *       Bootloader 跳转前会把向量表指到 App。这里再做一次显式设置,
 *       保证即使 Bootloader 未跳转(例如 App 被直接烧到 0x08004000 调试),
 *       向量表也是正确的。
 * ============================================================================
 */
#include "iap.h"
#include "ota_image.h"
#include "Int_w24c02.h"
#include "stm32f1xx_hal.h"

/*
 * 重定位向量表。
 * Cortex-M3(STM32F1) 通过 SCB->VTOR 指定向量表地址, 0x08004000 是 16KB 对齐,
 * 满足对齐要求。必须在任何中断(包括 HAL_Init 使能的 SysTick)之前执行。
 */
void App_SetVectorTable(void)
{
    SCB->VTOR = APP_FLASH_BASE;
}

/*
 * 升级确认: 读取 EEPROM 升级状态, 若为 FLASHED(0xCC)表示刚被 Bootloader
 * 烧写过新固件, 此时把状态写回 NORMAL(0xFF)并清零失败计数, 告诉 Bootloader
 * "新 App 能正常跑, 不要回滚"。若本来就是 NORMAL 则什么都不做。
 */
void App_Boot_Confirm(void)
{
    uint8_t state = Int_w24c02_read_byte(ADDR_UPGRADE_FLAG);

    if (state == BOOT_STATE_FLASHED)
    {
        (void)Int_w24c02_write_byte(ADDR_UPGRADE_FLAG, BOOT_STATE_NORMAL);
        (void)Int_w24c02_write_byte(ADDR_BOOT_ATTEMPT_CNT, 0U);
    }
}

/*
 * 请求进入 Bootloader 控制台:
 * 写 ENTER_BL(0x5A) 状态并复位。Bootloader 看到 0x5A 后会清状态并进入
 * 串口控制台(配合调试线使用, 例如现场维护/手动恢复)。
 */
void App_RequestBootloader(void)
{
    (void)Int_w24c02_write_byte(ADDR_UPGRADE_FLAG, BOOT_STATE_ENTER_BL);
    (void)Int_w24c02_write_byte(ADDR_BOOT_ATTEMPT_CNT, 0U);
    HAL_Delay(50U);               /* 等待 EEPROM 写周期完成 */
    HAL_NVIC_SystemReset();       /* 软件复位进入 Bootloader */
}
