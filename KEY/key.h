#ifndef __key_h
#define __key_h

#include "main.h"      // 用到 uint8_t 等基本类型

/* ---------------- 按键事件（key_event 的取值）---------------- */

#define ev_none    0   // 没有事件
#define ev_short   1   // 短按
#define ev_long    2   // 长按
#define ev_both    3   // 两个键同时按下
#define ev_double  4   // 双击

/* ---------------- 灯效模式（mode 的取值）---------------- */
/* 顺序就是短按依次切换的顺序 */

#define MODE_BREATH 0   // 呼吸灯
#define MODE_SLOW   1   // 慢闪
#define MODE_FAST   2   // 快闪
#define MODE_FLOW   3   // 流水灯
#define MODE_OFF    4   // 全灭
#define MODE_MAX    5   // 模式总数

/* ---------------- 对外变量 ---------------- */
/* 两个都加 volatile：它们在中断里被改，主循环里要读到最新值 */

extern volatile uint8_t key_event;   // 最近一次按键事件
extern volatile uint8_t mode;        // 当前灯效模式

/* ---------------- 对外函数 ---------------- */

void key_scan_task(void);              // 按键扫描：放进 1ms 定时器中断
void led_mode_task(uint8_t handle_key);// 灯效：放主循环；handle_key=0 时只跑动画不接手按键
void led_mode_reset(void);             // 切模式后复位灯效（按键和串口两条路都要调）

#endif
