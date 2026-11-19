#include "clock.h"

/* =========================================================
 * clock.c —— 秒表 + 倒计时
 *
 * 两个功能是同一个套路：
 *     1) 用一个变量记住"已经过了多少毫秒"
 *     2) 主循环每转一圈，如果正在跑，就把"现在 - 上次"这段加进去（或减掉）
 *
 * 为什么不直接用 HAL_Delay(1000) 来数秒？
 *     因为主循环还要刷屏、发串口，不能停下来等。
 *     这里只记时间点，用的时候再相减，主循环永远不会被卡住。
 * ========================================================= */

/* 24 小时 = 24 * 60 * 60 * 1000 = 86,400,000 毫秒
   注意：这个数超过了 16 位（65535），所以必须用 uint32_t 装，不能用 uint16_t */
#define COUNTDOWN_START_MS   (24UL * 60UL * 60UL * 1000UL)

/* ---------------- 秒表 ---------------- */

static uint32_t sw_ms   = 0;   // 已经跑了多少毫秒
static uint32_t sw_last = 0;   // 上次累加时的时刻
static uint8_t  sw_run  = 0;   // 1 = 正在跑，0 = 停着

void stopwatch_toggle(void)
{
    if (sw_run == 0)
    {
        sw_run = 1;            // 现在没跑 -> 开始跑
    }
    else
    {
        sw_run = 0;            // 现在在跑 -> 暂停
    }
}

void stopwatch_clear(void)
{
    sw_run = 0;
    sw_ms  = 0;
}

uint32_t stopwatch_ms(void)
{
    return sw_ms;
}

uint8_t stopwatch_running(void)
{
    return sw_run;
}

/* ---------------- 倒计时 ---------------- */

static uint32_t cd_ms   = COUNTDOWN_START_MS;   // 还剩多少毫秒
static uint32_t cd_last = 0;
static uint8_t  cd_run  = 0;

void countdown_toggle(void)
{
    if (cd_run == 0)
    {
        cd_run = 1;
    }
    else
    {
        cd_run = 0;
    }
}

void countdown_clear(void)
{
    cd_run = 0;
    cd_ms  = COUNTDOWN_START_MS;    // 复位回 24 小时
}

uint32_t countdown_ms(void)
{
    return cd_ms;
}

uint8_t countdown_running(void)
{
    return cd_run;
}

/* ---------------- 时间推进（主循环每圈调一次） ---------------- */

void clock_task(void)
{
    uint32_t now;      // 现在的毫秒数
    uint32_t delta;    // 距离上次进来过了多少毫秒

    now = HAL_GetTick();

    /* --- 秒表：往上加 --- */
    if (sw_run == 1)
    {
        delta = now - sw_last;
        sw_ms = sw_ms + delta;
    }

    /* 不管跑没跑，都更新"上次时刻"。
       这样暂停期间的时间不会被算进去，恢复时也不会一下子跳一大段。 */
    sw_last = now;

    /* --- 倒计时：往下减 --- */
    if (cd_run == 1)
    {
        delta = now - cd_last;

        if (delta >= cd_ms)          // 这段时间比剩下的还长 -> 归零，停下
        {
            cd_ms  = 0;
            cd_run = 0;
        }
        else
        {
            cd_ms = cd_ms - delta;
        }
    }

    cd_last = now;
}
