#include "key.h"
#include "tim.h"        // 用到 htim3 和 __HAL_TIM_SET_COMPARE

 
 
    // key_scan_task()   
    // led_mode_task()  
    // led_mode_reset()  
 
    // 数据流：按键 -> key_event -> mode -> 三路 PWM 亮度
  



/* 为什么加 volatile：
   这两个变量会被"定时器中断"和"主循环"同时读写。
   加上 volatile 才会每次都真正去内存里读，而不是用寄存器里的旧副本。 */
volatile uint8_t key_event = ev_none;      // 最近一次按键事件（中断里写，主循环里读）
volatile uint8_t mode      = MODE_BREATH;  // 当前灯效模式（取值见 key.h）

// key1_count         这次按下已经持续了多少毫秒

// key1_long_sent     这次已经报过长按"了吗（避免按住不放重复报）"
// key1_wait_double   松手了，正在等 300ms 看有没有第二下
// key1_double_count  已经等了多少毫秒


static uint16_t key1_count        = 0;//计时器便量
static uint8_t  key1_pressed      = 0;//确定你真的按下了的变量
static uint8_t  key1_long_sent    = 0;
static uint8_t  key1_wait_double  = 0;
static uint16_t key1_double_count = 0;//等着你双击的变量
 

static uint16_t key2_count        = 0;
static uint8_t  key2_pressed      = 0;
static uint8_t  key2_long_sent    = 0;
static uint8_t  key2_wait_double  = 0;
static uint16_t key2_double_count = 0;


static uint8_t both_latched = 0;//防止呼吸灯被锁住


static uint32_t last_led_time = 0;  
static uint16_t pwm_val       = 0;   
static uint8_t  pwm_dir       = 1;   
static uint8_t  toggle_state  = 0;  
static uint8_t  flow_step     = 0;   // 流水灯走到第几颗
































   





//控制pwm
static void set_leds(uint16_t c1, uint16_t c2, uint16_t c3)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, c1);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, c2);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, c3);
}






















//按键扫描：定时器更新中断（1ms）
void key_scan_task(void)
{
    //按键是上拉
    uint8_t p1 = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);
    uint8_t p2 = HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin);























    //双键同时按下
    if (p1 == 0 && p2 == 0)
    {
        if (both_latched == 0)                 
        {
            both_latched = 1;//直接防止呼吸灯卡住

            if (key_event == ev_none)//1，2，3，4都是别的事件
            {
                key_event = ev_both;
            }
            key1_long_sent   = 1;              
            key2_long_sent   = 1;

            key1_wait_double = 0;             
            key2_wait_double = 0;

            key1_count       = 0;//防止双击还在计时
            key2_count       = 0;
        }
        return;                        
    }
    both_latched = 0;//松开后变回去

























    //状态机key1
    if (p1 == 0)
    {
        key1_count++;

        if (key1_count > 20) 
        {
            key1_pressed = 1;//确定按下了
        }

        if (key1_count > 20 && key1_wait_double == 1)//双击判定的地方
        {
            key_event        = ev_double;
            key1_wait_double = 0;//防止再次判定
            key1_long_sent   = 1;//防止触发下面长按代码
        }

        if (key1_count > 1000 && key1_long_sent == 0)
        {
            key_event      = ev_long;
            key1_long_sent = 1;//同理，先把末影龙打了，去拿个鞘翅
        }
    }   
    else//第一次按下循环是确定按下，松开后循环一直执行这里，将k_w_d设置为1，第三次循环再按下就是触发双击
    {
        if (key1_pressed == 1 && key1_long_sent == 0)
        {
            key1_wait_double  = 1;
            key1_double_count = 0;
        }
        key1_count   = 0;//忘记加这个短按直接失效，一直长按状态
        key1_pressed = 0;
    }
    //松开按键的时间里，在这里等这第二次按下
    if (key1_wait_double == 1)
    {
        key1_double_count++;
        if (key1_double_count > 300)//辜负了我300ms，是短按
        {
            key_event        = ev_short;
            key1_wait_double = 0;
            key1_long_sent   = 0;
        }
    }
    else if (p1 == 1)
    {
        key1_long_sent = 0;                   
    }
























    //状态机key2
    if (p2 == 0)
    {
        key2_count++;

        if (key2_count > 20)
        {
            key2_pressed = 1;
        }

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
           



















void led_mode_task(void)
{
    /* 第一步：有按键事件就切换模式。 */
    if (key_event != ev_none)
    {
        //DSH
        __disable_irq();
        uint8_t ev = key_event;
        key_event  = ev_none;
        __enable_irq();

        switch (ev)
        {
            case ev_short:
                mode = mode + 1;

                if (mode >= MODE_MAX)          // 5 种模式循环：呼吸->慢闪->快闪->流水->全灭->呼吸
                {
                    mode = MODE_BREATH;
                }
                break;

            case ev_long:
                mode = MODE_OFF;                   // 长按：全灭
                break;

            case ev_both:
                mode = MODE_BREATH;                // 双键：回呼吸
                break;

            case ev_double:
                mode = MODE_FLOW;                  // 双击：流水灯
                break;

            default:
                break;
        }

        led_mode_reset(); 
    }

    
    uint32_t now = HAL_GetTick();

    switch (mode)
    {
        case MODE_BREATH:                                   
            if (now - last_led_time >= 20)
            {
                last_led_time = now;

                if (pwm_dir)
                {
                    pwm_val += 10;

                    if (pwm_val >= 1000)
                    {
                        pwm_val = 1000;
                        pwm_dir = 0;
                    }
                }
                else
                {
                    if (pwm_val < 10)
                    {
                        pwm_val = 0;                    
                        pwm_dir = 1;
                    }
                    else
                    {
                        pwm_val -= 10;
                    }
                }

                set_leds(pwm_val, pwm_val, pwm_val);
            }
            break;

        case MODE_SLOW:                                    
            if (now - last_led_time >= 500)
            {
                last_led_time = now;
                toggle_state  = !toggle_state;

                uint16_t v = 0;

                if (toggle_state)
                {
                    v = 1000;
                }
                set_leds(v, v, v);
            }
            break;

        case MODE_FAST:
            if (now - last_led_time >= 100)
            {
                last_led_time = now;
                toggle_state  = !toggle_state;

                uint16_t v = 0;

                if (toggle_state)
                {
                    v = 1000;
                }
                set_leds(v, v, v);
            }
            break;

        case MODE_FLOW:                                    
            if (now - last_led_time >= 200)
            {
                last_led_time = now;

                if (flow_step == 0)
                {
                    set_leds(1000, 0, 0);
                }
                else if (flow_step == 1)
                {
                    set_leds(0, 1000, 0);
                }
                else
                {
                    set_leds(0, 0, 1000);
                }

                flow_step = flow_step + 1;

                if (flow_step > 2)
                {
                    flow_step = 0;      // 三颗灯轮流，转完回到第 0 颗
                }
            }
            break;

        case MODE_OFF:                                      
            if (now - last_led_time >= 100)
            {
                last_led_time = now;
                set_leds(0, 0, 0);
            }
            break;
    }
}

void led_mode_reset(void)
{
    flow_step = 0;                   

    if (mode == MODE_BREATH)         
    {
        pwm_val = 0;
        pwm_dir = 1;
    }
    else if (mode == MODE_OFF)        
    {
        set_leds(0, 0, 0);
    }
}
