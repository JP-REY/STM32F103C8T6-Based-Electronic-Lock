/**********************************************************************************************************************************
 * @file    Lock_Control_Service.h
 * @brief   Public interface of the Lock Control Service.
 *
 * @details The Lock Control Service is a synchronous, event-driven and hardware-independent domain module that owns the
 *          authoritative electronic-lock state machine and the consecutive authentication-failure policy.
 *
 *          The service receives semantic product events through LCS_Process(), selects at most one valid transition for the
 *          current state, evaluates its private guard, applies any private runtime effect and returns one LCS_Action_t for the
 *          application layer to execute.
 *
 *          Credential authentication is shared by unlock and credential-register authorization. The service records the pending
 *          request while credential entry is active and uses it to select the deterministic destination of a successful
 *          authentication: bounded unlock or the first-entry phase of credential registration.
 *
 *          Credential registration is modeled inside this FSM as first entry, confirmation entry, staging validation, persistent
 *          storage and bounded success feedback. A private mismatch counter is independent from the authentication-failure
 *          counter, so registration retries cannot consume or clear the access lockout policy.
 *
 *          Actions are semantic coordination requests rather than hardware commands. The application remains responsible for
 *          grouping the Credential Entry, Credential Register staging, Credential Storage, Authentication, Display Render,
 *          Status Indication, Sound Generator, timeout and actuator-control operations required to realize each returned action.
 *
 *          The service owns neither peripheral handles nor timestamps. Timeout owners report elapsed intervals by dispatching
 *          the corresponding timeout event. Events that are invalid for the current state are ignored without changing the
 *          runtime context and produce LCS_ACTION_NONE.
 *
 *          While locked, the application may use any debounced key press to produce
 *          LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED. That initiating key only wakes the user interface and enters credential-entry
 *          mode; it is not appended to the candidate credential. Credential digits are processed only after the entry session
 *          has begun.
 *
 *          The acronym LCS means Lock Control Service and is used as the public symbol prefix throughout this module.
 *
 * @note    The module owns one static singleton runtime instance. All public operations shall be called from one serialized
 *          execution context.
 *
 * @note    The module does not create tasks, use RTOS primitives, allocate dynamic memory, access STM32 peripherals or include
 *          HAL types.
 *
 * @note    LCS_EVENT_INIT_OK activates normal operation from the initial boot state. LCS_EVENT_INIT_FAIL enters the fault state
 *          and requests controlled reset behavior.
 *
 * @note    LCS_EVENT_CREDENTIAL_NOT_REGISTERED activates the service directly in the first credential-entry state. This startup
 *          route intentionally bypasses authentication because no installed credential exists yet.
 *
 * @author  Joao Pedro Rey
 * @version 1.2.0
 * @date    Aug 21, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_SERVICES_LOCK_CONTROL_INC_LOCK_CONTROL_SERVICE_H_
#define LIBS_SERVICES_LOCK_CONTROL_INC_LOCK_CONTROL_SERVICE_H_

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
 * @brief   Semantic event consumed by the Lock Control Service state machine.
 *
 * @details Events describe facts that have already been interpreted by the application or another domain service. They contain
 *          no GPIO levels, keyboard scan codes, peripheral handles or timestamps.
 *
 *          LCS_Process() consumes one event synchronously. If no transition matches both the current state and the supplied
 *          event, the service preserves its runtime context and returns LCS_ACTION_NONE.
 *
 * @note    App Core is the dispatch boundary for every LCS event. When a group identifies CES, AS, CRS, CSS or TVS as its origin,
 *          App Core translates that module's result into the corresponding LCS_Event_t; those modules do not call LCS_Process()
 *          directly.
 */
typedef enum
{
    /**
     * @brief Dispatch sentinel produced by App Core when no follow-up LCS event is available.
     */
    LCS_EVENT_NONE = 0U,                            /*< No semantic event is available; never causes a state transition.                    */

    /**
     * @brief Startup results produced by App Core after validating all critical application dependencies.
     */
    LCS_EVENT_INIT_OK,                              /*< Critical startup dependencies are valid; normal locked operation may begin.         */

    LCS_EVENT_INIT_FAIL,                            /*< Critical startup validation failed; the service shall enter its fault path.         */

    /**
     * @brief Credential Storage Service startup result translated and dispatched by App Core.
     */
    LCS_EVENT_CREDENTIAL_NOT_REGISTERED,            /*< No installed credential was found; initial credential registration is required.     */

    /**
     * @brief Registration lifecycle events produced by App Core from user intent and completed registration feedback.
     */
    LCS_EVENT_CREDENTIAL_REGISTER_REQUESTED,        /*< Reclassifies the active credential entry as authorization for registration.         */

    LCS_EVENT_CREDENTIAL_REGISTER_DONE,             /*< Registration completion feedback ended and normal locked operation may resume.      */

    /**
     * @brief Credential-entry wake event produced by App Core from a debounced Matrix Keyboard Driver key click.
     */
    LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED,           /*< A wake key requests credential-entry mode; the triggering key is not a digit.       */

    /**
     * @brief Credential Entry Service outcomes translated and dispatched by App Core.
     */
    LCS_EVENT_CREDENTIAL_CANCELLED,                 /*< The user cancelled the currently active credential-entry phase.                     */

    LCS_EVENT_CANDIDATE_READY,                      /*< A complete credential candidate is available for the current entry purpose.         */

    /**
     * @brief Authentication Service results translated and dispatched by App Core.
     */
    LCS_EVENT_AUTH_SUCCESS,                         /*< Authentication accepted the submitted candidate credential.                         */

    LCS_EVENT_AUTH_FAILURE,                         /*< Authentication rejected the submitted candidate credential.                         */

    /**
     * @brief Credential Register Service staging-comparison results translated and dispatched by App Core.
     */
    LCS_EVENT_STAGING_VALIDATION_SUCCESS,           /*< The confirmation candidate matches the staged first credential entry.               */

    LCS_EVENT_STAGING_VALIDATION_FAILURE,           /*< The confirmation candidate differs from the staged first credential entry.          */

    /**
     * @brief Credential Storage Service persistence results translated and dispatched by App Core.
     */
    LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_SUCCESS,  /*< The confirmed credential was persisted and verified successfully.                   */

    LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_FAILURE,  /*< The confirmed credential could not be persisted or verified.                        */

    /**
     * @brief Credential Entry Service incomplete-candidate outcome translated and dispatched by App Core.
     */
    LCS_EVENT_CANDIDATE_INCOMPLETE,                 /*< Confirmation was requested before the active candidate reached the required length. */

    /**
     * @brief Door Control Service outcomes translated and dispatched by App Core.
     */
    LCS_EVENT_EXIT_REQUEST,                         /*< A validated request-to-exit condition requests unlock from locked idle.             */

    LCS_EVENT_DOOR_POSITION_CONFIRMED,              /*< The required door-position condition was confirmed and relock timing may begin.     */

    LCS_EVENT_DOOR_POSITION_NOT_CONFIRMED,          /*< The door condition required to complete relocking is no longer confirmed.           */

    LCS_EVENT_READY_TO_LOCK,                        /*< Door-control conditions are satisfied and the lock mechanism may return to locked.  */

    /**
     * @brief Application action-execution results produced by App Executor and dispatched back to LCS.
     */

    LCS_EVENT_UNLOCK_REQUEST_FAILED,                /*< Unlock execution failed and the lock state must be reconciled safely.               */

    LCS_EVENT_CRITICAL_FAULT,                       /*< Unrecoverable runtime fault requiring controlled recovery or reset.                 */

    /**
     * @brief Elapsed-interval events dispatched by App Core after Timeout Validation Service evaluation.
     */
    LCS_EVENT_ENTRY_TIMEOUT,                        /*< The bounded credential-entry interval elapsed.                                      */

    LCS_EVENT_UNLOCK_HOLD_TIMEOUT,                  /*< The bounded interval awaiting door-position confirmation after unlock elapsed.      */
    
    LCS_EVENT_DOOR_SENSOR_CONFIRMATION_TIMEOUT,     /*< The bounded delay after door-position confirmation elapsed.                         */

    LCS_EVENT_DENIED_ACCESS_TIMEOUT,                /*< The bounded access-denied feedback interval elapsed.                                */

    LCS_EVENT_LOCKOUT_TIMEOUT,                      /*< The temporary authentication lockout interval elapsed.                              */

    /**
     * @brief Enumeration-size sentinel used for range validation; no module dispatches this value.
     */
    LCS_EVENT_COUNT                                 /*< Number of event identifiers; not a dispatchable event.                              */

}LCS_Event_t;

/**
 * @brief   Semantic application action selected by the Lock Control Service.
 *
 * @details An action communicates application intent without directly invoking another service or accessing hardware. The
 *          application interprets the returned value and coordinates all participating modules required by that product-level
 *          operation.
 *
 *          LCS_ACTION_NONE means that no application coordination is requested. It does not necessarily mean that no internal
 *          transition occurred; for example, successful boot activation currently enters the locked state without requesting
 *          an external action.
 */
typedef enum
{
    /**
     * @brief No-op action used when no application-level coordination is required.
     */
    LCS_ACTION_NONE = 0U,                                                   /*< No application-level work is requested.                                        */

    /**
     * @brief Actions that manage the first-entry phase of credential registration.
     */
    LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION,               /*< Begin collection of the first new credential entry.                            */

    LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION,             /*< Erase and restart the incomplete first credential entry.                       */

    LCS_ACTION_END_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION,                 /*< Abort first entry, clear transient registration data and restore locked idle.  */

    /**
     * @brief Actions that stage the first entry and manage the confirmation-entry phase.
     */
    LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_TO_CONFIRM_ENTRY_SESSION,  /*< Stage the first entry and begin collection of its confirmation entry.          */

    LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION,           /*< Erase and restart confirmation while retaining the staged first entry.         */

    LCS_ACTION_END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION,               /*< Abort confirmation, clear transient registration data and restore locked idle. */

    /**
     * @brief Actions that manage credential persistence and its completion feedback.
     */
    LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_SAVING_SESSION,                    /*< Prepare credential registration for persistent-storage coordination.           */

    LCS_ACTION_END_CREDENTIAL_REGISTER_SAVING_SESSION,                      /*< Complete persistence handling and begin bounded registration-success feedback. */

    /**
     * @brief Action that reclassifies active credential entry as registration authorization.
     */
    LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_TO_REGISTER_SESSION,                /*< Restart credential entry for authorization of a registration request.          */

    /**
     * @brief Actions that manage the normal credential-entry session lifecycle.
     */
    LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION,                              /*< Begin credential entry and its bounded inactivity interval.                    */

    LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_SESSION,                            /*< Erase the incomplete candidate and restart the active entry interval.          */

    LCS_ACTION_END_CREDENTIAL_ENTRY_SESSION,                                /*< End the active credential-entry session and restore locked idle.               */

    /**
     * @brief Action that delegates candidate verification to the Authentication Service.
     */
    LCS_ACTION_REQUEST_AUTHENTICATION,                                      /*< Submit the completed candidate for authentication and await its result.        */

    /**
     * @brief Actions that request credential-registration validation and persistent storage.
     */
    LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STAGES_VALIDATION,               /*< Compare the confirmation candidate with the staged first credential entry.     */

    LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STORAGE,                         /*< Request persistent storage of the validated new credential.                    */

    /**
     * @brief Actions that apply authenticated access, request-to-exit and temporary lockout policy.
     */
    LCS_ACTION_REQUEST_UNLOCK,                                              /*< Request physical unlock for successfully authenticated access.                 */

    LCS_ACTION_EXIT_REQUEST_UNLOCK,                                         /*< Request physical unlock for an accepted request-to-exit condition.             */

    LCS_ACTION_FORCE_ACTUATOR_LOCK,                                         /*< Force the actuator to ocked without applying the door-position interlock.      */
    
    LCS_ACTION_RESTART_UNLOCK_HOLD_TIMEOUT,                                 /*< Restart the bounded unlock-hold interval after relock confirmation is lost.    */
    
    LCS_ACTION_DENY_ACCESS,                                                 /*< Preserve the locked state and begin bounded access-denied feedback.            */

    LCS_ACTION_ENTER_LOCKOUT,                                               /*< Preserve the locked state, reject credential entry and begin lockout timing.   */

    /**
     * @brief Actions that restore locked operation or coordinate the post-unlock relock sequence.
     */
    LCS_ACTION_RETURN_TO_LOCKED,                                            /*< Restore normal locked-idle operation.                                          */

    LCS_ACTION_RETURN_TO_LOCKED_FROM_ENTRY_TIMEOUT,                         /*< Restore locked idle and request credential-entry timeout feedback.             */

    LCS_ACTION_RETURN_FROM_CREDENTIAL_REGISTER_SESSION_FIRST_BOOT,          /*< Complete mandatory first-boot registration by requesting a controlled reset.   */

    LCS_ACTION_RETURN_TO_LOCKED_FROM_CREDENTIAL_REGISTER_SESSION,           /*< Restore locked idle after registration completion feedback.                    */

    LCS_ACTION_BEGIN_DOOR_SENSOR_CONFIRMATION,                              /*< Begin the bounded interval following confirmation of the required door state.  */

    LCS_ACTION_REQUEST_DOOR_SENSOR_CONFIRMATION,                            /*< Request evaluation of the current door-control conditions for safe relocking.  */

    LCS_ACTION_RETURN_TO_LOCKED_FROM_GRANTED_ACCESS,                        /*< Request physical relock and restore locked idle after granted access.          */

    /**
     * @brief Fault-recovery action used when safe normal operation cannot continue.
     */
    LCS_ACTION_REQUEST_CONTROLLED_RESET                                     /*< Preserve safe outputs and request the application-controlled reset path.       */

}LCS_Action_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
LCS_Action_t LCS_Process(LCS_Event_t Event);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_SERVICES_LOCK_CONTROL_INC_LOCK_CONTROL_SERVICE_H_ */
