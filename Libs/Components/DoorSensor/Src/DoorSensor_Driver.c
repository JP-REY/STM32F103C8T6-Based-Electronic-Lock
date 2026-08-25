/**********************************************************************************************************************************
 * @file    DoorSensor_Driver.c
 * @brief   Polarity-independent digital door-sensor driver implementation.
 *
 * @details Stores a caller-owned GPIO Platform descriptor and translates its electrical level into active or idle sensor states.
 *          The driver performs no GPIO configuration, debounce, transition filtering or door-control policy.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    2026-08-25
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "DoorSensor_Driver.h"

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
/**
 * @brief   Checks whether a door-sensor instance is initialized.
 *
 * @param   Device - Pointer to the door-sensor handle.
 *
 * @return  true  - Whether Device is non-NULL and initialized.
 * @return  false - Whether Device is NULL or not initialized.
 */
static inline bool DoorSensor_IsInit(DoorSensor_Handle_t* Device)
{
    return Device == NULL ? false : Device->_initialized;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes a door-sensor driver instance.
 *
 * @details Retains the supplied Platform GPIO handle and the polarity used to
 *          normalize subsequent GPIO reads. GPIO hardware configuration remains
 *          the caller's responsibility.
 *
 * @param   Device  - Pointer to the door-sensor handle to initialize.
 * @param   Context - Pointer to the caller-owned Platform GPIO input handle.
 * @param   Level   - Electrical level that represents an active sensor.
 *
 * @return  DOOR_SENSOR_OPERATION_OK   - Whether initialization succeeds.
 * @return  DOOR_SENSOR_OPERATION_FAIL - Whether a pointer or Level is invalid.
 */
DoorSensor_OpStatus_t DoorSensor_Init(DoorSensor_Handle_t* Device, GPIO_Handle_t* Context, DoorSensor_ActiveLevel_t Level)
{
    if(Device == NULL || Context == NULL ||
      (Level != DOOR_SENSOR_ACTIVE_LEVEL_LOW &&
       Level != DOOR_SENSOR_ACTIVE_LEVEL_HIGH))
    {
        return DOOR_SENSOR_OPERATION_FAIL;
    }

    Device->_gpio         = Context;
    Device->_active_level = Level;
    Device->_initialized  = true;

    return DOOR_SENSOR_OPERATION_OK;
}

/**
 * @brief   Returns the current polarity-independent sensor state.
 *
 * @details Reads the GPIO on every call and maps the configured active level to
 *          DOOR_SENSOR_STATE_ACTIVE. The opposite level maps to
 *          DOOR_SENSOR_STATE_IDLE.
 *
 * @param   Device - Pointer to an initialized door-sensor handle.
 *
 * @return  DOOR_SENSOR_STATE_ACTIVE  - Whether the active electrical level is present.
 * @return  DOOR_SENSOR_STATE_IDLE    - Whether the opposite electrical level is present.
 * @return  DOOR_SENSOR_STATE_UNKNOWN - Whether Device is NULL or not initialized.
 *
 * @note    The current implementation treats every initialized read that does not equal the configured active level as IDLE. This
 *          includes GPIO_LEVEL_UNKNOWN and is retained as a documented limitation until the state-reading contract is hardened.
 */
DoorSensor_State_t DoorSensor_GetState(DoorSensor_Handle_t* Device)
{
    if(Device == NULL || !DoorSensor_IsInit(Device))
    {
        return DOOR_SENSOR_STATE_UNKNOWN;
    }

    GPIO_Level_t lock_state = PGPIO_GetLevel(Device->_gpio);

    if(Device->_active_level == DOOR_SENSOR_ACTIVE_LEVEL_LOW)
    {

        return lock_state == GPIO_LEVEL_LOW          ?
                             DOOR_SENSOR_STATE_ACTIVE:
                             DOOR_SENSOR_STATE_IDLE  ;
    }

    else
    {
        return lock_state == GPIO_LEVEL_HIGH         ?
                             DOOR_SENSOR_STATE_ACTIVE:
                             DOOR_SENSOR_STATE_IDLE  ;
    }
}
