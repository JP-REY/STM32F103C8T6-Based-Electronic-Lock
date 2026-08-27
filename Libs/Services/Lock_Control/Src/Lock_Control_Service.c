/**********************************************************************************************************************************
 * @file    Lock_Control_Service.c
 * @brief   Lock Control Service implementation.
 *
 * @details Implements the singleton runtime context, ordered transition table, guard evaluation and private transition effects
 *          of the electronic-lock state machine.
 *
 *          Each accepted event is matched against the current state and the immutable transition table. The first transition
 *          whose source state, event, guard and, where required, pending-operation discriminator match is selected. Its internal
 *          effect is applied, the runtime state is committed to the transition target and its semantic action is returned to the
 *          application layer.
 *
 *          This implementation contains no hardware access, service-to-service calls, time-source reads, RTOS primitives or
 *          dynamic allocation. External behavior is requested exclusively through LCS_Action_t values.
 *
 * @author  Joao Pedro Rey
 * @version 1.2.0
 * @date    Aug 21, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Lock_Control_Service.h"
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**
 * @brief   Consecutive authentication-failure limit that activates lockout policy.
 *
 * @details The private failure counter saturates at this value. After denial feedback completes, a count below this limit returns
 *          the machine to the locked state, while a count equal to the limit enters lockout.
 *
 * @note    The limit expresses product policy and is intentionally independent from the storage width of the failure counter.
 */
#define LCS_FAILURE_ATTEMPTS_LIMIT  (3U)

/**
 * @brief   Maximum number of confirmation mismatches accepted during one credential-register session.
 *
 * @details The first two mismatches return to confirmation entry. The third mismatch aborts the registration session and restores
 *          the locked state. This policy is independent from LCS_FAILURE_ATTEMPTS_LIMIT.
 */
#define LCS_CREDENTIAL_REGISTER_MISMATCH_LIMIT  (3U)

/**
 * @brief   Number of transition records in the immutable transition table.
 *
 * @details Computes the record count directly from LCS_Transitions so table iteration cannot become inconsistent with the
 *          declared array length.
 *
 * @note    This macro shall be used only after the LCS_Transitions declaration is visible to the compiler.
 */
#define LCS_TRANSITION_COUNT (sizeof(LCS_Transitions) / sizeof(LCS_Transitions[0]))

/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**
 * @brief   Private states of the electronic-lock state machine.
 *
 * @details States describe product-level operating modes. They do not expose presentation details, electrical actuator levels or
 *          the internal states of collaborating services.
 *
 * @note    The type remains private so application code reacts to semantic actions rather than coupling itself to FSM internals.
 */
typedef enum
{
    LCS_STATE_BOOT = 0U,                             /*< Startup state awaiting the application initialization result.                            */

    LCS_STATE_LOCKED,                                /*< Secure idle state awaiting credential entry, registration or exit requests.              */

    LCS_STATE_CREDENTIAL_REGISTER_FIRST_ENTRY,       /*< First new credential entry is being collected.                                           */

    LCS_STATE_CREDENTIAL_REGISTER_CONFIRM_ENTRY,     /*< Confirmation entry for the staged credential is being collected.                         */

    LCS_STATE_CREDENTIAL_REGISTER_VALIDATING,        /*< First and confirmation credential entries are awaiting comparison.                       */

    LCS_STATE_CREDENTIAL_REGISTER_PERSISTING,        /*< Validated credential is awaiting the persistent-storage result.                          */

    LCS_STATE_CREDENTIAL_REGISTER_SUCCESS_FEEDBACK,  /*< Bounded successful-registration feedback is active.                                      */

    LCS_STATE_CREDENTIAL_SESSION_ACTIVE,             /*< Normal or authorization credential-entry session is active.                              */

    LCS_STATE_AUTHENTICATING,                        /*< A completed candidate is awaiting its authentication result.                             */

    LCS_STATE_ACCESS_UNLOCKED,                       /*< Access is unlocked and the FSM is awaiting the required door-position condition.         */

    LCS_STATE_DOOR_SENSOR_CONFIRMATION,              /*< Required door position was observed and the bounded confirmation delay is active.        */

    LCS_STATE_READY_TO_LOCK,                         /*< Door-control confirmation has been requested and the FSM awaits authorization to relock. */

    LCS_STATE_LOCKED_ACCESS_DENIED_FEEDBACK_ACTIVE,  /*< Access remains locked while bounded access-denied feedback is active.                    */

    LCS_STATE_LOCKOUT,                               /*< Credential-entry requests are rejected until the lockout interval expires.               */

    LCS_STATE_FAULT,                                 /*< A critical failure prevents further normal operation in this runtime.                    */

    LCS_STATE_COUNT                                  /*< Number of private state identifiers; not a runtime state.                                */

}LCS_State_t;

/**
 * @brief   Private guard conditions associated with transition records.
 *
 * @details A guard is evaluated only after the transition source state and event match the current dispatch context. Guard values
 *          allow more than one transition to share the same source/event pair while selecting different targets from runtime
 *          policy state.
 *
 * @note    Guards sharing a source/event pair shall be mutually exclusive. Unknown guard values evaluate false.
 */
typedef enum
{
    LCS_GUARD_ALWAYS,                       /*< Unconditional transition.                                         */

    LCS_GUARD_UNDER_ATTEMPT_LIMIT,          /*< True while consecutive failures remain below the lockout limit.   */

    LCS_GUARD_ATTEMPT_COUNT_LIMIT,          /*< True once consecutive failures reach the lockout limit.           */

    LCS_GUARD_REGISTER_RETRY_AVAILABLE,     /*< True when the current mismatch still leaves a confirmation retry. */

    LCS_GUARD_REGISTER_ATTEMPT_LIMIT        /*< True when the current mismatch consumes the final allowed try.    */

}LCS_Guard_t;

/**
 * @brief   Private mutation applied to the singleton runtime during a transition.
 *
 * @details Internal effects update only state owned by the Lock Control Service. They are distinct from public actions, which ask
 *          the application layer to coordinate other services and hardware-facing adapters.
 *
 * @note    The target state assignment is a mandatory part of every accepted transition and therefore is not represented as an
 *          internal-effect value.
 */
typedef enum
{
    LCS_INTERNAL_EFFECT_NONE = 0U,                                  /*< Preserve all runtime fields other than the target state.      */

    LCS_INTERNAL_EFFECT_SET_SERVICE_ACTIVE,                         /*< Mark successful boot activation in the singleton context.     */

    LCS_INTERNAL_EFFECT_SET_PENDING_REGISTER_SESSION,               /*< Route successful authentication to registration.              */

    LCS_INTERNAL_EFFECT_SET_PENDING_UNLOCK,                         /*< Route successful authentication to bounded unlock.            */

    LCS_INTERNAL_EFFECT_CLEAR_PENDING,                              /*< Restore the no-pending-request invariant.                     */

    LCS_INTERNAL_EFFECT_INCREMENT_ATTEMPT_COUNT,                    /*< Saturating increment of consecutive authentication failures.  */

    LCS_INTERNAL_EFFECT_RESET_ATTEMPT_COUNT,                        /*< Clear the consecutive authentication-failure counter.         */

    LCS_INTERNAL_EFFECT_CLEAR_PENDING_AND_INCREMENT_ATTEMPT_COUNT,  /*< Clear intent and record one rejected authentication.          */

    LCS_INTERNAL_EFFECT_CLEAR_PENDING_AND_RESET_ATTEMPT_COUNT,      /*< Clear intent and consecutive authentication failures.         */

    LCS_INTERNAL_EFFECT_INCREMENT_REGISTER_MISMATCH_COUNT,          /*< Record one mismatched registration confirmation.              */

    LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT,              /*< Clear registration-confirmation mismatch history.             */

}LCS_InternalEffect_t;

/**
 * @brief   Pending operation authorized by the active credential-entry flow.
 *
 * @details Distinguishes the product operation that shall follow successful authentication. The value is private policy context;
 *          it is never exposed as an application action and never contains credential data.
 *
 * @note    LCS_PENDING_NONE is the required idle value. A nonzero value is established when entry begins or is reclassified and
 *          is preserved until authentication, cancellation or timeout resolves the request.
 */
typedef enum
{
    LCS_PENDING_NONE = 0U,             /*< No product operation is awaiting authentication.     */

    LCS_PENDING_UNLOCK,                /*< Successful authentication authorizes bounded unlock. */
    
    LCS_PENDING_CREDENTIAL_REGISTER,   /*< Successful authentication authorizes registration.   */

}LCS_Pending_t;

/**
 * @brief   Immutable description of one state-machine transition.
 *
 * @details A record defines the source state and event used for candidate matching, the guard and optional pending-operation
 *          discriminator that authorize selection, the target state committed after selection, the private runtime effect
 *          applied by the service and the semantic action returned to the application.
 *
 * @note    Transition records reside in read-only static storage and shall never be modified at runtime.
 */
typedef struct
{
    LCS_State_t          source_state;      /*< State from which the transition may be selected.                */
    LCS_Event_t          event;             /*< Semantic event required to select the transition.               */
    LCS_Guard_t          guard;             /*< Runtime condition that must evaluate true.                      */
    LCS_Pending_t        pending_op;        /*< Pending request required by an authentication-success route.    */
    LCS_State_t          target_state;      /*< State committed after the transition is accepted.               */
    LCS_InternalEffect_t internal_effect;   /*< Private context mutation applied before target-state commit.    */
    LCS_Action_t         action;            /*< Semantic application action returned after target-state commit. */

}LCS_Transition_t;

/**
 * @brief   Mutable runtime context of the Lock Control Service singleton.
 *
 * @details Retains the authoritative FSM state, the number of consecutive rejected authentications, the number of confirmation
 *          mismatches in the active credential-register session, the operation awaiting authentication and the activation status
 *          of the service. The transition table is immutable module data and therefore is intentionally excluded from this handle.
 *
 * @note    Exactly one instance is allocated by this translation unit. Concurrent access is not supported.
 */
typedef struct
{
    LCS_State_t   current_state;                       /*< Authoritative current state used during transition lookup.  */
    uint8_t       failed_attempt_count;                /*< Saturating count of consecutive authentication failures.    */
    uint8_t       credential_register_mismatch_count;  /*< Prior confirmation mismatches in the active register flow.  */
    LCS_Pending_t pending_request;                     /*< Product operation selected after successful authentication. */
    bool          initialized;                         /*< Indicates whether successful boot activation has occurred.  */

}LCS_Handle_t;

/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**
 * @brief   Ordered transition rules of the electronic-lock state machine.
 *
 * @details Stores only declared transitions rather than allocating a dense state-by-event matrix. LCS_FindTransition() scans the
 *          table from the first record to the last and returns the first record whose source state and event match the dispatch
 *          context, whose guard evaluates true and, for authentication success, whose pending-operation value matches the
 *          singleton request.
 *
 *          The two authentication-success records intentionally share a source/event/guard tuple. Their mutually exclusive
 *          pending-operation values select either credential registration or bounded unlock. The two access-denied timeout
 *          records share a source/event pair and use mutually exclusive attempt-count guards to select return-to-locked or
 *          lockout behavior. The two validation-failure records likewise use complementary registration guards to select another
 *          confirmation attempt or session abortion on the third mismatch.
 *
 * @note    Because selection is first-match, future records with the same source/event pair shall use mutually exclusive guards
 *          or be ordered deliberately and documented as priority rules.
 */
static const LCS_Transition_t LCS_Transitions[] =
{
    {
        .source_state    = LCS_STATE_BOOT,
        .event           = LCS_EVENT_INIT_OK,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_SET_SERVICE_ACTIVE,
        .action          = LCS_ACTION_NONE
    },

    {
        .source_state    = LCS_STATE_BOOT,
        .event           = LCS_EVENT_INIT_FAIL,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_FAULT,
        .internal_effect = LCS_INTERNAL_EFFECT_NONE,
        .action          = LCS_ACTION_REQUEST_CONTROLLED_RESET
    },

    {
        .source_state    = LCS_STATE_BOOT,
        .event           = LCS_EVENT_CREDENTIAL_NOT_REGISTERED,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_CREDENTIAL_REGISTER_FIRST_ENTRY,
        .internal_effect = LCS_INTERNAL_EFFECT_SET_SERVICE_ACTIVE,
        .action          = LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_LOCKED,
        .event           = LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_CREDENTIAL_SESSION_ACTIVE,
        .internal_effect = LCS_INTERNAL_EFFECT_SET_PENDING_UNLOCK,
        .action          = LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_SESSION_ACTIVE,
        .event           = LCS_EVENT_CREDENTIAL_CANCELLED,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_CLEAR_PENDING,
        .action          = LCS_ACTION_END_CREDENTIAL_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_SESSION_ACTIVE,
        .event           = LCS_EVENT_CREDENTIAL_REGISTER_REQUESTED,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_CREDENTIAL_SESSION_ACTIVE,
        .internal_effect = LCS_INTERNAL_EFFECT_SET_PENDING_REGISTER_SESSION,
        .action          = LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_TO_REGISTER_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_SESSION_ACTIVE,
        .event           = LCS_EVENT_ENTRY_TIMEOUT,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_CLEAR_PENDING,
        .action          = LCS_ACTION_RETURN_TO_LOCKED_FROM_ENTRY_TIMEOUT
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_SESSION_ACTIVE,
        .event           = LCS_EVENT_CANDIDATE_INCOMPLETE,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_CREDENTIAL_SESSION_ACTIVE,
        .internal_effect = LCS_INTERNAL_EFFECT_NONE,
        .action          = LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_SESSION_ACTIVE,
        .event           = LCS_EVENT_CANDIDATE_READY,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_AUTHENTICATING,
        .internal_effect = LCS_INTERNAL_EFFECT_NONE,
        .action          = LCS_ACTION_REQUEST_AUTHENTICATION
    },

    {
        .source_state    = LCS_STATE_AUTHENTICATING,
        .event           = LCS_EVENT_AUTH_SUCCESS,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_CREDENTIAL_REGISTER,
        .target_state    = LCS_STATE_CREDENTIAL_REGISTER_FIRST_ENTRY,
        .internal_effect = LCS_INTERNAL_EFFECT_CLEAR_PENDING_AND_RESET_ATTEMPT_COUNT,
        .action          = LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_AUTHENTICATING,
        .event           = LCS_EVENT_AUTH_SUCCESS,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_UNLOCK,
        .target_state    = LCS_STATE_ACCESS_UNLOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_CLEAR_PENDING_AND_RESET_ATTEMPT_COUNT,
        .action          = LCS_ACTION_REQUEST_UNLOCK
    },

    {
        .source_state    = LCS_STATE_LOCKED,
        .event           = LCS_EVENT_EXIT_REQUEST,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_ACCESS_UNLOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_CLEAR_PENDING,
        .action          = LCS_ACTION_EXIT_REQUEST_UNLOCK
    },

    {
        .source_state    = LCS_STATE_ACCESS_UNLOCKED,
        .event           = LCS_EVENT_DOOR_POSITION_CONFIRMED,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_DOOR_SENSOR_CONFIRMATION,
        .internal_effect = LCS_INTERNAL_EFFECT_NONE,
        .action          = LCS_ACTION_BEGIN_DOOR_SENSOR_CONFIRMATION
    },

    {
        .source_state    = LCS_STATE_DOOR_SENSOR_CONFIRMATION,
        .event           = LCS_EVENT_DOOR_SENSOR_CONFIRMATION_TIMEOUT,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_READY_TO_LOCK,
        .internal_effect = LCS_INTERNAL_EFFECT_CLEAR_PENDING,
        .action          = LCS_ACTION_REQUEST_DOOR_SENSOR_CONFIRMATION
    },

    {
        .source_state    = LCS_STATE_READY_TO_LOCK,
        .event           = LCS_EVENT_READY_TO_LOCK,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_CLEAR_PENDING,
        .action          = LCS_ACTION_RETURN_TO_LOCKED_FROM_GRANTED_ACCESS
    },

    {
        .source_state    = LCS_STATE_AUTHENTICATING,
        .event           = LCS_EVENT_AUTH_FAILURE,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED_ACCESS_DENIED_FEEDBACK_ACTIVE,
        .internal_effect = LCS_INTERNAL_EFFECT_CLEAR_PENDING_AND_INCREMENT_ATTEMPT_COUNT,
        .action          = LCS_ACTION_DENY_ACCESS
    },

    {
        .source_state    = LCS_STATE_LOCKED_ACCESS_DENIED_FEEDBACK_ACTIVE,
        .event           = LCS_EVENT_DENIED_ACCESS_TIMEOUT,
        .guard           = LCS_GUARD_UNDER_ATTEMPT_LIMIT,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_CLEAR_PENDING,
        .action          = LCS_ACTION_RETURN_TO_LOCKED
    },

    {
        .source_state    = LCS_STATE_LOCKED_ACCESS_DENIED_FEEDBACK_ACTIVE,
        .event           = LCS_EVENT_DENIED_ACCESS_TIMEOUT,
        .guard           = LCS_GUARD_ATTEMPT_COUNT_LIMIT,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKOUT,
        .internal_effect = LCS_INTERNAL_EFFECT_CLEAR_PENDING,
        .action          = LCS_ACTION_ENTER_LOCKOUT
    },

    {
        .source_state    = LCS_STATE_LOCKOUT,
        .event           = LCS_EVENT_LOCKOUT_TIMEOUT,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_RESET_ATTEMPT_COUNT,
        .action          = LCS_ACTION_RETURN_TO_LOCKED
    },

    /* Credential-register session transitions. */
    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_FIRST_ENTRY,
        .event           = LCS_EVENT_CANDIDATE_READY,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_CREDENTIAL_REGISTER_CONFIRM_ENTRY,
        .internal_effect = LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT,
        .action          = LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_TO_CONFIRM_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_FIRST_ENTRY,
        .event           = LCS_EVENT_CREDENTIAL_CANCELLED,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT,
        .action          = LCS_ACTION_END_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_FIRST_ENTRY,
        .event           = LCS_EVENT_CANDIDATE_INCOMPLETE,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_CREDENTIAL_REGISTER_FIRST_ENTRY,
        .internal_effect = LCS_INTERNAL_EFFECT_NONE,
        .action          = LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_FIRST_ENTRY,
        .event           = LCS_EVENT_ENTRY_TIMEOUT,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT,
        .action          = LCS_ACTION_END_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_CONFIRM_ENTRY,
        .event           = LCS_EVENT_CANDIDATE_READY,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_CREDENTIAL_REGISTER_VALIDATING,
        .internal_effect = LCS_INTERNAL_EFFECT_NONE,
        .action          = LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STAGES_VALIDATION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_CONFIRM_ENTRY,
        .event           = LCS_EVENT_CREDENTIAL_CANCELLED,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT,
        .action          = LCS_ACTION_END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_CONFIRM_ENTRY,
        .event           = LCS_EVENT_CANDIDATE_INCOMPLETE,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_CREDENTIAL_REGISTER_CONFIRM_ENTRY,
        .internal_effect = LCS_INTERNAL_EFFECT_NONE,
        .action          = LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_CONFIRM_ENTRY,
        .event           = LCS_EVENT_ENTRY_TIMEOUT,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT,
        .action          = LCS_ACTION_END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_VALIDATING,
        .event           = LCS_EVENT_STAGING_VALIDATION_SUCCESS,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_CREDENTIAL_REGISTER_PERSISTING,
        .internal_effect = LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT,
        .action          = LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STORAGE
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_VALIDATING,
        .event           = LCS_EVENT_STAGING_VALIDATION_FAILURE,
        .guard           = LCS_GUARD_REGISTER_RETRY_AVAILABLE,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_CREDENTIAL_REGISTER_CONFIRM_ENTRY,
        .internal_effect = LCS_INTERNAL_EFFECT_INCREMENT_REGISTER_MISMATCH_COUNT,
        .action          = LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_VALIDATING,
        .event           = LCS_EVENT_STAGING_VALIDATION_FAILURE,
        .guard           = LCS_GUARD_REGISTER_ATTEMPT_LIMIT,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT,
        .action          = LCS_ACTION_END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_PERSISTING,
        .event           = LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_SUCCESS,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_CREDENTIAL_REGISTER_SUCCESS_FEEDBACK,
        .internal_effect = LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT,
        .action          = LCS_ACTION_END_CREDENTIAL_REGISTER_SAVING_SESSION
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_PERSISTING,
        .event           = LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_FAILURE,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_FAULT,
        .internal_effect = LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT,
        .action          = LCS_ACTION_REQUEST_CONTROLLED_RESET
    },

    {
        .source_state    = LCS_STATE_CREDENTIAL_REGISTER_SUCCESS_FEEDBACK,
        .event           = LCS_EVENT_CREDENTIAL_REGISTER_DONE,
        .guard           = LCS_GUARD_ALWAYS,
        .pending_op      = LCS_PENDING_NONE,
        .target_state    = LCS_STATE_LOCKED,
        .internal_effect = LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT,
        .action          = LCS_ACTION_RETURN_TO_LOCKED_FROM_CREDENTIAL_REGISTER_SESSION
    },

};

/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/**
 * @brief   Singleton runtime instance of the Lock Control Service.
 *
 * @details Starts in LCS_STATE_BOOT with no authentication failures, no registration mismatches, no pending operation and normal
 *          operation inactive. LCS_EVENT_INIT_OK activates the instance and commits the locked state, while
 *          LCS_EVENT_CREDENTIAL_NOT_REGISTERED activates it directly in the first-registration state.
 *          LCS_EVENT_INIT_FAIL commits the fault state without activating normal operation.
 *
 * @note    Static initialization guarantees a deterministic safe context before the first call to LCS_Process().
 */
static LCS_Handle_t LCS_RuntimeInstance =
{
    .current_state                      = LCS_STATE_BOOT,
    .failed_attempt_count               = 0U,
    .credential_register_mismatch_count = 0U,
    .pending_request                    = LCS_PENDING_NONE,
    .initialized                        = false
};

/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
static void                    LCS_ApplyInternalEffect (LCS_InternalEffect_t Effect);
static bool                    LCS_EvaluateGuard       (LCS_Guard_t Guard);
static LCS_Pending_t           LCS_GetPendingOperation (void);
static const LCS_Transition_t* LCS_FindTransition      (LCS_Event_t Event);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Reports whether normal Lock Control Service processing is active.
 *
 * @details Reads the lifecycle flag owned by the singleton runtime. The flag becomes true only when the boot state processes
 *          LCS_EVENT_INIT_OK.
 *
 * @note    This helper does not modify the runtime context.
 *
 * @return  true when normal event processing is active; otherwise false.
 */
static inline bool LCS_IsActive(void)
{
    return LCS_RuntimeInstance.initialized;
}

/**
 * @brief   Applies one private transition effect to the singleton runtime.
 *
 * @details Activates the service after successful boot; establishes, clears or resolves the pending operation; updates the
 *          consecutive authentication-failure counter with saturation at LCS_FAILURE_ATTEMPTS_LIMIT; and updates the independent
 *          registration-mismatch counter with saturation at LCS_CREDENTIAL_REGISTER_MISMATCH_LIMIT.
 *
 * @param   Effect - Private mutation selected by the accepted transition.
 *
 * @note    LCS_INTERNAL_EFFECT_NONE and unknown values preserve the runtime fields. Target-state assignment is performed
 *          separately by LCS_Process() after this function returns.
 */
static void LCS_ApplyInternalEffect(LCS_InternalEffect_t Effect)
{
    switch(Effect)
    {
        case LCS_INTERNAL_EFFECT_SET_SERVICE_ACTIVE:

            LCS_RuntimeInstance.initialized = true;

        break;

        case LCS_INTERNAL_EFFECT_SET_PENDING_REGISTER_SESSION:

            LCS_RuntimeInstance.pending_request = LCS_PENDING_CREDENTIAL_REGISTER;

        break;

        case LCS_INTERNAL_EFFECT_SET_PENDING_UNLOCK:

            LCS_RuntimeInstance.pending_request = LCS_PENDING_UNLOCK;

        break;

        case LCS_INTERNAL_EFFECT_CLEAR_PENDING:

            LCS_RuntimeInstance.pending_request = LCS_PENDING_NONE;

        break;

        case LCS_INTERNAL_EFFECT_INCREMENT_ATTEMPT_COUNT:

            if(LCS_RuntimeInstance.failed_attempt_count < LCS_FAILURE_ATTEMPTS_LIMIT)
            {
                LCS_RuntimeInstance.failed_attempt_count++;
            }

        break;

        case LCS_INTERNAL_EFFECT_RESET_ATTEMPT_COUNT:

            LCS_RuntimeInstance.failed_attempt_count = 0U;

        break;

        case LCS_INTERNAL_EFFECT_CLEAR_PENDING_AND_INCREMENT_ATTEMPT_COUNT:

            LCS_RuntimeInstance.pending_request = LCS_PENDING_NONE;

            if(LCS_RuntimeInstance.failed_attempt_count < LCS_FAILURE_ATTEMPTS_LIMIT)
            {
                LCS_RuntimeInstance.failed_attempt_count++;
            }

        break;

        case LCS_INTERNAL_EFFECT_CLEAR_PENDING_AND_RESET_ATTEMPT_COUNT:

            LCS_RuntimeInstance.pending_request      = LCS_PENDING_NONE;
            LCS_RuntimeInstance.failed_attempt_count = 0U;

        break;

        case LCS_INTERNAL_EFFECT_INCREMENT_REGISTER_MISMATCH_COUNT:

            if(LCS_RuntimeInstance.credential_register_mismatch_count < LCS_CREDENTIAL_REGISTER_MISMATCH_LIMIT)
            {
                LCS_RuntimeInstance.credential_register_mismatch_count++;
            }

        break;

        case LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT:

            LCS_RuntimeInstance.credential_register_mismatch_count = 0U;

        break;

        default:
            break;
    }
}

/**
 * @brief   Evaluates one private guard against the current singleton context.
 *
 * @details Resolves unconditional selection, the two complementary authentication-failure conditions used after access-denied
 *          feedback and the two complementary registration-mismatch conditions used after staging validation fails.
 *
 * @param   Guard - Guard identifier stored in a candidate transition record.
 *
 * @note    Unknown guard values return false so malformed transition data cannot authorize a state change.
 *
 * @return  true when Guard authorizes the candidate transition; otherwise false.
 */
static bool LCS_EvaluateGuard(LCS_Guard_t Guard)
{
    switch(Guard)
    {
        case LCS_GUARD_ALWAYS:

            return true;

        case LCS_GUARD_ATTEMPT_COUNT_LIMIT:

            return (LCS_RuntimeInstance.failed_attempt_count >= LCS_FAILURE_ATTEMPTS_LIMIT);

        case LCS_GUARD_UNDER_ATTEMPT_LIMIT:

            return (LCS_RuntimeInstance.failed_attempt_count < LCS_FAILURE_ATTEMPTS_LIMIT);

        case LCS_GUARD_REGISTER_RETRY_AVAILABLE:

            return ((uint32_t)LCS_RuntimeInstance.credential_register_mismatch_count + 1U) <
                   LCS_CREDENTIAL_REGISTER_MISMATCH_LIMIT;

        case LCS_GUARD_REGISTER_ATTEMPT_LIMIT:

            return ((uint32_t)LCS_RuntimeInstance.credential_register_mismatch_count + 1U) >=
                   LCS_CREDENTIAL_REGISTER_MISMATCH_LIMIT;

        default:
            return false;
    }
}

/**
 * @brief   Obtains the operation awaiting successful authentication.
 *
 * @details Reads the singleton pending-request field without modifying any runtime state.
 *
 * @return  Current pending operation, or LCS_PENDING_NONE when no request awaits authorization.
 */
static LCS_Pending_t LCS_GetPendingOperation(void)
{
    return LCS_RuntimeInstance.pending_request;
}

/**
 * @brief   Finds the first transition authorized for the current state and supplied event.
 *
 * @details Performs a bounded linear scan of LCS_Transitions. A record is selected only when its source state equals the current
 *          runtime state, its event equals Event and LCS_EvaluateGuard() authorizes its guard. Authentication-success candidates
 *          additionally require their pending-operation discriminator to equal the singleton pending request.
 *
 * @param   Event - Semantic product event to match against transition records.
 *
 * @note    The returned pointer refers to immutable static storage and remains valid for the entire program lifetime. This helper
 *          does not modify the runtime context.
 *
 * @return  Pointer to the first authorized transition; NULL when no transition is valid or authentication succeeds without a
 *          pending operation.
 */
static const LCS_Transition_t* LCS_FindTransition(LCS_Event_t Event)
{
    size_t index = 0;

    LCS_Pending_t pending = LCS_GetPendingOperation();

    for(index = 0; index < LCS_TRANSITION_COUNT; index++)
    {
        const LCS_Transition_t* transition = &LCS_Transitions[index];

        if(transition->source_state == LCS_RuntimeInstance.current_state &&
           transition->event == Event && LCS_EvaluateGuard(transition->guard))
        {
            if(LCS_RuntimeInstance.current_state == LCS_STATE_AUTHENTICATING &&
               Event == LCS_EVENT_AUTH_SUCCESS)
            {
                if(pending != LCS_PENDING_NONE)
                {
                    if(transition->pending_op == pending)
                    {
                        return transition;
                    }
                }
            }

            else
            {
                return transition;
            }
        }
    }

    return NULL;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Processes one semantic event through the Lock Control Service state machine.
 *
 * @details When normal operation is inactive, LCS_EVENT_INIT_OK, LCS_EVENT_INIT_FAIL and
 *          LCS_EVENT_CREDENTIAL_NOT_REGISTERED are eligible for dispatch. All other events are ignored.
 *
 *          For an eligible event, the function locates the first authorized transition, applies its private effect, commits the
 *          target state and returns its semantic application action. If no transition matches, the runtime context is preserved
 *          and LCS_ACTION_NONE is returned.
 *
 * @param   Event - Semantic product event to process.
 *
 * @note    The internal effect and target-state commit occur before the action is returned. The application may therefore dispatch
 *          a synchronous result event only after this call completes and observe the new authoritative state.
 *
 * @note    The function is non-blocking, allocates no memory and performs no calls to hardware or collaborating services.
 *
 * @return  Semantic action associated with the accepted transition, or LCS_ACTION_NONE when no external work is requested.
 */
LCS_Action_t LCS_Process(LCS_Event_t Event)
{
    const LCS_Transition_t* transition;

    if(!LCS_IsActive() &&
       Event != LCS_EVENT_INIT_OK &&
       Event != LCS_EVENT_INIT_FAIL &&
       Event != LCS_EVENT_CREDENTIAL_NOT_REGISTERED)
    {
        return LCS_ACTION_NONE;
    }

    transition = LCS_FindTransition(Event);

    if(transition == NULL)
    {
        return LCS_ACTION_NONE;
    }

    LCS_ApplyInternalEffect(transition->internal_effect);

    LCS_RuntimeInstance.current_state = transition->target_state;

    return transition->action;
}
