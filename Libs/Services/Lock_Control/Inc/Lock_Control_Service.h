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
 *          Actions are semantic coordination requests rather than hardware commands. The application remains responsible for
 *          grouping the Credential Entry, Authentication, Display Render, Status Indication, Sound Generator, timeout and
 *          actuator-control operations required to realize each returned action.
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
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 18, 2026
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
 */
typedef enum
{
    LCS_EVENT_NONE = 0U,                  /*< No semantic event is available; never causes a state transition.                 */

    LCS_EVENT_INIT_OK,                    /*< Critical startup dependencies are valid; normal operation may begin.             */

    LCS_EVENT_INIT_FAIL,                  /*< Startup validation failed; the service enters its fault path.                    */

    LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED, /*< A wake key requests credential-entry mode; the key is not a credential digit.    */

    LCS_EVENT_CREDENTIAL_CANCELLED,       /*< The active credential-entry session was cancelled while its candidate was empty. */

    LCS_EVENT_CANDIDATE_READY,            /*< A complete candidate credential is ready for authentication.                     */

    LCS_EVENT_AUTH_SUCCESS,               /*< Authentication accepted the candidate credential.                                */

    LCS_EVENT_AUTH_FAILURE,               /*< Authentication rejected the candidate credential.                                */

    LCS_EVENT_ENTRY_TIMEOUT,              /*< The bounded credential-entry interval elapsed.                                   */

    LCS_EVENT_UNLOCK_TIMEOUT,             /*< The bounded authorized-unlock interval elapsed.                                  */

    LCS_EVENT_DENIED_ACCESS_TIMEOUT,      /*< The bounded access-denied feedback interval elapsed.                             */

    LCS_EVENT_LOCKOUT_TIMEOUT,            /*< The temporary lockout interval elapsed.                                          */

    LCS_EVENT_COUNT                       /*< Number of event identifiers; not a dispatchable event.                           */

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
    LCS_ACTION_NONE = 0U,                      /*< No application-level work is requested.                                      */

    LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION, /*< Wake the UI, begin credential entry and establish its inactivity timing.     */

    LCS_ACTION_END_CREDENTIAL_ENTRY_SESSION,   /*< End and erase the entry session, then restore the locked-idle presentation.  */

    LCS_ACTION_REQUEST_AUTHENTICATION,         /*< Obtain, erase and authenticate the completed candidate, then report result. */

    LCS_ACTION_GRANT_ACCESS_UNLOCK,            /*< Start bounded unlock operation and access-granted feedback.                 */

    LCS_ACTION_DENY_ACCESS,                    /*< Preserve the safe lock state and start bounded access-denied feedback.      */

    LCS_ACTION_ENTER_LOCKOUT,                  /*< Preserve the safe lock state, reject entry and start the lockout interval.  */

    LCS_ACTION_RETURN_TO_LOCKED,               /*< Restore the safe locked-idle mode and its user-interface policy.            */

    LCS_ACTION_REQUEST_CONTROLLED_RESET        /*< Preserve safe outputs and request the application-controlled reset path.    */

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
