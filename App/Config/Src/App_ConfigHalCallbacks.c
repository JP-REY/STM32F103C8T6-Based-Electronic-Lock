/**********************************************************************************************************************************
 * @file    App_ConfigHalCallbacks.c
 * @brief   STM32 HAL callback bridge for application-owned interrupt inputs.
 *
 * @details Implements the strong HAL GPIO EXTI callback used by the CubeMX-generated interrupt handlers. The bridge filters the exit
 *          button and door-sensor lines and publishes their timestamps to the corresponding application-owned driver instances. It
 *          does not sample or debounce either input, call product services or command the lock from interrupt context.
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
 * @brief   Publishes application-owned GPIO edges to their interrupt-oriented drivers.
 *
 * @details Ignores notifications until App_Init() binds a runtime registry. For the exit button and door sensor, captures the common
 *          application millisecond time base and calls the respective NotifyInterrupt function. The callback performs no GPIO read,
 *          delay, debounce evaluation, service dispatch or actuator command.
 *
 * @param   GPIO_Pin - HAL GPIO bit mask identifying the EXTI source.
 *
 * @note    Notifications received before the corresponding driver initialization completes are safely ignored. Runtime consumption
 *          of pending edges belongs to the Door Control Service.
 *
 * @return  void
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(App_Instance == NULL)
    {
        return;
    }

    uint32_t now = Platform_GetMillis();

    if(GPIO_Pin == EXIT_BUTTON_Pin && App_Instance->Exit_Button != NULL)
    {
        ExitButton_NotifyInterrupt(App_Instance->Exit_Button, now);
    }

    else if(GPIO_Pin == DOOR_SENSOR_Pin && App_Instance->Door_Sensor != NULL)
    {
        DoorSensor_NotifyInterrupt(App_Instance->Door_Sensor, now);
    }
}
