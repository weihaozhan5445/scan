#ifndef __COM_DEBUG_H
#define __COM_DEBUG_H

#include "usart.h"
#include "stdio.h"
#include "stdarg.h"

//日志打印开关
#define DEBUG_ENABLE 1
#ifdef DEBUG_ENABLE

#define debug_printf(format,...) printf("[%s:%d]" format "\r\n",__FILE__,__LINE__, ##__VA_ARGS__)

#else
#define debug_printf(...)

#endif

#endif
