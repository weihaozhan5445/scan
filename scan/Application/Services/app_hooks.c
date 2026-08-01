/*
 * ============================================================================
 * 文件: app_hooks.c
 * 功能: 看门狗驱动 + FreeRTOS 钩子函数。
 *
 * 为什么需要看门狗:
 *   - 任何任务死循环/内存溢出/断言失败都会导致系统卡死;
 *   - IWDG 是独立于内核的硬件定时器, 超时自动复位, 系统即可自恢复。
 *
 * 喂狗策略:
 *   - vApplicationIdleHook: 空闲任务运行即喂狗(系统正常调度的标志);
 *   - 采集任务每周期也喂一次(见 environment_tasks.c), 双保险;
 *   - 长时间阻塞操作(如 OTA 下载)内部会主动喂狗。
 *
 * 注意: IWDG 启动后无法软件关闭, 复位后继续运行, 所以 Bootloader
 *       同样需要喂狗(它已在主循环里喂)。
 * ============================================================================
 */
#include "app_hooks.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f1xx_hal.h"

/* IWDG 参数: LSI≈40kHz, 预分频 /64 => 1.6ms/计数, 重装 3125 => 约 5 秒 */
#define APP_WDG_PRESCALER  0x06U
#define APP_WDG_RELOAD     3125U

/*
 * 初始化看门狗:
 *   写密钥 0x5555 解锁寄存器 → 配置预分频/重装值 → 0xAAAA 重载 → 0xCCCC 启动。
 */
void App_Watchdog_Init(void)
{
    IWDG->KR = 0x5555U;
    IWDG->PR = APP_WDG_PRESCALER;
    IWDG->RLR = APP_WDG_RELOAD;
    IWDG->KR = 0xAAAAU;
    IWDG->KR = 0xCCCCU;
}

/* 喂狗: 写入密钥 0xAAAA 即重载计数器 */
void App_Watchdog_Feed(void)
{
    IWDG->KR = 0xAAAAU;
}

/* ---- FreeRTOS 空闲钩子: 系统有空闲时间片就喂狗 ----
 * 说明: 空闲任务能运行 => 调度器正常 => 没有任务把 CPU 独占死循环。 */
void vApplicationIdleHook(void)
{
    App_Watchdog_Feed();
}

/* ---- 栈溢出钩子: 某个任务栈溢出了 ----
 * 关中断后死循环喂狗: 等待看门狗复位, 让系统自动恢复而不是带病运行。 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    __disable_irq();
    for (;;)
    {
        App_Watchdog_Feed();
    }
}

/* ---- 内存分配失败钩子: 堆不够用了 ----
 * 同样关中断死循环喂狗, 让看门狗复位重启(重启后堆会重新整理)。 */
void vApplicationMallocFailedHook(void)
{
    __disable_irq();
    for (;;)
    {
        App_Watchdog_Feed();
    }
}

/* ---- configASSERT 断言失败处理(见 FreeRTOSConfig.h) ---- */
void vApplicationAssert(const char *file, int line)
{
    (void)file;
    (void)line;
    __disable_irq();
    for (;;)
    {
        App_Watchdog_Feed();
    }
}
