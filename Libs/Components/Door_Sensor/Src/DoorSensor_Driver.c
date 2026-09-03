/**********************************************************************************************************************************
 * @file    DoorSensor_Driver.c
 * @brief   Interrupt-oriented, non-blocking door-sensor driver implementation.
 *
 * @details Separates interrupt-facing edge notification from application-context GPIO sampling and debounce evaluation. The driver
 *          reports active and idle transitions but owns no physical door-position policy or actuator command.
 *
 * @author  Joao Pedro Rey
 * @version 1.2.0
 * @date    2026-08-26
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "DoorSensor_Driver.h"
#include "stddef.h"

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
static bool                  DoorSensor_IsInitialized  (const DoorSensor_Handle_t* Device);
static DoorSensor_OpStatus_t DoorSensor_NormalizeLevel (const DoorSensor_Handle_t* Device, GPIO_Level_t Level, bool* IsActive);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Checks whether a door-sensor driver instance is initialized.
 *
 * @param   Device - Pointer to the door-sensor handle.
 *
 * @return  true  - Whether Device is non-NULL and initialized.
 * @return  false - Whether Device is NULL or not initialized.
 */
static bool DoorSensor_IsInitialized(const DoorSensor_Handle_t* Device)
{
    return Device == NULL ? false : Device->_initialized;
}

/**
 * @brief   Converts one electrical GPIO level into a logical sensor state.
 *
 * @param   Device   - Pointer to an initialized door-sensor handle.
 * @param   Level    - Electrical GPIO level to normalize.
 * @param   IsActive - Pointer receiving the normalized logical level.
 *
 * @return  DOOR_SENSOR_OPERATION_OK   - Whether Level was normalized.
 * @return  DOOR_SENSOR_OPERATION_FAIL - Whether Level or IsActive is invalid.
 */
static DoorSensor_OpStatus_t DoorSensor_NormalizeLevel(const DoorSensor_Handle_t* Device, GPIO_Level_t Level, bool* IsActive)
{
    if(IsActive == NULL || (Level != GPIO_LEVEL_LOW && Level != GPIO_LEVEL_HIGH))
    {
        return DOOR_SENSOR_OPERATION_FAIL;
    }

    if(Device->_active_level == DOOR_SENSOR_ACTIVE_LEVEL_LOW)
    {
        *IsActive = Level == GPIO_LEVEL_LOW;
    }
    else
    {
        *IsActive = Level == GPIO_LEVEL_HIGH;
    }

    return DOOR_SENSOR_OPERATION_OK;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes an interrupt-oriented door-sensor instance.
 *
 * @details Retains the supplied GPIO Platform handle, active electrical level and debounce interval. Runtime state and interrupt
 *          sequencing are reset without reading the GPIO or generating an event.
 *
 * @param   Device         - Pointer to the door-sensor handle to initialize.
 * @param   Context        - Pointer to the caller-owned GPIO Platform input handle.
 * @param   Level          - Electrical level that represents an active sensor.
 * @param   DebounceTimeMs - Required input stability interval in milliseconds.
 *
 * @return  DOOR_SENSOR_OPERATION_OK   - Whether initialization succeeds.
 * @return  DOOR_SENSOR_OPERATION_FAIL - Whether a parameter is invalid.
 */
DoorSensor_OpStatus_t DoorSensor_Init(DoorSensor_Handle_t* Device, GPIO_Handle_t* Context, DoorSensor_ActiveLevel_t Level, uint32_t DebounceTimeMs)
{
    if(Device == NULL || Context == NULL || DebounceTimeMs == 0U ||
      (Level != DOOR_SENSOR_ACTIVE_LEVEL_LOW &&
       Level != DOOR_SENSOR_ACTIVE_LEVEL_HIGH))
    {
        return DOOR_SENSOR_OPERATION_FAIL;
    }

    Device->_gpio                = Context;
    Device->_active_level        = Level;
    Device->_is_active           = false;
    Device->_state_is_known      = false;
    Device->_debounce_time_ms    = DebounceTimeMs;
    Device->_interrupt_sequence  = 0U;
    Device->_interrupt_time_ms   = 0U;
    Device->_processed_sequence  = 0U;
    Device->_initialized         = true;

    return DOOR_SENSOR_OPERATION_OK;
}

/**
 * @brief   Returns the current polarity-independent sensor state.
 *
 * @details Reads the GPIO immediately and maps its electrical level to DOOR_SENSOR_STATE_ACTIVE or DOOR_SENSOR_STATE_IDLE.
 *
 * @param   Device - Pointer to an initialized door-sensor handle.
 *
 * @return  DOOR_SENSOR_STATE_ACTIVE  - Whether the active electrical level is present.
 * @return  DOOR_SENSOR_STATE_IDLE    - Whether the opposite electrical level is present.
 * @return  DOOR_SENSOR_STATE_UNKNOWN - Whether the instance or GPIO level is invalid.
 */
DoorSensor_State_t DoorSensor_GetState(DoorSensor_Handle_t* Device)
{
    if(!DoorSensor_IsInitialized(Device))
    {
        return DOOR_SENSOR_STATE_UNKNOWN;
    }

    GPIO_Level_t raw_level = PGPIO_GetLevel(Device->_gpio);
    bool         is_active = false;

    if(DoorSensor_NormalizeLevel(Device, raw_level, &is_active) != DOOR_SENSOR_OPERATION_OK)
    {
        return DOOR_SENSOR_STATE_UNKNOWN;
    }

    return is_active ? DOOR_SENSOR_STATE_ACTIVE : DOOR_SENSOR_STATE_IDLE;
}

/**
 * @brief   Records an interrupt edge and restarts the debounce interval.
 *
 * @details Timestamp is written before the sequence increment. The application processor observes the sequence as the publication
 *          marker and verifies it again around GPIO sampling so an edge arriving during processing restarts validation.
 *
 * @param   Device      - Pointer to an initialized door-sensor handle.
 * @param   TimestampMs - Millisecond timestamp associated with the edge.
 */
void DoorSensor_NotifyInterrupt(DoorSensor_Handle_t* Device, uint32_t TimestampMs)
{
    if(!DoorSensor_IsInitialized(Device))
    {
        return;
    }

    Device->_interrupt_time_ms = TimestampMs;
    Device->_interrupt_sequence++;
}

/**
 * @brief   Processes pending interrupt activity and validates one stable sensor state.
 *
 * @details The newest interrupt sequence is processed only after the configured interval has elapsed since its timestamp. If a new
 *          interrupt arrives during processing, the current sample is discarded and debounce restarts from the newer edge.
 *
 * @param   Device        - Pointer to an initialized door-sensor handle.
 * @param   CurrentTimeMs - Current monotonic millisecond timestamp.
 * @param   Event         - Pointer receiving the validated transition event.
 *
 * @return  DOOR_SENSOR_OPERATION_OK   - Whether processing completes normally.
 * @return  DOOR_SENSOR_OPERATION_FAIL - Whether validation or GPIO reading fails.
 */
DoorSensor_OpStatus_t DoorSensor_Update(DoorSensor_Handle_t* Device, uint32_t CurrentTimeMs, DoorSensor_Event_t* Event)
{
    if(Event == NULL)
    {
        return DOOR_SENSOR_OPERATION_FAIL;
    }

    *Event = DOOR_SENSOR_EVENT_NONE;

    if(!DoorSensor_IsInitialized(Device))
    {
        return DOOR_SENSOR_OPERATION_FAIL;
    }

    uint32_t interrupt_sequence = Device->_interrupt_sequence;

    if(interrupt_sequence == Device->_processed_sequence)
    {
        return DOOR_SENSOR_OPERATION_OK;
    }

    uint32_t interrupt_time_ms = Device->_interrupt_time_ms;

    if(interrupt_sequence != Device->_interrupt_sequence)
    {
        return DOOR_SENSOR_OPERATION_OK;
    }

    if((uint32_t)(CurrentTimeMs - interrupt_time_ms) < Device->_debounce_time_ms)
    {
        return DOOR_SENSOR_OPERATION_OK;
    }

    GPIO_Level_t raw_level = PGPIO_GetLevel(Device->_gpio);
    bool         is_active = false;

    if(DoorSensor_NormalizeLevel(Device, raw_level, &is_active) != DOOR_SENSOR_OPERATION_OK)
    {
        return DOOR_SENSOR_OPERATION_FAIL;
    }

    if(interrupt_sequence != Device->_interrupt_sequence)
    {
        return DOOR_SENSOR_OPERATION_OK;
    }

    Device->_processed_sequence = interrupt_sequence;

    if(Device->_state_is_known && is_active == Device->_is_active)
    {
        return DOOR_SENSOR_OPERATION_OK;
    }

    Device->_is_active      = is_active;
    Device->_state_is_known = true;

    *Event = is_active ? DOOR_SENSOR_EVENT_ACTIVE : DOOR_SENSOR_EVENT_IDLE;

    return DOOR_SENSOR_OPERATION_OK;
}
