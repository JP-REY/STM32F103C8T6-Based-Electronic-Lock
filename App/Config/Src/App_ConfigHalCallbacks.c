/**********************************************************************************************************************************
 * @file    App_ConfigHalCallbacks.c
 * @brief   STM32 HAL callback bridge for application-owned interrupt inputs.
 *
 * @details Implements the strong HAL GPIO EXTI callback used by the CubeMX-generated interrupt handlers. The current bridge filters
 *          the PB10 request-to-exit line and publishes its timestamp to the application-owned Exit Button Driver instance. It does
 *          not debounce the input, call product services or command the lock from interrupt context.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-25
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "App_Config.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Publishes a request-to-exit GPIO edge to the Exit Button Driver.
 *
 * @details Ignores notifications until App_Init() binds a runtime registry and ignores every EXTI source other than EXIT_BUTTON_Pin.
 *          For PB10, captures the application millisecond time base and calls ExitButton_NotifyInterrupt(). The callback performs no
 *          GPIO read, delay, debounce evaluation, service dispatch or actuator command.
 *
 * @param   GPIO_Pin - HAL GPIO bit mask identifying the EXTI source.
 *
 * @note    Notifications received before ExitButton_Init() completes are safely ignored by the driver. Runtime consumption of the
 *          pending edge belongs to the planned Door Control Service.
 *
 * @return  void
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(App_Instance == NULL || App_Instance->Exit_Button == NULL)
    {
        return;
    }

    uint32_t now = Platform_GetMillis();

    if(GPIO_Pin == EXIT_BUTTON_Pin)
    {
        ExitButton_NotifyInterrupt(App_Instance->Exit_Button, now);
    }
}
