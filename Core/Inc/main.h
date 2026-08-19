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
#define LED_ON_BOARD_Pin GPIO_PIN_13
#define LED_ON_BOARD_GPIO_Port GPIOC
#define KEYBOARD_ROW_3_Pin GPIO_PIN_0
#define KEYBOARD_ROW_3_GPIO_Port GPIOA
#define KEYBOARD_ROW_3_EXTI_IRQn EXTI0_IRQn
#define KEYBOARD_ROW_2_Pin GPIO_PIN_1
#define KEYBOARD_ROW_2_GPIO_Port GPIOA
#define KEYBOARD_ROW_2_EXTI_IRQn EXTI1_IRQn
#define KEYBOARD_ROW_1_Pin GPIO_PIN_2
#define KEYBOARD_ROW_1_GPIO_Port GPIOA
#define KEYBOARD_ROW_1_EXTI_IRQn EXTI2_IRQn
#define KEYBOARD_ROW_0_Pin GPIO_PIN_3
#define KEYBOARD_ROW_0_GPIO_Port GPIOA
#define KEYBOARD_ROW_0_EXTI_IRQn EXTI3_IRQn
#define KEYBOARD_COL_3_Pin GPIO_PIN_4
#define KEYBOARD_COL_3_GPIO_Port GPIOA
#define KEYBOARD_COL_2_Pin GPIO_PIN_5
#define KEYBOARD_COL_2_GPIO_Port GPIOA
#define KEYBOARD_COL_1_Pin GPIO_PIN_6
#define KEYBOARD_COL_1_GPIO_Port GPIOA
#define KEYBOARD_COL_0_Pin GPIO_PIN_7
#define KEYBOARD_COL_0_GPIO_Port GPIOA
#define LOW_BATTERY_INDICATION_LED_Pin GPIO_PIN_12
#define LOW_BATTERY_INDICATION_LED_GPIO_Port GPIOA
#define LOCK_STATUS_INDICATION_LED_Pin GPIO_PIN_15
#define LOCK_STATUS_INDICATION_LED_GPIO_Port GPIOA
#define SOUND_FEEDBACK_BUZZER_Pin GPIO_PIN_4
#define SOUND_FEEDBACK_BUZZER_GPIO_Port GPIOB
#define LOCK_ACTUATOR_Pin GPIO_PIN_8
#define LOCK_ACTUATOR_GPIO_Port GPIOB
#define LCD_BACKLIGHT_Pin GPIO_PIN_9
#define LCD_BACKLIGHT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
