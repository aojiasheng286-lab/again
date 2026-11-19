#ifndef __key_h
#define __key_h

#include "main.h"      // 用到 uint8_t 等基本类型

//事件

#define ev_none    0   // 没有事件
#define ev_short   1   // 短按
#define ev_long    2   // 长按
#define ev_both    3   // 双同按
#define ev_double  4   // 双击

//mode

#define MODE_BREATH 0   // 呼吸灯
#define MODE_SLOW   1   // 慢闪
#define MODE_FAST   2   // 快闪
#define MODE_FLOW   3   // 流水灯
#define MODE_OFF    4   // 全灭
#define MODE_MAX    5   // 模式总数

//声明一下

extern volatile uint8_t key_event;   // 最近一次按键事件
extern volatile uint8_t mode;        // 当前灯效模式

//对外函数

void key_scan_task(void);// 按键扫描 1ms
void led_mode_task(void);// 灯效
void led_mode_reset(void);// 切模式后复位灯效//作用

#endif
