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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define IS_UART1 0

#define BUF_SIZE 128
#define NAME_SIZE 32
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t rx3_byte;
uint8_t rx6_byte;

uint8_t kb_buf[BUF_SIZE];
volatile uint16_t kb_idx = 0;
volatile uint8_t kb_ready = 0;

uint8_t remote_buf[BUF_SIZE];
volatile uint16_t remote_idx = 0;
volatile uint8_t remote_ready = 0;

char my_name[NAME_SIZE];
char other_name[NAME_SIZE];

volatile uint8_t my_turn = 0;
volatile uint8_t chat_ended = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void send_str(UART_HandleTypeDef *huart, const char *s)
{
  HAL_UART_Transmit(huart, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

void send_line(UART_HandleTypeDef *huart, const char *s)
{
  send_str(huart, s);
  send_str(huart, "\r\n");
}

void read_line_from_keyboard(void)
{
  __disable_irq();
  kb_idx = 0;
  kb_ready = 0;
  __enable_irq();
  while (!kb_ready)
  {
  }
}

void wait_remote_line(void)
{
  __disable_irq();
  remote_idx = 0;
  remote_ready = 0;
  __enable_irq();
  while (!remote_ready && !chat_ended)
  {
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

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
  MX_USART3_UART_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart3, &rx3_byte, 1);

#if IS_UART1
  send_line(&huart3, "Man from U.A.R.T.1!");
#else
  send_line(&huart3, "Man from U.A.R.T.2!");
#endif
  send_line(&huart3, "Quit PRESS q");

#if IS_UART1
  send_str(&huart3, "    Name: ");
  read_line_from_keyboard();
  memcpy(my_name, kb_buf, kb_idx);
  my_name[kb_idx] = '\0';
  send_str(&huart3, "\r\n");

  send_str(&huart6, my_name);
  send_str(&huart6, "\r");

  HAL_UART_Receive_IT(&huart6, &rx6_byte, 1);
  wait_remote_line();
  memcpy(other_name, remote_buf, remote_idx);
  other_name[remote_idx] = '\0';

  char tmp[64];
  snprintf(tmp, sizeof(tmp), "    %s is ready", other_name);
  send_line(&huart3, tmp);

  my_turn = 1;
#else
  HAL_UART_Receive_IT(&huart6, &rx6_byte, 1);
  wait_remote_line();
  memcpy(other_name, remote_buf, remote_idx);
  other_name[remote_idx] = '\0';

  char tmp[64];
  snprintf(tmp, sizeof(tmp), "    %s is ready", other_name);
  send_line(&huart3, tmp);

  send_str(&huart3, "    Name: ");
  read_line_from_keyboard();
  memcpy(my_name, kb_buf, kb_idx);
  my_name[kb_idx] = '\0';
  send_str(&huart3, "\r\n");

  send_str(&huart6, my_name);
  send_str(&huart6, "\r");

  my_turn = 0;
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (chat_ended) break;

    if (my_turn)
    {
      char prompt[64];
      snprintf(prompt, sizeof(prompt), "    %s => ", my_name);
      send_str(&huart3, prompt);

      read_line_from_keyboard();
      send_str(&huart3, "\r\n");

      if (kb_idx == 1 && kb_buf[0] == 'q')
      {
        send_str(&huart6, "q\r");
        send_line(&huart3, "    Chat ended.");
        chat_ended = 1;
        break;
      }

      HAL_UART_Transmit(&huart6, kb_buf, kb_idx, HAL_MAX_DELAY);
      send_str(&huart6, "\r");

      my_turn = 0;
    }
    else
    {
      wait_remote_line();
      if (chat_ended) break;

      if (remote_idx == 1 && remote_buf[0] == 'q')
      {
        send_line(&huart3, "    Chat ended by other side.");
        chat_ended = 1;
        break;
      }

      char line[BUF_SIZE + NAME_SIZE + 16];
      remote_buf[remote_idx] = '\0';
      snprintf(line, sizeof(line), "    %s : %s", other_name, (char *)remote_buf);
      send_line(&huart3, line);

      my_turn = 1;
    }
  }

  while (1) {}
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 216;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3)
  {
    if (rx3_byte == '\r' || rx3_byte == '\n')
    {
      kb_ready = 1;
    }
    else if (rx3_byte == 0x08 || rx3_byte == 0x7F)
    {
      if (kb_idx > 0)
      {
        kb_idx--;
        uint8_t bs[] = {0x08, ' ', 0x08};
        HAL_UART_Transmit(&huart3, bs, 3, HAL_MAX_DELAY);
      }
    }
    else
    {
      HAL_UART_Transmit(&huart3, &rx3_byte, 1, HAL_MAX_DELAY);
      if (kb_idx < BUF_SIZE - 1)
      {
        kb_buf[kb_idx++] = rx3_byte;
      }
    }
    HAL_UART_Receive_IT(&huart3, &rx3_byte, 1);
  }
  else if (huart->Instance == USART6)
  {
    if (rx6_byte == '\r' || rx6_byte == '\n')
    {
      remote_ready = 1;
    }
    else
    {
      if (remote_idx < BUF_SIZE - 1)
      {
        remote_buf[remote_idx++] = rx6_byte;
      }
    }
    HAL_UART_Receive_IT(&huart6, &rx6_byte, 1);
  }
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

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
