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
volatile uint8_t mode = 0;       // 0:慢闪 1:快闪 2:关闭
volatile uint8_t key_flag = 0;   // 按键触发的标志位（1表示被按下了）
uint32_t last_led_time = 0;      // 灯效时间戳

uint16_t pwm_val = 0;
uint8_t pwm_dir = 1;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
 // 这里的 TIM4 改成你实际用来做 1ms 中断的定时器
    if (htim->Instance == TIM4) 
    {
        static uint8_t key_state = 0; // 0:松开, 1:消抖中, 2:已按下
        static uint16_t key_count = 0; 
        
        uint8_t current_pin = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);

        if (current_pin == GPIO_PIN_RESET) // 检测到低电平（按下）
        {
            if (key_state == 0) // 刚按下
            {
                key_state = 1; 
                key_count = 0;
            }
            else if (key_state == 1) // 消抖中
            {
                key_count++;
                if (key_count >= 5) // 连续 5 次检测到低电平（即 5ms）
                {
                    key_state = 2; // 确认按下
                    key_flag = 1;  // ★★★ 告诉主循环：按键按下了！ ★★★
                }
            }
        }
        else // 松开
        {
            key_state = 0; 
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
        // 1. 处理按键
      if (key_flag == 1) 
      {
          key_flag = 0; 
          mode++; 
          if (mode > 3) 
           {
            mode = 0; // 0,1,2,3 循环
           }
          // 切换模式时重置一下呼吸灯状态
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
              // 每 20ms 改变一次亮度，20ms * 1000 = 20秒一个完整的呼吸周期
              if (current_time - last_led_time >= 20) 
              {
                  last_led_time = current_time;
                  
                  if (pwm_dir == 1) 
                  { // 渐亮
                      pwm_val += 10; // 每次加 10
                      if (pwm_val >= 1000) 
                      { 
                        pwm_val = 1000; 
                        pwm_dir = 0; 
                      }
                  } 
                  else 
                  {            // 渐暗
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
