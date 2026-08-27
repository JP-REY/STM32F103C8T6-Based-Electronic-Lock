/**********************************************************************************************************************************
 * @file    DoorSensor_Driver.h
 * @brief   Interrupt-oriented door-sensor driver public interface.
 *
 * @details Converts GPIO interrupt notifications into debounced active and idle transition events. The interrupt-facing function
 *          records only the edge timestamp and sequence. GPIO sampling, polarity normalization and debounce validation are executed
 *          later by DoorSensor_Update() in application context.
 *
 *          DoorSensor_GetState() remains available for an immediate normalized read when a safety decision requires the current
 *          contact level. The driver does not configure the MCU pin or assign physical meaning such as "door open" or "door closed".
 *
 * @author  Joao Pedro Rey
 * @version 1.2.0
 * @date    2026-08-26
 **********************************************************************************************************************************/

#ifndef DOORSENSOR_DRIVER_H
#define DOORSENSOR_DRIVER_H

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
 * @brief   Defines the result of a door-sensor driver operation.
 *
 * @details Initialization returns this type to report whether its parameters
 *          were accepted and the driver handle was configured.
 */
typedef enum
{
    DOOR_SENSOR_OPERATION_OK,
    DOOR_SENSOR_OPERATION_FAIL

}DoorSensor_OpStatus_t;

/**
 * @brief   Selects the electrical level that asserts the sensor.
 *
 * @details Active-high maps GPIO HIGH to DOOR_SENSOR_STATE_ACTIVE. Active-low
 *          maps GPIO LOW to DOOR_SENSOR_STATE_ACTIVE.
 */
typedef enum
{
    DOOR_SENSOR_ACTIVE_LEVEL_HIGH,
    DOOR_SENSOR_ACTIVE_LEVEL_LOW

}DoorSensor_ActiveLevel_t;

/**
 * @brief   Defines the normalized logical state reported by the sensor.
 *
 * @details ACTIVE means that the configured active electrical level is present;
 *          IDLE means that the opposite electrical level is present; UNKNOWN
 *          reports that the driver instance is invalid or not initialized. The
 *          application decides which physical door condition each state means.
 */
typedef enum
{
    DOOR_SENSOR_STATE_ACTIVE,
    DOOR_SENSOR_STATE_IDLE,
    DOOR_SENSOR_STATE_UNKNOWN

}DoorSensor_State_t;

/**
 * @brief   Defines one debounced door-sensor state-transition event.
 *
 * @details An event is reported once by the DoorSensor_Update() call that validates the transition. It is not retained in an
 *          internal queue.
 */
typedef enum
{
    DOOR_SENSOR_EVENT_NONE,
    DOOR_SENSOR_EVENT_ACTIVE,
    DOOR_SENSOR_EVENT_IDLE

}DoorSensor_Event_t;

/**
 * @brief   Represents one door-sensor driver instance.
 *
 * @details The interrupt sequence and timestamp are volatile because DoorSensor_NotifyInterrupt() writes them in interrupt context
 *          while DoorSensor_Update() observes them in application context.
 *
 * @warning Members are private driver data and shall not be read or modified
 *          directly by application code.
 */
typedef struct
{
                                                  /*< Private data. Do not read or modify!                           */
    GPIO_Handle_t*           _gpio;               /*< Caller-owned GPIO Platform handle used for sensor reads.       */
    DoorSensor_ActiveLevel_t _active_level;       /*< Electrical level interpreted as the active sensor state.       */
    bool                     _is_active;          /*< Most recently validated logical active state.                  */
    bool                     _state_is_known;     /*< Indicates whether a stable state has already been established. */
    uint32_t                 _debounce_time_ms;   /*< Required stability interval after the most recent interrupt.   */
    volatile uint32_t        _interrupt_sequence; /*< Sequence incremented for every received interrupt notification.*/
    volatile uint32_t        _interrupt_time_ms;  /*< Timestamp associated with the most recent interrupt.           */
    uint32_t                 _processed_sequence; /*< Most recent interrupt sequence processed by Update().          */
    bool                     _initialized;        /*< Indicates whether this driver instance was initialized.        */

}DoorSensor_Handle_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
DoorSensor_OpStatus_t DoorSensor_Init            (DoorSensor_Handle_t* Device, GPIO_Handle_t* Context, DoorSensor_ActiveLevel_t Level, uint32_t DebounceTimeMs);
DoorSensor_State_t    DoorSensor_GetState        (DoorSensor_Handle_t* Device);
void                  DoorSensor_NotifyInterrupt (DoorSensor_Handle_t* Device, uint32_t TimestampMs);
DoorSensor_OpStatus_t DoorSensor_Update          (DoorSensor_Handle_t* Device, uint32_t CurrentTimeMs, DoorSensor_Event_t* Event);

#ifdef __cplusplus
}
#endif

#endif /* DOORSENSOR_DRIVER_H */
