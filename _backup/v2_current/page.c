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

/* ---------------- 2. KEY3(PB13) 扫描 ---------------- */
/* 在 TIM4 的 1ms 中断里调用。PB13 配的是下拉，所以 1 = 按下。 */

static uint8_t key3_stable = 0;   // 消抖后的稳定电平
static uint8_t key3_cnt    = 0;   // 电平"变了"已经持续多少毫秒

void page_key_scan(void)
{
    uint8_t raw = HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin);

    if (raw == key3_stable)            // 电平没变，重新计时
    {
        key3_cnt = 0;
        return;
    }

    if (++key3_cnt < 20) return;       // 还没稳定 20ms，继续等

    key3_cnt    = 0;
    key3_stable = raw;                 // 记下新状态

    if (raw == 1) page_next_flag = 1;  // 只在"按下"那一下触发，松开不触发
}

/* ---------------- 3. 消息历史（最近 10 条）---------------- */

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
    if (msg_n < MSG_MAX) msg_n++;

    msg_view = 0;                          // 来新消息就跳回第一条
}

/* 消息页的按键：短按往下翻一条。
   这里读走并清掉 key_event，所以 led_mode_task 拿不到，不会顺带切换灯效。 */
void msg_key_task(void)
{
    uint8_t ev;

    if (key_event == ev_none) return;

    __disable_irq();
    ev = key_event;
    key_event = ev_none;
    __enable_irq();

    if (ev == ev_short && msg_n > 0)
        msg_view = (uint8_t)((msg_view + 1) % msg_n);
}

/* ---------------- 3b. 给串口用的查询 ---------------- */

/* 第 ch 号灯（1/2/3）的亮度百分比。
   TIM3 的 ARR = 999，CCR 范围 0~1000，除以 10 就是 0~100%。 */
uint16_t page_led_duty(uint8_t ch)
{
    if (ch == 1) return (uint16_t)(__HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_1) / 10);
    if (ch == 2) return (uint16_t)(__HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_2) / 10);
    return (uint16_t)(__HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_3) / 10);
}

/* "第几条/共几条 + 内容"，例如 "3/10 hello" */
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

/* ---------------- 4. 显示小工具 ---------------- */
/* 屏幕 128×64，用 8×6 小字体：一屏 8 行，每行 21 个字符。
   所有页面只做一件事 —— 往第 row 行写字。 */

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

/* ---------------- 5. 四个页面 ---------------- */

/* 1/4 电压 */
static void page_render_adc(void)
{
    float   v = adc_get_voltage();
    uint8_t w;

    line_text(0, "1/4  ADC");
    line_value(2, "V", v);

    w = (uint8_t)(v / 3.3f * 120.0f);     // 3.3V 对应满宽 120 像素
    if (w > 120) w = 120;
    OLED_DrawRectangle(4, 40, 120, 10, OLED_COLOR_NORMAL);
    OLED_DrawFilledRectangle(4, 40, w, 10, OLED_COLOR_NORMAL);
}

/* 2/4 六轴 */
static void page_render_mpu(void)
{
    line_text(0,  "2/4  MPU");
    line_value(1, "T",  g_mpu.temp_c);
    line_value(3, "Ax", g_mpu.ax_g);
    line_value(4, "Ay", g_mpu.ay_g);
    line_value(5, "Az", g_mpu.az_g);
}

/* 3/4 灯效亮度 */
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

/* 4/4 消息 */
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

    if (msg_view >= msg_n) msg_view = 0;
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

/* ---------------- 6. 按页号分派 ---------------- */

static void page_render(void)
{
    OLED_NewFrame();                 // 清显存

    switch (page)
    {
        case PAGE_ADC: page_render_adc(); break;
        case PAGE_MPU: page_render_mpu(); break;
        case PAGE_LED: page_render_led(); break;
        case PAGE_MSG: page_render_msg(); break;
        default:       page = PAGE_ADC;   break;   // 兜底
    }

    OLED_ShowFrame();                // 整屏推给 OLED
}

/* ---------------- 7. 任务入口 ---------------- */

void page_task(void)
{
    static uint32_t last = 0;

    /* 先看有没有切页请求 */
    if (page_next_flag)
    {
        page_next_flag = 0;
        page = (PageId)((page + 1) % PAGE_COUNT);
    }

    /* 每 500ms 采一次数据，刷新一屏 */
    if (HAL_GetTick() - last >= 500)
    {
        last = HAL_GetTick();
        MPU6050_ReadAll(&g_mpu);     // 不管在哪一页都采，串口那边要用
        page_render();
    }
}
