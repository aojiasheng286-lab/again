#ifndef __PAGE_H
#define __PAGE_H

#include "main.h"

// 页面编号：顺序就是按 KEY3 切换的顺序
typedef enum {
    PAGE_ADC = 0,     // 1/6  电压
    PAGE_MPU,         // 2/6  六轴数据
    PAGE_LED,         // 3/6  灯效亮度
    PAGE_MSG,         // 4/6  消息历史
    PAGE_SW,          // 5/6  秒表
    PAGE_CD,          // 6/6  倒计时
    PAGE_COUNT        // 页面总数，必须永远放最后
} PageId;

// ---------------- 页面系统 ----------------
void   page_key_scan(void);      // 1ms 定时器里调：扫描 KEY3(PB13)
void   page_request_next(void);  // 请求切下一页（中断里也能调）
void   page_task(void);          // 主循环里调：切页 + 定时刷屏
PageId page_current(void);       // 当前页号

// ---------------- 消息历史 ----------------
void msg_push(const char *s);    // 存一条消息（uart.c 收到整行时调）
void nav_key_task(void);         // 消息页的按键处理（main.c 在消息页调）

// ---------------- 给串口用的查询 ----------------
uint16_t    page_led_duty(uint8_t ch);  // 第 ch 号灯(1/2/3)的亮度百分比 0~100
const char *page_msg_status(void);      // "第几条/共几条 内容"

#endif
