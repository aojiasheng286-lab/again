#ifndef __key_h
#define __key_h

#include "main.h" // 引入HAL库的基本定义，比如 uint8_t

// ================= 1. 宏定义按键事件 =================
#define ev_none       0
#define ev_short      1 // 短按
#define ev_long       2 // 长按
#define ev_both       3 // 双键同按
#define ev_double     4 // 双击

// ================= 2. 声明全局变量 =================

extern volatile uint8_t key_event;
extern volatile uint8_t mode; 

// ================= 3. 声明对外提供的函数 =================
void key_scan_task(void); // 按键扫描任务（放到 1ms 定时器中断里）
void led_mode_task(void); // 灯效状态机任务（放到 while(1) 里）

#endif