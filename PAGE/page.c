#include "page.h"
#include "oled.h"
#include "key.h"        // key_event / ev_short / mode
#include "my_adc.h"     // adc_get_voltage()
#include "mpu6050.h"    // g_mpu
#include "tim.h"        // htim3（读三路 PWM 的占空比）
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


















typedef struct
{
    uint8_t stable;   // 消抖后的稳定电平（1 = 按下）
    uint8_t cnt;      // 电平"变了"已经持续多少毫秒
} KeyDebounce;

/* 扫描一个按键：只在"刚刚稳定按下"的那一毫秒返回 1，其余返回 0 */
static uint8_t key_just_pressed(GPIO_TypeDef *port, uint16_t pin, KeyDebounce *k)
{
    uint8_t raw;

    raw = (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1 : 0;

    if (raw == k->stable)              // 电平没变，重新计时
    {
        k->cnt = 0;
        return 0;
    }

    k->cnt++;

    if (k->cnt < 20)                   // 还没稳定 20ms
    {
        return 0;
    }

    k->cnt    = 0;
    k->stable = raw;                   // 记下新状态

    if (raw == 1)                      // 稳定到"按下"
    {
        return 1;
    }

    return 0;
}

static KeyDebounce k3 = { 0, 0 };
static KeyDebounce k4 = { 0, 0 };
static KeyDebounce k5 = { 0, 0 };

static volatile uint8_t msg_up_flag   = 0;   // KEY4 按了一下 -> 上一条
static volatile uint8_t msg_down_flag = 0;   // KEY5 按了一下 -> 下一条
















//13 / 14 / 15 三个导航键，共用一个消抖函数
void page_key_scan(void)
{
    if (key_just_pressed(KEY3_GPIO_Port, KEY3_Pin, &k3))
    {
        page_next_flag = 1;
    }

    if (key_just_pressed(KEY4_GPIO_Port, KEY4_Pin, &k4))
    {
        msg_up_flag = 1;
    }

    if (key_just_pressed(KEY5_GPIO_Port, KEY5_Pin, &k5))
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

    msg_w = (msg_w + 1) % MSG_MAX;        // 存满 10 条后回头覆盖最旧的

    if (msg_n < MSG_MAX)
    {
        msg_n++;
    }

    msg_view = 0;                          // 来新消息就跳回第一条
}





















/* 消息页的按键：KEY4 翻上一条、KEY5 翻下一条。
   这两个键有自己的标志，不经过 key_event，所以不会和灯效按键冲突。 */
void msg_key_task(void)
{
    if (msg_up_flag)
    {
        msg_up_flag = 0;

        if (msg_n > 0)
        {
            if (msg_view == 0)
            {
                msg_view = (uint8_t)(msg_n - 1);    // 到头了绕回最后一条
            }
            else
            {
                msg_view--;
            }
        }
    }

    if (msg_down_flag)
    {
        msg_down_flag = 0;

        if (msg_n > 0)
        {
            msg_view = (uint8_t)((msg_view + 1) % msg_n);
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

    sprintf(buf, "mode%d %s", mode, mode_name[mode < 5 ? mode : 0]);
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
        page = (PageId)((page + 1) % PAGE_COUNT);
    }

    //每 500ms 采一次数据，刷新一屏
    if (HAL_GetTick() - last >= 500)
    {
        last = HAL_GetTick();

        MPU6050_ReadAll(&g_mpu);// 不管在哪一页都采，串口那边要用
        page_render();
    }
}
