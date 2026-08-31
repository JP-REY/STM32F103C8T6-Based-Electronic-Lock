/**********************************************************************************************************************************
 * @file    App_ConfigHalCallbacks.c
 * @brief   STM32 HAL EXTI callback bridge for application-owned interrupt inputs.
 *
 * @details Implements the strong HAL_GPIO_EXTI_Callback() used by the CubeMX-generated EXTI interrupt path. The bridge identifies the
 *          application-owned Exit Button and Door Sensor lines, captures the current application timestamp and forwards the interrupt
 *          notification to the corresponding component driver.
 *
 *          The callback performs only bounded interrupt handoff. It does not sample GPIO state, execute debounce processing, update
 *          services, dispatch Lock Control events or command the lock actuator from interrupt context. Stable input evaluation and
 *          product-level event translation are deferred to the serialized application runtime through the Door Control Service.
 *
 * @note    The callback may observe an EXTI edge before the corresponding component driver has completed initialization. Driver
 *          NotifyInterrupt() contracts shall therefore reject or ignore premature notifications safely.
 *
 * @note    This module is part of the App composition boundary and shall not contain product-state policy or general runtime
 *          orchestration.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    Aug 30, 2026
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
