/**********************************************************************************************************************************
 * @file    Time_Platform_Interface.h
 * @brief   Platform abstraction for millisecond and microsecond timing services.
 *
 * @details Provides a hardware abstraction layer for time-related operations used by higher software layers.
 *          This module exposes blocking delay functions and system time retrieval functions while hiding the
 *          underlying timer implementation.
 *
 *          Millisecond services are based on the HAL system tick.
 *          Microsecond services are based on a hardware timer configured by the platform.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/

#ifndef INC_TIME_PLATFORM_INTERFACE_H_
#define INC_TIME_PLATFORM_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
void     Platform_DelayMs   (uint32_t Delay);
uint32_t Platform_GetMillis ();
void     Platform_DelayUs   (uint32_t Delay);
uint32_t Platform_GetMicros ();


#ifdef __cplusplus
}
#endif

#endif /* INC_TIME_PLATFORM_INTERFACE_H_ */
