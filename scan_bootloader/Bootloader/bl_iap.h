/*
 * 文件: bl_iap.h
 * 功能: IAP 应用跳转接口 —— 校验 App 镜像有效性并跳转执行。
 */
#ifndef BL_IAP_H
#define BL_IAP_H

#include <stdint.h>

/* 校验 0x08004000 处是否是一个可执行的 App(返回 1=有效) */
uint8_t bl_app_is_valid(void);

/* 跳转到 App: 关闭中断、复位外设、设置向量表/栈指针后执行复位向量 */
void    bl_jump_to_app(void);

#endif
