/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

//----------------------------------------定义全局变量--------------------------------------------//

// 按键事件：0无, 1短按, 2长按, 3双键同按
volatile uint8_t key_event = 0; //作为桥梁bro，明天记得跑步

// 按键1的变量
uint16_t key1_count = 0;
uint8_t key1_pressed = 0; // 是否已经确认按下
uint8_t key1_long_sent = 0; // 长按是否已经触发过

uint8_t key1_wait_double = 0;
uint16_t key1_double_count = 0;

// 按键2的变量
uint16_t key2_count = 0;
uint8_t key2_pressed = 0; 
uint8_t key2_long_sent = 0; 

uint8_t key2_wait_double = 0;
uint16_t key2_double_count = 0;

//主循环的变量
volatile uint8_t mode = 0;       // 0:慢闪 1:快闪 2:关闭
volatile uint8_t key_flag = 0;   // 按键触发的标志位（1表示被按下了）
uint32_t last_led_time = 0;      // 灯效时间戳

//插上电呼吸灯的变量
uint16_t pwm_val = 0;
uint8_t pwm_dir = 1;

//----------------------------------------定义全局变量--------------------------------------------//

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4) 
    {
        uint8_t p1 = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin); 
        uint8_t p2 = HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin); 

        // =========================================================
        // 最高优先级：双键同时按
        // =========================================================
        if (p1 == 0 && p2 == 0) 
        {
            if (key_event == 0) 
            {
                key_event = 3; 
                key1_long_sent = 1; 
                key2_long_sent = 1;
                key1_wait_double = 0; // 取消双击等待，直接出双键
                key2_wait_double = 0;
            }
            return; 
        }

        // =========================================================
        // 处理按键1
        // =========================================================
        if (p1 == 0) // 按下
        {
            // 如果在等待双击状态，又按下了！-> 触发双击
            if (key1_wait_double == 1) 
            {
                key_event = 4;       // 4代表双击
                key1_wait_double = 0;// 退出等待
                key1_long_sent = 1;  // 屏蔽此次按下产生的后续事件(比如长按)
            }

            key1_count++; 
            if (key1_count > 20) 
            {
                key1_pressed = 1; 
            }
            
            // 长按判定
            if (key1_count > 1000 && key1_long_sent == 0) 
            {
                key_event = 2; // 2代表长按
                key1_long_sent = 1; 
            }
        }
        else // 松开
        {
            if (key1_pressed == 1) 
            {
                if (key1_long_sent == 0) 
                {
                    // 正常松开，不直接触发短按，而是进入“等待双击”状态
                    key1_wait_double = 1;
                    key1_double_count = 0; // 清零双击倒计时
                }
            }
            key1_count = 0;
            key1_pressed = 0;
            // 注意：这里不清零 key1_long_sent，要等双击倒计时结束再清
        }

        // 双击倒计时逻辑
        if (key1_wait_double == 1) 
        {
            key1_double_count++;
            if (key1_double_count > 300) // 等待 300ms 没再按
            {
                key_event = 1;       // 1代表短按
                key1_wait_double = 0;// 退出等待
                key1_long_sent = 0;  // 恢复正常
            }
        }
        else if (p1 != 0) 
        {
            key1_long_sent = 0; // 彻底松手后清零长按标志
        }

        // =========================================================
        // 处理按键2 (逻辑和按键1一模一样)
        // =========================================================
        if (p2 == 0) 
        {
            if (key2_wait_double == 1) 
            {
                key_event = 4; 
                key2_wait_double = 0; 
                key2_long_sent = 1; 
            }

            key2_count++; 
            if (key2_count > 20) 
            {
                key2_pressed = 1; 
            }
            if (key2_count > 1000 && key2_long_sent == 0) 
            {
                key_event = 2; 
                key2_long_sent = 1; 
            }
        }
        else 
        {
            if (key2_pressed == 1) 
            {
                if (key2_long_sent == 0) 
                {
                    key2_wait_double = 1;
                    key2_double_count = 0; 
                }
            }
            key2_count = 0;
            key2_pressed = 0;
        }

        if (key2_wait_double == 1) 
        {
            key2_double_count++;
            if (key2_double_count > 300) 
            {
                key_event = 1; 
                key2_wait_double = 0; 
                key2_long_sent = 0; 
            }
        }
        else if (p2 != 0) 
        {
            key2_long_sent = 0; 
        }
			}
}
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
  
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_Base_Start_IT(&htim4);

  HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_3);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if (key_event != 0) 
      {
          uint8_t event = key_event; 
          key_event = 0;             
          
          if (event == 1) // 短按：切换模式
          {
              mode++; 
              if (mode > 3) 
              {
                mode = 0; 
              }
          }
          else if (event == 2) // 长按：关灯
          {
              mode = 3; 
          }
          else if (event == 3) // 双键同按：恢复呼吸灯
          {
              mode = 0; 
          }
          else if (event == 4) // 双击 
          {
              // 双击的功能由你定，比如：直接跳到模式2(快闪)，或者你自定义个模式4
              mode = 2; 
          }
          
          if (mode == 0) 
          {
             pwm_val = 0; 
             pwm_dir = 1; 
          }
      }

      // 2. 状态机
      uint32_t current_time = HAL_GetTick();

      switch (mode)
      {
          case 0: // 【新增：同步呼吸灯】
              
              if (current_time - last_led_time >= 20) 
              {
                  last_led_time = current_time;
                  
                  if (pwm_dir == 1) 
                  {    // 渐亮
                      pwm_val += 10; // 每次加 10
                      if (pwm_val >= 1000) 
                      { 
                        pwm_val = 1000; 
                        pwm_dir = 0; 
                      }
                  } 
                  else 
                  {    // 渐暗
                      pwm_val -= 10; // 每次减 10
                      if (pwm_val <= 0) 
                      { 
                        pwm_val = 0; 
                        pwm_dir = 1; 
                      }
                  }
                  
                  // 三个通道同时设置相同的占空比，实现“同步”呼吸
                  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_val);
                  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pwm_val);
                  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pwm_val);
              }
              break;

          case 1: // 慢闪 (用 PWM 控制，满占空比就是全亮)
              if (current_time - last_led_time >= 500) 
              {
                  last_led_time = current_time;
                  // 每次直接翻转状态
                  static uint8_t toggle = 0;
                  toggle = !toggle;
                  uint16_t val = toggle ? 1000 : 0; // 1000全亮，0全灭
                  
                  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, val);
                  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, val);
                  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, val);
              }
              break;

          case 2: // 【快闪】
              if (current_time - last_led_time >= 100) // 时间改短
              {
                  last_led_time = current_time;
                  static uint8_t toggle_fast = 0;
                  toggle_fast = !toggle_fast;
                  uint16_t val = toggle_fast ? 1000 : 0;
                  
                  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, val);
                  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, val);
                  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, val);
              }
              break;

          case 3: // 【关闭】
              __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
              __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
              __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
              break;
      }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
