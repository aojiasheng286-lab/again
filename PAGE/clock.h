#ifndef __CLOCK_H
#define __CLOCK_H

#include "main.h"

/* ---------------- 秒表：从 0 往上加 ---------------- */
void     stopwatch_toggle(void);   // 开始 / 暂停
void     stopwatch_clear(void);    // 清零
uint32_t stopwatch_ms(void);       // 已经跑了多少毫秒
uint8_t  stopwatch_running(void);  // 1 = 正在跑

/* ---------------- 倒计时：从 24 小时往下减 ---------------- */
void     countdown_toggle(void);   // 开始 / 暂停
void     countdown_clear(void);    // 复位回 24 小时
uint32_t countdown_ms(void);       // 还剩多少毫秒
uint8_t  countdown_running(void);  // 1 = 正在跑

/* 主循环每一圈都调一次，负责把时间往前推 */
void clock_task(void);

#endif
