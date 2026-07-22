/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_BOARD_Pin GPIO_PIN_13
#define LED_BOARD_GPIO_Port GPIOC
#define MKB_OUT1_PIN_Pin GPIO_PIN_0
#define MKB_OUT1_PIN_GPIO_Port GPIOA
#define MKB_OUT2_PIN_Pin GPIO_PIN_1
#define MKB_OUT2_PIN_GPIO_Port GPIOA
#define MKB_OUT3_PIN_Pin GPIO_PIN_2
#define MKB_OUT3_PIN_GPIO_Port GPIOA
#define MKB_OUT4_PIN_Pin GPIO_PIN_3
#define MKB_OUT4_PIN_GPIO_Port GPIOA
#define MKB_IN1_PIN_Pin GPIO_PIN_4
#define MKB_IN1_PIN_GPIO_Port GPIOA
#define MKB_IN2_PIN_Pin GPIO_PIN_5
#define MKB_IN2_PIN_GPIO_Port GPIOA
#define MKB_IN3_PIN_Pin GPIO_PIN_6
#define MKB_IN3_PIN_GPIO_Port GPIOA
#define MKB_IN4_PIN_Pin GPIO_PIN_7
#define MKB_IN4_PIN_GPIO_Port GPIOA
#define BLED_PIN_Pin GPIO_PIN_12
#define BLED_PIN_GPIO_Port GPIOA
#define RLED_PIN_Pin GPIO_PIN_15
#define RLED_PIN_GPIO_Port GPIOA
#define BUZZER_TIM3CH1_Pin GPIO_PIN_4
#define BUZZER_TIM3CH1_GPIO_Port GPIOB
#define LCD_BRIGHTNESS_TIM3CH2_Pin GPIO_PIN_5
#define LCD_BRIGHTNESS_TIM3CH2_GPIO_Port GPIOB
#define PCF8574_SCL_Pin GPIO_PIN_6
#define PCF8574_SCL_GPIO_Port GPIOB
#define PCF8574_SDA_Pin GPIO_PIN_7
#define PCF8574_SDA_GPIO_Port GPIOB
#define LOAD_ACT_PIN_Pin GPIO_PIN_8
#define LOAD_ACT_PIN_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
