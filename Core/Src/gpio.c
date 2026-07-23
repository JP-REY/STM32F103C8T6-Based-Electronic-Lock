/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_ON_BOARD_GPIO_Port, LED_ON_BOARD_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, MKB_COL1_Pin|MKB_COL2_Pin|MKB_COL3_Pin|MKB_COL4_Pin
                          |MKB_ROW1_Pin|MKB_ROW2_Pin|MKB_ROW3_Pin|MKB_ROW4_Pin
                          |STATUS_LED_BLUE_Pin|STATUS_LED_RED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LOAD_ACTUATOR_GPIO_Port, LOAD_ACTUATOR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_ON_BOARD_Pin */
  GPIO_InitStruct.Pin = LED_ON_BOARD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_ON_BOARD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : MKB_COL1_Pin MKB_COL2_Pin MKB_COL3_Pin MKB_COL4_Pin
                           MKB_ROW1_Pin MKB_ROW2_Pin MKB_ROW3_Pin MKB_ROW4_Pin
                           STATUS_LED_BLUE_Pin STATUS_LED_RED_Pin */
  GPIO_InitStruct.Pin = MKB_COL1_Pin|MKB_COL2_Pin|MKB_COL3_Pin|MKB_COL4_Pin
                          |MKB_ROW1_Pin|MKB_ROW2_Pin|MKB_ROW3_Pin|MKB_ROW4_Pin
                          |STATUS_LED_BLUE_Pin|STATUS_LED_RED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LOAD_ACTUATOR_Pin */
  GPIO_InitStruct.Pin = LOAD_ACTUATOR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LOAD_ACTUATOR_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
