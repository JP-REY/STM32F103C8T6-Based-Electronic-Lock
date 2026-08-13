/**********************************************************************************************************************************
 * @file    Time_Platform_Interface.c
 * @brief   This module implements a platform abstraction for millisecond and microsecond timing services.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Time_Platform_Interface.h"
#include "tim.h"
#include "stm32f4xx.h"

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
 Private Function Prototypes
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Generates a blocking delay in milliseconds.
 *
 * @details Waits until the specified number of milliseconds has elapsed using the HAL system tick counter.
 *          This function blocks the CPU during the entire delay period.
 *
 * @param   Delay - Delay duration in milliseconds.
 *
 * @note    Requires the HAL tick to be running correctly.
 *          This function should not be used in time-critical execution paths.
 *
 * @return  void
 */
void Platform_DelayMs(uint32_t Delay)
{
    uint32_t wait_time  = Delay;
    uint32_t tick_start = HAL_GetTick();

    while((HAL_GetTick() - tick_start) < wait_time){}
}

/**
 * @brief   Returns the current system time in milliseconds.
 *
 * @param   void
 *
 * @details Retrieves the current HAL system tick value.
 *
 * @return  Current system uptime in milliseconds.
 */
uint32_t Platform_GetMillis(void)
{
    return HAL_GetTick();
}

/**
 * @brief   Generates a blocking delay in microseconds.
 *
 * @details Waits until the specified number of microseconds has elapsed using the hardware timer configured
 *          as the platform time base.
 *
 * @param   Delay - Delay duration in microseconds.
 *
 * @note    Requires the timer used by Platform_GetMicros() to be running continuously with a resolution
 *          of one microsecond per counter increment.
 *
 *          This function performs a busy wait and blocks the CPU until completion.
 *
 * @return  void
 */
void Platform_DelayUs(uint32_t Delay)
{
    uint32_t wait_time  = Delay;
    uint32_t tick_start = Platform_GetMicros();

    while(Platform_GetMicros() - tick_start < wait_time){}
}

/**
 * @brief   Returns the current hardware timer counter value.
 *
 * @param   void
 *
 * @details Reads the current counter value from the hardware timer configured as the microsecond time base.
 *
 * @note    The returned value corresponds to the timer counter and will eventually overflow depending on the
 *          timer configuration.
 *
 * @return  Current timer counter value in microseconds.
 */
uint32_t Platform_GetMicros(void)
{
    return htim2.Instance->CNT;
}


























