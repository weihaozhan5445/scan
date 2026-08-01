#ifndef __APP__SHOW__
#define __APP__SHOW__

#include "APP_GetData.h"
#include "OLED.h"

//入参直接塞结构体，一行刷新全部屏幕
void OLED_RefreshAll(APP_ALL_DATA_type *app_all_data);

#endif
