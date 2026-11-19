#include "key.h"
#include "tim.h"        // 用到 htim3 和 __HAL_TIM_SET_COMPARE

/* =========================================================
 * key.c —— 按键识别 + LED 灯效
 *
 * 对外只有三样东西：
 *   key_scan_task()   1ms 定时器中断里调用，识别按键，写出 key_event
 *   led_mode_task()   主循环里调用，消费 key_event 改 mode，并驱动灯效
 *   led_mode_reset()  切换模式后复位灯效状态
 *
 * 数据流：按键 -> key_event -> mode -> 三路 PWM 亮度
 * ========================================================= */

/* ---------------- 1. 对外变量 ---------------- */

volatile uint8_t key_event = ev_none;      // 最近一次按键事件
volatile uint8_t mode      = MODE_BREATH;  // 当前灯效模式（取值见 key.h 的 MODE_xxx）

/* ---------------- 2. 内部变量 ---------------- */
/* static = 只有本文件能看见，别的文件即使重名也不冲突 */

/* 每个按键需要 5 个变量记录状态。下面以按键1 为例，按键2 完全一样：
 *
 *   key1_count         这次按下已经持续了多少毫秒
 *   key1_pressed       按满 20ms 了吗（= 消抖通过，才算真的按下）
 *   key1_long_sent     这次已经报过"长按"了吗（避免按住不放重复报）
 *   key1_wait_double   松手了，正在等 300ms 看有没有第二下
 *   key1_double_count  已经等了多少毫秒
 */

static uint16_t key1_count        = 0;
static uint8_t  key1_pressed      = 0;
static uint8_t  key1_long_sent    = 0;
static uint8_t  key1_wait_double  = 0;
static uint16_t key1_double_count = 0;

static uint16_t key2_count        = 0;
static uint8_t  key2_pressed      = 0;
static uint8_t  key2_long_sent    = 0;
static uint8_t  key2_wait_double  = 0;
static uint16_t key2_double_count = 0;

/* 双键同按的锁：按住期间只报一次 ev_both */
static uint8_t both_latched = 0;

/* 灯效用的变量 */
static uint32_t last_led_time = 0;   // 上一次更新灯效的时间
static uint16_t pwm_val       = 0;   // 呼吸灯当前亮度，0~1000
static uint8_t  pwm_dir       = 1;   // 1=正在变亮，0=正在变暗
static uint8_t  toggle_state  = 0;   // 慢闪/快闪的亮灭状态
static uint8_t  flow_step     = 0;   // 流水灯走到第几颗（0/1/2）

/* ---------------- 3. 小工具：一次设置三路亮度 ---------------- */

/* TIM3 的 ARR = 999，所以亮度取值范围 0~1000：0=全灭，1000=全亮。
   原来每处都写三行 __HAL_TIM_SET_COMPARE，抽成一个函数好读也好改。 */
static void set_leds(uint16_t c1, uint16_t c2, uint16_t c3)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, c1);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, c2);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, c3);
}

/* ---------------- 4. 按键扫描（1ms 定时器中断） ---------------- */

void key_scan_task(void)
{
    /* 两个按键都配置成上拉输入，所以按下读到 0，松开读到 1 */
    uint8_t p1 = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);
    uint8_t p2 = HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin);

    /* ===== 双键同按优先级最高 ===== */
    if (p1 == 0 && p2 == 0)
    {
        if (both_latched == 0)                  // 只在"刚同时按下"的那一下报一次
        {
            both_latched = 1;

            if (key_event == ev_none) key_event = ev_both;

            key1_long_sent   = 1;               // 别再报长按了
            key2_long_sent   = 1;
            key1_wait_double = 0;               // 也别再报双击
            key2_wait_double = 0;
            key1_count       = 0;
            key2_count       = 0;
        }
        return;                                 // 双键状态下，下面的单键逻辑全部跳过
    }
    both_latched = 0;                           // 松开了，解锁

    /* ===== 按键1：分三个阶段 ===== */

    /* 阶段① 按住期间：计时，并判断"双击"和"长按" */
    if (p1 == 0)
    {
        key1_count++;

        if (key1_count > 20) key1_pressed = 1;              // 按满 20ms，消抖通过

        if (key1_count > 20 && key1_wait_double == 1)       // 第二下也按稳了 -> 双击
        {
            key_event        = ev_double;
            key1_wait_double = 0;
            key1_long_sent   = 1;
        }

        if (key1_count > 1000 && key1_long_sent == 0)       // 按满 1 秒 -> 长按
        {
            key_event      = ev_long;
            key1_long_sent = 1;
        }
    }
    /* 阶段② 松开的那一毫秒：如果刚才是一次干净的短按，就开始等第二下 */
    else
    {
        if (key1_pressed == 1 && key1_long_sent == 0)
        {
            key1_wait_double  = 1;
            key1_double_count = 0;
        }
        key1_count   = 0;
        key1_pressed = 0;
    }

    /* 阶段③ 松手后等 300ms：期间没等到第二下，就判定为短按 */
    if (key1_wait_double == 1)
    {
        key1_double_count++;
        if (key1_double_count > 300)
        {
            key_event        = ev_short;
            key1_wait_double = 0;
            key1_long_sent   = 0;
        }
    }
    else if (p1 != 0)
    {
        key1_long_sent = 0;                     // 松开状态，为下一次按键做准备
    }

    /* ===== 按键2：和上面一模一样，只是变量名从 key1_ 换成 key2_ ===== */

    if (p2 == 0)
    {
        key2_count++;

        if (key2_count > 20) key2_pressed = 1;

        if (key2_count > 20 && key2_wait_double == 1)
        {
            key_event        = ev_double;
            key2_wait_double = 0;
            key2_long_sent   = 1;
        }

        if (key2_count > 1000 && key2_long_sent == 0)
        {
            key_event      = ev_long;
            key2_long_sent = 1;
        }
    }
    else
    {
        if (key2_pressed == 1 && key2_long_sent == 0)
        {
            key2_wait_double  = 1;
            key2_double_count = 0;
        }
        key2_count   = 0;
        key2_pressed = 0;
    }

    if (key2_wait_double == 1)
    {
        key2_double_count++;
        if (key2_double_count > 300)
        {
            key_event        = ev_short;
            key2_wait_double = 0;
            key2_long_sent   = 0;
        }
    }
    else if (p2 != 0)
    {
        key2_long_sent = 0;
    }
}

/* ---------------- 5. 灯效（主循环里调用） ---------------- */

void led_mode_task(uint8_t handle_key)
{
    /* 第一步：有按键事件就切换模式。
       handle_key = 0 时把按键让给别的模块（消息页就是这样）。 */
    if (handle_key && key_event != ev_none)
    {
        /* 读和清必须一起完成，否则中断在中间写入的新事件会被清零抹掉 */
        __disable_irq();
        uint8_t ev = key_event;
        key_event  = ev_none;
        __enable_irq();

        switch (ev)
        {
            case ev_short:  mode = (mode + 1) % MODE_MAX; break;  // 短按：依次切换
            case ev_long:   mode = MODE_OFF;              break;  // 长按：全灭
            case ev_both:   mode = MODE_BREATH;           break;  // 双键：回呼吸
            case ev_double: mode = MODE_FLOW;             break;  // 双击：流水灯
            default: break;
        }

        led_mode_reset();          // 切换后复位灯效状态
    }

    /* 第二步：按当前模式驱动三路亮度 */
    uint32_t now = HAL_GetTick();

    switch (mode)
    {
        case MODE_BREATH:                                   // 呼吸：每 20ms 变一格，0 -> 1000 -> 0
            if (now - last_led_time >= 20)
            {
                last_led_time = now;

                if (pwm_dir)
                {
                    pwm_val += 10;
                    if (pwm_val >= 1000) { pwm_val = 1000; pwm_dir = 0; }
                }
                else
                {
                    if (pwm_val < 10) { pwm_val = 0; pwm_dir = 1; }   // 到底了，转为变亮
                    else              { pwm_val -= 10; }
                }

                set_leds(pwm_val, pwm_val, pwm_val);
            }
            break;

        case MODE_SLOW:                                     // 慢闪：500ms 翻转一次
            if (now - last_led_time >= 500)
            {
                last_led_time = now;
                toggle_state  = !toggle_state;

                uint16_t v = toggle_state ? 1000 : 0;
                set_leds(v, v, v);
            }
            break;

        case MODE_FAST:                                     // 快闪：100ms 翻转一次
            if (now - last_led_time >= 100)
            {
                last_led_time = now;
                toggle_state  = !toggle_state;

                uint16_t v = toggle_state ? 1000 : 0;
                set_leds(v, v, v);
            }
            break;

        case MODE_FLOW:                                     // 流水灯：200ms 换一颗亮
            if (now - last_led_time >= 200)
            {
                last_led_time = now;

                if      (flow_step == 0) set_leds(1000,    0,    0);
                else if (flow_step == 1) set_leds(   0, 1000,    0);
                else                     set_leds(   0,    0, 1000);

                flow_step = (flow_step + 1) % 3;            // 0 -> 1 -> 2 -> 0
            }
            break;

        case MODE_OFF:                                      // 全灭
            if (now - last_led_time >= 100)
            {
                last_led_time = now;
                set_leds(0, 0, 0);
            }
            break;
    }
}

/* ---------------- 6. 切换模式后复位 ---------------- */

void led_mode_reset(void)
{
    flow_step = 0;                    // 流水灯从第 0 颗重新开始

    if (mode == MODE_BREATH)          // 呼吸灯从最暗重新开始，不要接着上次的亮度
    {
        pwm_val = 0;
        pwm_dir = 1;
    }
    else if (mode == MODE_OFF)        // 全灭：立刻熄灯，不等那 100ms 的门控
    {
        set_leds(0, 0, 0);
    }
}
