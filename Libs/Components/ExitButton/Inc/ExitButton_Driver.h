/**********************************************************************************************************************************
 * @file    ExitButton_Driver.h
 * @brief   Interrupt-oriented exit button driver public interface.
 *
 * @details This module converts GPIO interrupt notifications into debounced
 *          press and release events. The interrupt-facing function only
 *          records the edge timestamp and sequence. GPIO sampling, polarity
 *          normalization and debounce validation are executed later by
 *          ExitButton_Update() in the application context.
 *
 *          The driver supports active-high and active-low buttons and performs
 *          no blocking delay. It reports button events only; authorization,
 *          actuator control and unlock timing remain application concerns.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-24
 **********************************************************************************************************************************/

#ifndef LIBS_COMPONENTS_EXITBUTTON_INC_EXITBUTTON_DRIVER_H_
#define LIBS_COMPONENTS_EXITBUTTON_INC_EXITBUTTON_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "GPIO_Platform_Interface.h"
#include "stdbool.h"
#include "stdint.h"

/**********************************************************************************************************************************
 Defines
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief Defines the result of an exit button driver operation.
 */
typedef enum
{
    EXIT_BUTTON_OPERATION_OK,
    EXIT_BUTTON_OPERATION_FAIL

}ExitButton_OpStatus_t;

/**
 * @brief Selects the electrical level that represents a pressed button.
 */
typedef enum
{
    EXIT_BUTTON_ACTIVE_LEVEL_HIGH,
    EXIT_BUTTON_ACTIVE_LEVEL_LOW

}ExitButton_ActiveLevel_t;

/**
 * @brief   Defines a debounced state-transition event.
 *
 * @details An event is reported once by the ExitButton_Update() call that
 *          validates the transition. It is not retained in an internal queue.
 */
typedef enum
{
    EXIT_BUTTON_EVENT_NONE,
    EXIT_BUTTON_EVENT_PRESS,
    EXIT_BUTTON_EVENT_RELEASE

}ExitButton_Event_t;

/**
 * @brief   Represents one interrupt-oriented exit button driver instance.
 *
 * @details The interrupt sequence and timestamp fields are volatile because
 *          they are written by ExitButton_NotifyInterrupt() in interrupt
 *          context and observed by ExitButton_Update() in application context.
 *
 * @warning Members are private driver data and shall not be read or modified
 *          directly by application code.
 */
typedef struct
{
                                                  /*< Private data. Do not read or modify!                           */
    GPIO_Handle_t*           _gpio;               /*< Caller-owned GPIO Platform handle used for button reads.       */
    ExitButton_ActiveLevel_t _active_level;       /*< Electrical level interpreted as a pressed button.              */
    bool                     _is_pressed;         /*< Most recently validated logical pressed state.                 */
    bool                     _state_is_known;     /*< Indicates whether a stable state has already been established. */
    uint32_t                 _debounce_time_ms;   /*< Required stability interval after the most recent interrupt.   */
    volatile uint32_t        _interrupt_sequence; /*< Sequence incremented for every received interrupt notification.*/
    volatile uint32_t        _interrupt_time_ms;  /*< Timestamp associated with the most recent interrupt.           */
    uint32_t                 _processed_sequence; /*< Most recent interrupt sequence processed by Update().          */
    bool                     _initialized;        /*< Indicates whether this driver instance was initialized.        */

}ExitButton_Handle_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
ExitButton_OpStatus_t ExitButton_Init            (ExitButton_Handle_t* Device, GPIO_Handle_t* Context, ExitButton_ActiveLevel_t Level, uint32_t DebounceTimeMs);
void                  ExitButton_NotifyInterrupt (ExitButton_Handle_t* Device, uint32_t TimestampMs);
ExitButton_OpStatus_t ExitButton_Update          (ExitButton_Handle_t* Device, uint32_t CurrentTimeMs, ExitButton_Event_t* Event);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_EXITBUTTON_INC_EXITBUTTON_DRIVER_H_ */
