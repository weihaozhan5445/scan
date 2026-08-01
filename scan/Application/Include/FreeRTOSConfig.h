/*
 * ============================================================================
 * 文件: FreeRTOSConfig.h
 * 功能: FreeRTOS 内核配置(本文件在 include 路径最前, 是实际生效的配置)。
 *
 * 关键配置说明:
 *   - configTICK_RATE_HZ = 1000 : 系统时钟节拍 1ms。
 *     原工程为 20Hz(50ms 一拍), 而 HAL 的 HAL_GetTick/HAL_Delay 按 1ms 累加,
 *     导致 HAL_Delay(800) 实际要 40 秒; 改为 1ms 后与 HAL 对齐。
 *   - configUSE_TICKLESS_IDLE = 0 : 关闭无滴答低功耗。
 *     本设备由电源供电无需省电, 且低功耗会停 SysTick 导致 HAL 超时不准。
 *   - configUSE_IDLE_HOOK = 1 : 空闲钩子喂看门狗(app_hooks.c)。
 *   - configCHECK_FOR_STACK_OVERFLOW / MALLOC_FAILED_HOOK: 提前发现故障。
 * ============================================================================
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f1xx_hal.h"

#define configUSE_PREEMPTION 1                 /* 抢占式调度                */
#define configUSE_MUTEXES 1                    /* 允许使用互斥量(ESP串口锁) */
#define configUSE_IDLE_HOOK 1                  /* 空闲钩子(喂狗)            */
#define configUSE_TICK_HOOK 0
#define configCPU_CLOCK_HZ ((unsigned long)72000000)   /* 内核时钟 72MHz  */
#define configTICK_RATE_HZ ((TickType_t)1000)  /* 1ms 节拍, 与 HAL 对齐     */
#define configMAX_PRIORITIES 5                 /* 最大任务优先级数          */
#define configMINIMAL_STACK_SIZE ((unsigned short)128) /* 空闲任务栈(字)   */
#define configTOTAL_HEAP_SIZE ((size_t)(10 * 1024))    /* 堆 10KB          */
#define configMAX_TASK_NAME_LEN 16             /* 任务名最大长度            */
#define configUSE_TRACE_FACILITY 0             /* 关闭跟踪(省ROM)           */
#define configUSE_16_BIT_TICKS 0               /* 32 位 tick                */
#define configIDLE_SHOULD_YIELD 1

/* 故障检测: 栈溢出检查 + 内存分配失败钩子 */
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 1
/* 关闭无滴答低功耗: 保证 HAL_GetTick/超时准确 */
#define configUSE_TICKLESS_IDLE 0

/* 可选 API 开关 */
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskCleanUpResources 0
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskGetSchedulerState 1

/* ---- Cortex-M3 中断优先级(数值越小优先级越高) ---- */
#define configKERNEL_INTERRUPT_PRIORITY 255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 191     /* 0xB0 => 优先级11 */
#define configLIBRARY_KERNEL_INTERRUPT_PRIORITY 15

/* FreeRTOS 移植层要求的向量重映射 */
#define xPortPendSVHandler PendSV_Handler
#define vPortSVCHandler SVC_Handler

#define configUSE_TIME_SLICING 1               /* 同优先级时间片轮转      */

/* 断言: 内核检测到非法状态时调用 vApplicationAssert(在 app_hooks.c 实现) */
void vApplicationAssert(const char *file, int line);
#define configASSERT(x) if (!(x)) { vApplicationAssert(__FILE__, __LINE__); }

#endif
