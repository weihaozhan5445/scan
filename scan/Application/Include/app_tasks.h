/*
 * 文件: app_tasks.h
 * 功能: FreeRTOS 任务创建入口。
 */
#ifndef APP_TASKS_H
#define APP_TASKS_H

/* 创建全部任务/队列/互斥锁(main 中调用, 见 environment_tasks.c) */
void AppTasks_Create(void);

#endif
