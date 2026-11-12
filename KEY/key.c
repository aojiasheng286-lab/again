#include "key.h"
#include "tim.h" // 需要用到 htim3 和 __HAL_TIM_SET_COMPARE 宏

// ================= 1. 定义全局变量 =================
volatile uint8_t key_event = ev_none;
volatile uint8_t mode = 0; // 0:呼吸 1:慢闪 2:快闪 3:全灭 4:流水灯

// ================= 2. 私有变量 (加上 static) =================
// static 表示这些变量只能在这个 key.c 文件里用，防止和其他文件重名

// 按键1变量
static uint16_t key1_count = 0;
static uint8_t  key1_pressed = 0; 
static uint8_t  key1_long_sent = 0; 
static uint8_t  key1_wait_double = 0;  
static uint16_t key1_double_count = 0;

// 按键2变量
static uint16_t key2_count = 0;
static uint8_t  key2_pressed = 0; 
static uint8_t  key2_long_sent = 0; 
static uint8_t  key2_wait_double = 0;  
static uint16_t key2_double_count = 0;

// 灯效控制变量
static uint32_t last_led_time = 0;      
static uint16_t pwm_val = 0;            
static uint8_t  pwm_dir = 1;            
static uint8_t  toggle_state = 0;       
static uint8_t  flow_step = 0;          


// =========================================================
// 函数1：按键扫描 (1ms中断里调用)
// =========================================================
void key_scan_task(void)
{
    // 假设按键按下为低电平 0
    uint8_t p1 = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin); 
    uint8_t p2 = HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin); 

    // --- 双键同按最高优先级 ---
    if (p1 == 0 && p2 == 0) {
        if (key_event == ev_none) {
            key_event = ev_both; 
            key1_long_sent = 1; key2_long_sent = 1;
            key1_wait_double = 0; key2_wait_double = 0;
        }
        return; 
    }

    // --- 按键1 状态机 ---
    if (p1 == 0) {
        if (key1_wait_double == 1) {
            key_event = ev_double; 
            key1_wait_double = 0; key1_long_sent = 1; 
        }
        key1_count++; 
        if (key1_count > 20) key1_pressed = 1; 
        
        if (key1_count > 1000 && key1_long_sent == 0) {
            key_event = ev_long; key1_long_sent = 1; 
        }
    } else {
        if (key1_pressed == 1) {
            if (key1_long_sent == 0) {
                key1_wait_double = 1; key1_double_count = 0; 
            }
        }
        key1_count = 0; key1_pressed = 0;
    }
    
    if (key1_wait_double == 1) {
        key1_double_count++;
        if (key1_double_count > 300) {
            key_event = ev_short; key1_wait_double = 0; key1_long_sent = 0; 
        }
    } else if (p1 != 0) { key1_long_sent = 0; }

    // --- 按键2 状态机 ---
    if (p2 == 0) {
        if (key2_wait_double == 1) {
            key_event = ev_double; 
            key2_wait_double = 0; key2_long_sent = 1; 
        }
        key2_count++; 
        if (key2_count > 20) key2_pressed = 1; 
        
        if (key2_count > 1000 && key2_long_sent == 0) {
            key_event = ev_long; key2_long_sent = 1; 
        }
    } else {
        if (key2_pressed == 1) {
            if (key2_long_sent == 0) {
                key2_wait_double = 1; key2_double_count = 0; 
            }
        }
        key2_count = 0; key2_pressed = 0;
    }
    
    if (key2_wait_double == 1) {
        key2_double_count++;
        if (key2_double_count > 300) {
            key_event = ev_short; key2_wait_double = 0; key2_long_sent = 0; 
        }
    } else if (p2 != 0) { key2_long_sent = 0; }
}


// =========================================================
// 函数2：灯效状态机 (while(1) 里调用)
// =========================================================
void led_mode_task(void)
{
    // 1. 处理按键事件，切换模式
    if (key_event != ev_none) {
        uint8_t event = key_event; 
        key_event = ev_none; // 读完立刻清零
        
        if (event == ev_short) {
            mode++; if (mode > 4) mode = 0; 
            flow_step = 0; // 切换模式时重置流水灯
        }
        else if (event == ev_long)   { mode = 4; } // 长按全灭
        else if (event == ev_both)   { mode = 0; } // 双键同按回呼吸
        else if (event == ev_double) { mode = 3; } // 双击直接流水
        
        if (mode == 0) { pwm_val = 0; pwm_dir = 1; }
    }

    // 2. 根据 mode 执行灯效
    uint32_t current_time = HAL_GetTick();
    
    switch (mode) {
        case 0: // 【模式0：呼吸灯】
            if (current_time - last_led_time >= 20) {
                last_led_time = current_time;
                if (pwm_dir == 1) { 
                    pwm_val += 10; 
                    if (pwm_val >= 1000) { pwm_val = 1000; pwm_dir = 0; } 
                } else { 
                    pwm_val -= 10; 
                    if (pwm_val <= 0) { pwm_val = 0; pwm_dir = 1; } 
                }
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_val);
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pwm_val);
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pwm_val);
            }
            break;
            
        case 1: // 【模式1：慢闪】
            if (current_time - last_led_time >= 500) {
                last_led_time = current_time;
                toggle_state = !toggle_state; 
                uint16_t val = toggle_state ? 1000 : 0;
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, val);
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, val);
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, val);
            }
            break;
            
        case 2: // 【模式2：快闪】
            if (current_time - last_led_time >= 100) {
                last_led_time = current_time;
                toggle_state = !toggle_state; 
                uint16_t val = toggle_state ? 1000 : 0;
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, val);
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, val);
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, val);
            }
            break;
            
        case 3: // 【模式4：流水灯】
            if (current_time - last_led_time >= 200) {
                last_led_time = current_time;
                // 先全灭
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
                
                // 再点亮当前的一颗
                if (flow_step == 0)      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 1000);
                else if (flow_step == 1) __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 1000);
                else if (flow_step == 2) __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 1000);
                
                flow_step++; 
                if (flow_step > 2) flow_step = 0; 
            }
            break;

            case 4: // 【模式3：全灭】
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
            break;
    }
}