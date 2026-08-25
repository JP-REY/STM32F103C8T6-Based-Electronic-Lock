/**********************************************************************************************************************************
 * @file    DoorSensor_Driver.h
 * @brief   Door-sensor digital input driver public interface.
 *
 * @details This module translates the electrical level read through a Platform
 *          GPIO handle into an active or idle logical sensor state. The active
 *          electrical level is selected when the driver instance is initialized,
 *          allowing the same API to support active-high and active-low sensors.
 *
 *          The driver does not configure the MCU pin, debounce the signal or
 *          assign physical meaning such as "door open" or "door closed" to the
 *          active state. Those responsibilities belong to the platform,
 *          application and hardware integration respectively.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    2026-08-25
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
 * @brief   Represents one door-sensor driver instance.
 *
 * @details The handle retains a caller-owned Platform GPIO descriptor, the
 *          configured active polarity and the initialization state.
 *
 * @warning Members are private driver data and shall not be read or modified
 *          directly by application code.
 */
typedef struct
{
                                            /*< Private data. Do not read or modify!                    */
    GPIO_Handle_t*           _gpio;         /*< Caller-owned Platform GPIO handle used for input reads. */
    DoorSensor_ActiveLevel_t _active_level; /*< Electrical level interpreted as the active state.       */
    bool                     _initialized;  /*< Indicates whether this driver instance was initialized. */

}DoorSensor_Handle_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
DoorSensor_OpStatus_t DoorSensor_Init     (DoorSensor_Handle_t* Device, GPIO_Handle_t* Context, DoorSensor_ActiveLevel_t Level);
DoorSensor_State_t    DoorSensor_GetState (DoorSensor_Handle_t* Device);

#ifdef __cplusplus
}
#endif

#endif /* DOORSENSOR_DRIVER_H */
