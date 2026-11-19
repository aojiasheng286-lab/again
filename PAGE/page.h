#ifndef __PAGE_H
#define __PAGE_H

#include "main.h"

// 页面编号：顺序就是按键切换的顺序
typedef enum {
    PAGE_ADC = 0,     // 电压
    PAGE_MPU,         // 六轴数据
    PAGE_MSG,
    PAGE_COUNT        // 只用来数总数，必须永远放最后
} PageId;

void    msg_push(const char *s);   // 存一条消息（uart.c 会调）
void    msg_key_task(void);        // 消息页按键处理（main.c 会调）
uint8_t msg_total(void);           // 现在有几条
void   page_key_scan(void);      // 1ms 定时器里调：扫描 PB13
void   page_request_next(void);  // 请求切下一页（中断里也能安全调用）
void   page_task(void);          // 主循环里调：处理切页 + 刷屏
PageId page_current(void);       // 当前页号

#endif
