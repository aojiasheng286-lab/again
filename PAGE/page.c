#include "page.h"
#include "oled.h"
#include "key.h"        // key_event / ev_short / mode
#include "my_adc.h"     // adc_get_voltage()
#include "mpu6050.h"    // g_mpu
#include "tim.h"
#include "clock.h"        // htim3（读三路 PWM 的占空比）
#include <stdio.h>
#include <string.h>

/* =========================================================
 * page.c —— 页面显示管理
 *
 * 一个页面 = 一个 render 函数，往固定的 8 行里写字。
 * 按 KEY3 或串口发 'p' 切下一页，主循环每 500ms 刷新一屏。
 * ========================================================= */

/* ---------------- 1. 页面状态 ---------------- */

static volatile PageId  page           = PAGE_ADC;  // 当前页
static volatile uint8_t page_next_flag = 0;         // 置 1 = 请求切下一页

void page_request_next(void)
{
    page_next_flag = 1;
}

PageId page_current(void)
{
    return page;
}


















/* =========================================================
 * 三个导航按键的扫描
 *
 * 三个键都接成"下拉输入"，所以：
 *     松开  ->  读到 0
 *     按下  ->  读到 1
 *
 * 每个键都要消抖：连续 20 毫秒读数都一样，才算真的变了。
 * 函数只在"刚刚稳定按下"的那一毫秒返回 1，其它时候返回 0。
 * ========================================================= */

/* ---------- KEY3（PB13）：翻页 ---------- */
static uint8_t key3_pressed(void)
{
    static uint8_t last = 0;   // 上次确认过的电平（0=松开 1=按下）
    static uint8_t cnt  = 0;   // 电平变了以后，持续了多少毫秒
    uint8_t now = HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin);

    if (now == last)           // 读数没变，计数清零
    {
        cnt = 0;
        return 0;
    }

    cnt++;                     // 读数变了，开始计时

    if (cnt < 20)              // 还没稳定 20 毫秒，先不算数
    {
        return 0;
    }

    cnt  = 0;
    last = now;                // 确认状态真的变了

    if (now == 1)              // 变成"按下"，报一次
    {
        return 1;
    }

    return 0;                  // 变成"松开"，不报
}

/* ---------- KEY4（PB12）：消息上一条 ---------- */
static uint8_t key4_pressed(void)
{
    static uint8_t last = 0;   // 上次确认过的电平（0=松开 1=按下）
    static uint8_t cnt  = 0;   // 电平变了以后，持续了多少毫秒
    uint8_t now = HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin);

    if (now == last)           // 读数没变，计数清零
    {
        cnt = 0;
        return 0;
    }

    cnt++;                     // 读数变了，开始计时

    if (cnt < 20)              // 还没稳定 20 毫秒，先不算数
    {
        return 0;
    }

    cnt  = 0;
    last = now;                // 确认状态真的变了

    if (now == 1)              // 变成"按下"，报一次
    {
        return 1;
    }

    return 0;                  // 变成"松开"，不报
}

/* ---------- KEY5（PB14）：消息下一条 ---------- */
static uint8_t key5_pressed(void)
{
    static uint8_t last = 0;   // 上次确认过的电平（0=松开 1=按下）
    static uint8_t cnt  = 0;   // 电平变了以后，持续了多少毫秒
    uint8_t now = HAL_GPIO_ReadPin(KEY5_GPIO_Port, KEY5_Pin);

    if (now == last)           // 读数没变，计数清零
    {
        cnt = 0;
        return 0;
    }

    cnt++;                     // 读数变了，开始计时

    if (cnt < 20)              // 还没稳定 20 毫秒，先不算数
    {
        return 0;
    }

    cnt  = 0;
    last = now;                // 确认状态真的变了

    if (now == 1)              // 变成"按下"，报一次
    {
        return 1;
    }

    return 0;                  // 变成"松开"，不报
}

static volatile uint8_t msg_up_flag   = 0;   // KEY4 按了一下 -> 消息上一条
static volatile uint8_t msg_down_flag = 0;   // KEY5 按了一下 -> 消息下一条
















/* ---------- 扫描入口：每 1 毫秒被调用一次 ---------- */
void page_key_scan(void)
{
    if (key3_pressed())        // KEY3：切下一页
    {
        page_next_flag = 1;
    }

    if (key4_pressed())        // KEY4：消息上一条
    {
        msg_up_flag = 1;
    }

    if (key5_pressed())        // KEY5：消息下一条
    {
        msg_down_flag = 1;
    }
}




















//消息历史
#define MSG_MAX 10    // 最多保存 10 条
#define MSG_LEN 32    // 每条最多 31 个字符

static char    msg[MSG_MAX][MSG_LEN];
static uint8_t msg_w    = 0;   // 下一条写到哪一格
static uint8_t msg_n    = 0;   // 已经存了几条（最多 10）
static uint8_t msg_view = 0;   // 正在看第几条

void msg_push(const char *s)
{
    strncpy(msg[msg_w], s, MSG_LEN - 1);
    msg[msg_w][MSG_LEN - 1] = '\0';       // 保证有结尾

    msg_w = msg_w + 1;

    if (msg_w >= MSG_MAX)                 // 存满 10 条，回到第 0 格，覆盖最旧的
    {
        msg_w = 0;
    }

    if (msg_n < MSG_MAX)
    {
        msg_n++;
    }

    msg_view = 0;                          // 来新消息就跳回第一条
}





















/* 两个导航键：在不同页面干不同的事
     - 消息页：               KEY4 = 上一条，  KEY5 = 下一条
     - 秒表页 / 倒计时页：   KEY4 = 开始/暂停，KEY5 = 清零 */
void nav_key_task(void)
{
    if (msg_up_flag == 1)                    // KEY4 按了一下
    {
        msg_up_flag = 0;

        if (page_current() == PAGE_MSG)      // 消息页：上一条
        {
            if (msg_n > 0)
            {
                if (msg_view == 0)
                {
                    msg_view = msg_n - 1;    // 已经是第一条，绕到最后一条
                }
                else
                {
                    msg_view--;
                }
            }
        }
        else if (page_current() == PAGE_SW)  // 秒表页：开始 / 暂停
        {
            stopwatch_toggle();
        }
        else if (page_current() == PAGE_CD)  // 倒计时页：开始 / 暂停
        {
            countdown_toggle();
        }
    }

    if (msg_down_flag == 1)                  // KEY5 按了一下
    {
        msg_down_flag = 0;

        if (page_current() == PAGE_MSG)      // 消息页：下一条
        {
            if (msg_n > 0)
            {
                msg_view = msg_view + 1;

                if (msg_view >= msg_n)
                {
                    msg_view = 0;            // 已经是最后一条，绕回第一条
                }
            }
        }
        else if (page_current() == PAGE_SW)  // 秒表页：清零
        {
            stopwatch_clear();
        }
        else if (page_current() == PAGE_CD)  // 倒计时页：复位回 24 小时
        {
            countdown_clear();
        }
    }
}






















/* 第 ch 号灯（1/2/3）的亮度百分比。
   TIM3 的 ARR = 999，CCR 范围 0~1000，除以 10 就是 0~100%。 */
uint16_t page_led_duty(uint8_t ch)
{
    if (ch == 1)
    {
        return (uint16_t)(__HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_1) / 10);
    }

    if (ch == 2)
    {
        return (uint16_t)(__HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_2) / 10);
    }

    return (uint16_t)(__HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_3) / 10);
}
















const char *page_msg_status(void)
{
    static char s[48];

    if (msg_n == 0)
    {
        strcpy(s, "no message");
    }
    else
    {
        sprintf(s, "%d/%d %s", msg_view + 1, msg_n, msg[msg_view]);
    }

    return s;
}













static void line_text(uint8_t row, const char *s)
{
    OLED_PrintASCIIString(0, row * 8, (char *)s, &afont8x6, OLED_COLOR_NORMAL);
}

static void line_value(uint8_t row, const char *name, float v)
{
    char buf[24];

    sprintf(buf, "%s=%.2f", name, v);
    line_text(row, buf);
}










//电压
static void page_render_adc(void)
{
    float   v = adc_get_voltage();
    uint8_t w;

    line_text(0, "1/4  ADC");
    line_value(2, "V", v);

    w = (uint8_t)(v / 3.3f * 120.0f);     // 3.3V 对应满宽 120 像素

    if (w > 120)
    {
        w = 120;
    }

    OLED_DrawRectangle(4, 40, 120, 10, OLED_COLOR_NORMAL);
    OLED_DrawFilledRectangle(4, 40, w, 10, OLED_COLOR_NORMAL);
}








//六轴
static void page_render_mpu(void)
{
    line_text(0,  "2/4  MPU");
    line_value(1, "T",  g_mpu.temp_c);
    line_value(3, "Ax", g_mpu.ax_g);
    line_value(4, "Ay", g_mpu.ay_g);
    line_value(5, "Az", g_mpu.az_g);
}















//小灯
static const char *const mode_name[] = { "BREATH", "SLOW", "FAST", "FLOW", "OFF" };

static void page_render_led(void)
{
    /* TIM3 的 ARR = 999，所以 CCR 是 0~1000，除以 10 正好是 0~100% */
    uint16_t c1 = (uint16_t)__HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_1);
    uint16_t c2 = (uint16_t)__HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_2);
    uint16_t c3 = (uint16_t)__HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_3);

    char buf[24];

    line_text(0, "3/4  LED");

    uint8_t m = mode;

    if (m >= MODE_MAX)              // 保险：模式值异常就按呼吸灯显示
    {
        m = 0;
    }

    sprintf(buf, "mode%d %s", m, mode_name[m]);
    line_text(1, buf);

    sprintf(buf, "C1=%3d%%  C2=%3d%%", c1 / 10, c2 / 10);
    line_text(3, buf);

    sprintf(buf, "C3=%3d%%", c3 / 10);
    line_text(4, buf);
}


















//消息
static void page_render_msg(void)
{
    char    buf[MSG_LEN];
    uint8_t len;

    if (msg_n == 0)
    {
        line_text(0, "4/4  MSG");
        line_text(3, "NO MESSAGE");
        return;
    }

    if (msg_view >= msg_n)
    {
        msg_view = 0;
    }

    len = (uint8_t)strlen(msg[msg_view]);

    sprintf(buf, "4/4  MSG %d/%d", msg_view + 1, msg_n);
    line_text(0, buf);

    /* 一条消息最多 31 个字符，分两行显示，每行 21 个字符 */
    strncpy(buf, msg[msg_view], 21);
    buf[21] = '\0';
    line_text(3, buf);

    if (len > 21)
    {
        strncpy(buf, msg[msg_view] + 21, 21);
        buf[21] = '\0';
        line_text(4, buf);
    }
}












/* ---------------- 6. 秒表和倒计时两个页面 ---------------- */

/* 5/6 秒表：从 0 往上加 */
static void page_render_sw(void)
{
    char buf[24];

    uint32_t ms      = stopwatch_ms();
    uint32_t total_s = ms / 1000;                    // 一共多少整秒
    uint32_t h       = total_s / 3600;               // 多少整小时
    uint32_t rest    = total_s - h * 3600;           // 扣掉小时，剩下多少秒
    uint32_t m       = rest / 60;                    // 剩下多少整分钟
    uint32_t s       = rest - m * 60;                // 再扣掉分钟，就是秒
    uint32_t cs      = (ms - total_s * 1000) / 10;   // 毫秒部分 -> 百分秒

    line_text(0, "5/6  STOPWATCH");

    if (stopwatch_running() == 1)
    {
        line_text(1, "RUN");
    }
    else
    {
        line_text(1, "PAUSE");
    }

    sprintf(buf, "%02d:%02d:%02d.%02d", (int)h, (int)m, (int)s, (int)cs);
    OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);
}

/* 6/6 倒计时：从 24 小时往下减 */
static void page_render_cd(void)
{
    char buf[24];

    uint32_t ms      = countdown_ms();
    uint32_t total_s = ms / 1000;
    uint32_t h       = total_s / 3600;
    uint32_t rest    = total_s - h * 3600;
    uint32_t m       = rest / 60;
    uint32_t s       = rest - m * 60;

    line_text(0, "6/6  COUNTDOWN");

    if (countdown_running() == 1)
    {
        line_text(1, "RUN");
    }
    else
    {
        line_text(1, "PAUSE");
    }

    sprintf(buf, "%02d:%02d:%02d", (int)h, (int)m, (int)s);
    OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);
}

static void page_render(void)
{
    OLED_NewFrame();                 // 清显存

    switch (page)
    {
        case PAGE_ADC:
            page_render_adc();
            break;

        case PAGE_MPU:
            page_render_mpu();
            break;

        case PAGE_LED:
            page_render_led();
            break;

        case PAGE_MSG:
            page_render_msg();
            break;

        case PAGE_SW:
            page_render_sw();
            break;

        case PAGE_CD:
            page_render_cd();
            break;

        default:
            page = PAGE_ADC;         // 兜底
            break;
    }

    OLED_ShowFrame();                // 整屏推给 OLED
}














void page_task(void)
{
    static uint32_t last = 0;

    
    if (page_next_flag)
    {
        page_next_flag = 0;
        switch (page)
        {
            case PAGE_ADC:  page = PAGE_MPU; break;
            case PAGE_MPU:  page = PAGE_LED; break;
            case PAGE_LED:  page = PAGE_MSG; break;
            case PAGE_MSG:  page = PAGE_SW;  break;
            case PAGE_SW:   page = PAGE_CD;  break;
            default:        page = PAGE_ADC; break;   // 最后一页再按 -> 回到第一页
        }
    }

    //每 500ms 采一次数据，刷新一屏
    if (HAL_GetTick() - last >= 500)
    {
        last = HAL_GetTick();

        MPU6050_ReadAll(&g_mpu);// 不管在哪一页都采，串口那边要用
        page_render();
    }
}
