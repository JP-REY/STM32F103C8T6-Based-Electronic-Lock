/**********************************************************************************************************************************
 * @file    Door_Control_Service.h
 * @brief   Public interface of the Door Control Service.
 *
 * @details The Door Control Service is the application-facing coordination boundary for the Lock Actuator, Door Sensor and Exit
 *          Button components. It centralizes physical door-mechanism access while leaving product-state transitions in the Lock
 *          Control Service and hardware configuration in App Core and the Platform layer.
 *
 *          Door and exit inputs follow an interrupt-oriented, non-blocking execution model. STM32 EXTI callbacks notify the component
 *          drivers of an edge and return immediately. DCS_Update() later reads the Platform millisecond time once, runs both component
 *          debounce state machines and publishes at most one DoorSensor_Event_t and one ExitButton_Event_t through caller-owned output
 *          slots attached during initialization. The service never calls LCS_Process(); App Core translates published component events
 *          and synchronous DCS results into semantic Lock Control events.
 *
 *          A normal lock request is interlocked with the instantaneous normalized door-sensor level. In the current product mapping,
 *          DOOR_SENSOR_STATE_ACTIVE represents the contact condition in which locking is permitted. DCS_RequestLock() denies the
 *          command for IDLE or UNKNOWN. DCS_ForceLock() intentionally bypasses that policy and exists only for explicit fail-safe paths.
 *
 *          The service owns one private singleton runtime containing borrowed component handles and event-output pointers. App Core
 *          shall call DCS_AttachComponents() and DCS_AttachContext() after initializing the three components and before any request,
 *          status or update operation. All referenced objects must remain valid for the complete firmware lifetime.
 *
 *          Attachment parameters are intentionally opaque at the public boundary. The caller is responsible for supplying exactly the
 *          concrete handle and event types documented by each function. The service has no runtime type information and cannot detect a
 *          non-NULL pointer of the wrong concrete type.
 *
 * @note    Interrupt handlers shall call only the component NotifyInterrupt functions. They shall not call DCS_Update(), request an
 *          actuator command, read the door sensor, dispatch an LCS event or perform debounce work.
 *
 * @note    DCS_Update() is non-blocking and must execute periodically from the serialized application context at a cadence compatible
 *          with the configured component debounce intervals. Events are one-cycle outputs rather than queued messages.
 *
 * @note    The service exposes no readiness query. Successful attachment of both component and event contexts is a caller-maintained
 *          precondition for normal runtime use.
 *
 * @note    The module performs no dynamic allocation, creates no task and owns no STM32 HAL handle.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.1
 * @date    2026-08-26
 **********************************************************************************************************************************/

#ifndef LIBS_SERVICES_DOOR_CONTROL_INC_DOOR_CONTROL_SERVICE_H_
#define LIBS_SERVICES_DOOR_CONTROL_INC_DOOR_CONTROL_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   General execution status returned by Door Control Service operations.
 *
 * @details Reports whether attachment, direct actuator control or sensor-status translation completed successfully. Conditional
 *          normal locking uses DCS_RequestLockStatus_t so a safe policy denial remains distinguishable from an execution failure.
 */
typedef enum
{
    DCS_OPERATION_OK,   /*< The requested operation completed successfully.                  */

    DCS_OPERATION_FAIL  /*< An argument, dependency or delegated component operation failed. */

}DCS_OpStatus_t;

/**
 * @brief   Result of a door-position-aware normal lock request.
 *
 * @details Separates a valid policy denial caused by a non-active door contact from a Lock Actuator Driver failure. The service does
 *          not retain or retry a denied or failed command.
 */
typedef enum
{
    DCS_LOCK_REQUEST_APPROVED, /*< The sensor permitted locking and the actuator accepted the lock command.          */

    DCS_LOCK_REQUEST_DENIED,   /*< The current sensor state is IDLE or UNKNOWN; no actuator lock command was issued. */

    DCS_LOCK_REQUEST_FAILED    /*< The sensor permitted locking but the actuator lock command failed.                */

}DCS_RequestLockStatus_t;

/**
 * @brief   Service-level view of the instantaneous normalized door-sensor state.
 *
 * @details Mirrors the component's ACTIVE, IDLE and UNKNOWN electrical-policy states without exposing the Door Sensor Driver type
 *          through the public DCS interface. The application assigns the product meaning of ACTIVE through its configuration.
 *
 * @note    UNKNOWN is a valid status value, not a DCS execution failure by itself.
 */
typedef enum
{
    DCS_SENSOR_STATUS_ACTIVE,  /*< The configured active electrical contact level is present. */

    DCS_SENSOR_STATUS_IDLE,    /*< The inverse electrical contact level is present.           */

    DCS_SENSOR_STATUS_UNKNOWN  /*< A trustworthy current sensor level is unavailable.         */

}DCS_SensorStatus_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
DCS_OpStatus_t          DCS_AttachComponents (void* LockActuator_Context, void* DoorSensor_Context, void* ExitButton_Context);
DCS_OpStatus_t          DCS_AttachContext    (void* ExitButton_Event, void* DoorSensor_Event);
DCS_RequestLockStatus_t DCS_RequestLock      (void);
DCS_OpStatus_t          DCS_ForceLock        (void);
DCS_OpStatus_t          DCS_RequestUnlock    (void);
DCS_OpStatus_t          DCS_GetSensorStatus  (DCS_SensorStatus_t* Status);
void                    DCS_Update           (void);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_SERVICES_DOOR_CONTROL_INC_DOOR_CONTROL_SERVICE_H_ */