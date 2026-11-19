#include "page.h"
#include "oled.h"
#include "key.h"
#include "my_adc.h"
#include "mpu6050.h"
#include <stdio.h>
#include <string.h> 

// ==================== 页面状态 ====================
static volatile PageId  page           = PAGE_ADC;
static volatile uint8_t page_next_flag = 1;   // 切页请求，任意上下文都可以置 1

// ==================== PB13 按键：消抖 + 边沿 ====================
static uint8_t key3_stable = 0;   // 消抖后的稳定状态：0=松开，1=按下
static uint8_t key3_cnt    = 0;   // 读数与稳定状态不一致，持续了多少毫秒

void page_key_scan(void)
{
    uint8_t raw = HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin);   // 下拉：1 = 按下

    if (raw != key3_stable)          // 和稳定状态不一致 → 可能只是抖动
    {
        key3_cnt++;
        if (key3_cnt >= 20)          // 连续 20ms 都是这个值 → 认定真的变了
        {
            key3_cnt    = 0;
            key3_stable = raw;         
            {
                page_next_flag = 1;
            }
        }
    }
    else
    {
        key3_cnt = 0;
    }
}

void page_request_next(void)
{
    page_next_flag = 1;
}

PageId page_current(void)
{
    return page;
}

// ==================== 消息历史（10 条）====================
#define MSG_MAX  10
#define MSG_LEN  32

static char    msg[MSG_MAX][MSG_LEN];   // 10 × 32 字节
static uint8_t msg_w    = 0;            // 下一条写到哪一格
static uint8_t msg_n    = 0;            // 已经存了几条（最多 10）
static uint8_t msg_view = 0;            // 现在看第几条

// 存一条
void msg_push(const char *s)
{
    strncpy(msg[msg_w], s, MSG_LEN - 1);
    msg[msg_w][MSG_LEN - 1] = '\0';     // 保险：强制结尾

    msg_w = (msg_w + 1) % MSG_MAX;      // 写满就回到第 0 格
    if (msg_n < MSG_MAX) msg_n++;

    msg_view = 0;                       // 来新消息就跳回第一条
}

uint8_t msg_total(void)
{
    return msg_n;
}

// 消息页的按键处理：按一下就往下翻
void msg_key_task(void)
{
    uint8_t ev;

    if (key_event == ev_none) return;

    __disable_irq();
    ev = key_event;
    key_event = ev_none;                // 自己清掉，这样 led_mode_task 就收不到了
    __enable_irq();

    if (ev == ev_short && msg_n > 0)
    {
        msg_view = (uint8_t)((msg_view + 1) % msg_n);   // 翻下一条，到头回第一条
    }
}

static void page_render_msg(void)
{
    char buf[MSG_LEN];

    if (msg_n == 0)
    {
        OLED_PrintString(0, 24, "NO MSG", &font16x16, OLED_COLOR_NORMAL);
        return;
    }

    if (msg_view >= msg_n) msg_view = 0;

    // 第一行：第几条 / 共几条
    sprintf(buf, "%d/%d", msg_view + 1, msg_n);
    OLED_PrintASCIIString(0, 0, buf, &afont8x6, OLED_COLOR_NORMAL);

    // 第二行：消息内容（一行最多 21 个字符，多了截断）
    strncpy(buf, msg[msg_view], 21);
    buf[21] = '\0';
    OLED_PrintASCIIString(0, 20, buf, &afont8x6, OLED_COLOR_NORMAL);
}

// ==================== 各页面绘制 ====================
// 约定：每个 page_render_xxx() 只负责往显存里画，不改任何状态

static void page_render_adc(void)
{
    char buf[24];
    float v = adc_get_voltage();

    sprintf(buf, "2/3 ADC");
    OLED_PrintString(0, 0, buf, &font16x16, OLED_COLOR_NORMAL);

    sprintf(buf, "V:%.2f", v);
    OLED_PrintString(0, 20, buf, &font16x16, OLED_COLOR_NORMAL);

    // 一条电压进度条（按 3.3V 满量程算，方便一眼看出高低）
    uint8_t w = (uint8_t)(v / 3.3f * 120.0f);
    if (w > 120) w = 120;
    OLED_DrawRectangle(4, 44, 118, 10, OLED_COLOR_NORMAL);
    OLED_DrawFilledRectangle(4, 44, w, 10, OLED_COLOR_NORMAL);
}

static void page_render_mpu(void)
{
    char buf[24];

    sprintf(buf, "1/3 T:%.1f", g_mpu.temp_c);
    OLED_PrintString(0, 0, buf, &font16x16, OLED_COLOR_NORMAL);

    sprintf(buf, "Ax:%.2f", g_mpu.ax_g);
    OLED_PrintString(0, 16, buf, &font16x16, OLED_COLOR_NORMAL);

    sprintf(buf, "Ay:%.2f", g_mpu.ay_g);
    OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);

    sprintf(buf, "Az:%.2f", g_mpu.az_g);
    OLED_PrintString(0, 48, buf, &font16x16, OLED_COLOR_NORMAL);
}

// ==================== 分派：switch 状态机 ====================
static void page_render(void)
{
    OLED_NewFrame();                 // 1) 清显存

    switch (page)                    // 2) 按当前页号分派
    {
        case PAGE_ADC: page_render_adc(); break;
        case PAGE_MPU: page_render_mpu(); break;
        case PAGE_MSG: page_render_msg(); break;  
        default:       page = PAGE_ADC;   break;   // 兜底，防止状态跑飞
    }

    OLED_ShowFrame();                // 3) 整屏推给 OLED
}

// ==================== 任务入口 ====================
void page_task(void)
{
    // 1) 先处理切页请求
    if (page_next_flag)
    {
        page_next_flag = 0;
        page = (PageId)((page + 1) % PAGE_COUNT);
    }

    // 2) 每 500ms 采一次数据 + 刷一屏
    static uint32_t last = 0;
    if (HAL_GetTick() - last >= 500)
    {
        last = HAL_GetTick();

        MPU6050_ReadAll(&g_mpu);     // 不管在哪一页都采，保证串口那边数据是新的
        page_render();
    }
}
