/**********************************************************************************************************************************
 * @file    Door_Control_Service.c
 * @brief   Door Control Service implementation.
 *
 * @details Owns the singleton composition context for the Lock Actuator, Door Sensor and Exit Button components. Actuator requests
 *          remain synchronous, while physical input processing follows a split interrupt/application-context model: EXTI callbacks
 *          publish edge metadata directly to the component drivers and DCS_Update() later performs non-blocking debounce evaluation.
 *
 *          The service deliberately returns component events through application-owned slots instead of dispatching Lock Control
 *          events itself. This preserves App Core as the only event-routing boundary and keeps interrupt handlers independent from
 *          product-state policy.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-26
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Door_Control_Service.h"
#include "DoorSensor_Driver.h"
#include "ExitButton_Driver.h"
#include "LockActuator_Driver.h"
#include "Time_Platform_Interface.h"
#include "stddef.h"
#include "stdint.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**
 * @brief   Borrowed component and output context retained by the singleton service.
 *
 * @details App Config owns every referenced object for the complete firmware lifetime. The service never allocates, replaces or
 *          releases those objects. Component handles receive synchronous commands, while the event pointers receive one-cycle output
 *          values from DCS_Update().
 */
typedef struct
{
    LockActuator_Handle_t* lock_actuator;     /*< Actuator component receiving synchronous lock and unlock commands.  */
    DoorSensor_Handle_t*   door_sensor;       /*< Interrupt-oriented door-contact component processed by Update().    */
    DoorSensor_Event_t*    door_sensor_event; /*< Caller-owned output slot overwritten by every sensor update.        */
    ExitButton_Handle_t*   exit_button;       /*< Interrupt-oriented request-to-exit component processed by Update(). */
    ExitButton_Event_t*    exit_button_event; /*< Caller-owned output slot overwritten by every button update.        */

}DCS_Handle_t;

/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/**
 * @brief   Singleton Door Control runtime.
 *
 * @details All pointers remain NULL until App Core completes the two attachment operations during initialization. Runtime calls are
 *          serialized by the application main loop; concurrent access is not supported.
 */
static DCS_Handle_t DCS_RuntimeInstance =
{
    .lock_actuator     = NULL,
    .door_sensor       = NULL,
    .door_sensor_event = NULL,
    .exit_button       = NULL,
    .exit_button_event = NULL
};

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
 * @brief   Attaches the initialized door-mechanism components.
 *
 * @details Validates all opaque pointers before committing them to the singleton. App Core supplies the concrete LockActuator_Handle_t,
 *          DoorSensor_Handle_t and ExitButton_Handle_t instances after their component initialization succeeds.
 *
 * @param   LockActuator_Context - Pointer to the application-owned Lock Actuator Driver handle.
 * @param   DoorSensor_Context   - Pointer to the application-owned Door Sensor Driver handle.
 * @param   ExitButton_Context   - Pointer to the application-owned Exit Button Driver handle.
 *
 * @return  DCS_OPERATION_OK   - All component pointers were attached.
 * @return  DCS_OPERATION_FAIL - At least one pointer was NULL; the previous context was preserved.
 */
DCS_OpStatus_t DCS_AttachComponents(void* LockActuator_Context, void* DoorSensor_Context, void* ExitButton_Context)
{
    if(LockActuator_Context == NULL ||
       DoorSensor_Context   == NULL ||
       ExitButton_Context   == NULL)
    {
        return DCS_OPERATION_FAIL;
    }

    DCS_RuntimeInstance.lock_actuator = LockActuator_Context;
    DCS_RuntimeInstance.door_sensor   = DoorSensor_Context;
    DCS_RuntimeInstance.exit_button   = ExitButton_Context;

    return DCS_OPERATION_OK;
}

/**
 * @brief   Attaches the caller-owned event-output slots used during periodic updates.
 *
 * @details Retains one ExitButton_Event_t pointer and one DoorSensor_Event_t pointer. Their component Update functions overwrite the
 *          slots on every DCS_Update() call, including resetting them to their respective NONE sentinel when no transition completes.
 *
 * @param   ExitButton_Event - Pointer to the application-owned Exit Button event slot.
 * @param   DoorSensor_Event - Pointer to the application-owned Door Sensor event slot.
 *
 * @return  DCS_OPERATION_OK   - Both event pointers were attached.
 * @return  DCS_OPERATION_FAIL - At least one pointer was NULL; the previous context was preserved.
 */
DCS_OpStatus_t DCS_AttachContext(void* ExitButton_Event, void* DoorSensor_Event)
{
    if(ExitButton_Event == NULL || DoorSensor_Event == NULL)
    {
        return DCS_OPERATION_FAIL;
    }

    DCS_RuntimeInstance.exit_button_event = ExitButton_Event;
    DCS_RuntimeInstance.door_sensor_event = DoorSensor_Event;

    return DCS_OPERATION_OK;
}

/**
 * @brief   Requests a door-position-aware normal lock command.
 *
 * @details Samples the current normalized sensor level synchronously. Only DOOR_SENSOR_STATE_ACTIVE permits the actuator command;
 *          IDLE and UNKNOWN are safe policy denials. The current App configuration assigns ACTIVE to the closed-contact condition.
 *
 * @return  DCS_LOCK_REQUEST_APPROVED - The sensor permitted locking and the actuator accepted the command.
 * @return  DCS_LOCK_REQUEST_DENIED   - The current sensor level does not permit normal locking.
 * @return  DCS_LOCK_REQUEST_FAILED   - The sensor permitted locking but the actuator command failed.
 */
DCS_RequestLockStatus_t DCS_RequestLock(void)
{
    DoorSensor_State_t status = DoorSensor_GetState(DCS_RuntimeInstance.door_sensor);

    if(status != DOOR_SENSOR_STATE_ACTIVE)
    {
        return DCS_LOCK_REQUEST_DENIED;
    }

    if(LockActuator_Lock(DCS_RuntimeInstance.lock_actuator) != LOCK_ACTUATOR_OPERATION_OK)
    {
        return DCS_LOCK_REQUEST_FAILED;
    }

    return DCS_LOCK_REQUEST_APPROVED;
}

/**
 * @brief   Commands the actuator to lock without evaluating the door-sensor interlock.
 *
 * @details Delegates directly to LockActuator_Lock(). This operation is distinct from DCS_RequestLock() so explicit fail-safe paths
 *          can request the configured safe actuator level independently from normal product policy.
 *
 * @return  DCS_OPERATION_OK   - The actuator accepted the lock command.
 * @return  DCS_OPERATION_FAIL - The actuator command failed.
 */
DCS_OpStatus_t DCS_ForceLock(void)
{
    if(LockActuator_Lock(DCS_RuntimeInstance.lock_actuator) != LOCK_ACTUATOR_OPERATION_OK)
    {
        return DCS_OPERATION_FAIL;
    }

    return DCS_OPERATION_OK;
}

/**
 * @brief   Commands the actuator to unlock.
 *
 * @details Delegates directly to LockActuator_Unlock(). Unlock authorization and product-state selection remain owned by LCS and App
 *          Core; DCS receives only the already-authorized actuator request.
 *
 * @return  DCS_OPERATION_OK   - The actuator accepted the unlock command.
 * @return  DCS_OPERATION_FAIL - The actuator command failed.
 */
DCS_OpStatus_t DCS_RequestUnlock(void)
{
    if(LockActuator_Unlock(DCS_RuntimeInstance.lock_actuator) != LOCK_ACTUATOR_OPERATION_OK)
    {
        return DCS_OPERATION_FAIL;
    }

    return DCS_OPERATION_OK;
}

/**
 * @brief   Reads the instantaneous normalized door-sensor status.
 *
 * @details Translates DoorSensor_State_t into the public DCS status type without applying debounce or consuming a pending event. This
 *          operation is intended for synchronous policy checks; interrupt-driven transition handling remains in DCS_Update().
 *
 * @param   Status - Destination receiving the translated sensor status.
 *
 * @return  DCS_OPERATION_OK   - Status received ACTIVE, IDLE or UNKNOWN.
 * @return  DCS_OPERATION_FAIL - Status was NULL.
 */
DCS_OpStatus_t DCS_GetSensorStatus(DCS_SensorStatus_t* Status)
{
    if(Status == NULL)
    {
        return DCS_OPERATION_FAIL;
    }

    DoorSensor_State_t status = DoorSensor_GetState(DCS_RuntimeInstance.door_sensor);

    switch(status)
    {
        case DOOR_SENSOR_STATE_ACTIVE:

            *Status = DCS_SENSOR_STATUS_ACTIVE;

        break;

        case DOOR_SENSOR_STATE_IDLE:

            *Status = DCS_SENSOR_STATUS_IDLE;

        break;

        case DOOR_SENSOR_STATE_UNKNOWN:
        default:

            *Status = DCS_SENSOR_STATUS_UNKNOWN;

        break;
    }

    return DCS_OPERATION_OK;
}

/**
 * @brief   Advances both interrupt-driven input components once in application context.
 *
 * @details Reads the Platform millisecond counter once so the exit button and door sensor evaluate pending edges against the same
 *          timestamp. Each Update function performs non-blocking debounce, samples its GPIO only after the quiet interval and writes
 *          a one-cycle event to its attached output slot. Return statuses are currently not promoted to a DCS fault event.
 *
 * @note    STM32 EXTI callbacks only call ExitButton_NotifyInterrupt() or DoorSensor_NotifyInterrupt(). They never call this function.
 */
void DCS_Update(void)
{
    uint32_t current_time_ms = Platform_GetMillis();

    (void)ExitButton_Update(DCS_RuntimeInstance.exit_button,
                            current_time_ms,
                            DCS_RuntimeInstance.exit_button_event);

    (void)DoorSensor_Update(DCS_RuntimeInstance.door_sensor,
                            current_time_ms,
                            DCS_RuntimeInstance.door_sensor_event);
}
