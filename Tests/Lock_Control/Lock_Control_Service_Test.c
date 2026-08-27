/**********************************************************************************************************************************
 * @file    Lock_Control_Service_Test.c
 * @brief   Black-box host validation of the Lock Control Service state machine.
 *
 * @details Exercises the production LCS_Process() API without exposing private state or compiling test-only behavior into the
 *          service. Each scenario supplies an ordered event sequence and validates the semantic action returned after every
 *          relevant event. When a transition returns LCS_ACTION_NONE, a later observable transition is also used when necessary
 *          to prove that the expected private state was reached or preserved.
 *
 *          The Lock Control Service owns one private singleton and intentionally exposes no reset API. CTest therefore launches
 *          every named scenario in a separate process. Static initialization then gives every scenario the same deterministic
 *          LCS_STATE_BOOT starting condition without weakening the production interface with a test-only reset hook.
 *
 *          This translation unit contains no mock of the state machine. It links the production Lock_Control_Service.c source
 *          and interacts only through the public declarations in Lock_Control_Service.h.
 *
 * @note    Add every new scenario to both LCS_TestCases in this file and LCS_FSM_TEST_SCENARIOS in Tests/CMakeLists.txt. Document
 *          its purpose and expected coverage in Tests/README.md so the executable, CTest registration and maintenance guide stay
 *          synchronized.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 21, 2026
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Lock_Control_Service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**
 * @brief   Dispatches one event and requires the exact semantic action expected by the scenario.
 *
 * @details The event and expected-action expressions are evaluated once. Their source identifiers and the call-site line are
 *          forwarded to LCS_test_expect_action() so a failure identifies the violated step. The enclosing scenario returns false
 *          immediately after a failed expectation.
 *
 * @param   Event    - LCS_Event_t expression to dispatch through the production LCS_Process() API.
 * @param   Expected - LCS_Action_t expression expected from that dispatch.
 *
 * @note    This macro may be used only inside a function that returns bool.
 */
#define LCS_TEST_EXPECT_ACTION(Event, Expected)                                                                \
    do                                                                                                         \
    {                                                                                                          \
        if(!LCS_test_expect_action((Event), (Expected), #Event, #Expected, (uint32_t)__LINE__))                \
        {                                                                                                      \
            return false;                                                                                      \
        }                                                                                                      \
    }while(0)

/**
 * @brief   Requires a Boolean fixture or intermediate condition to succeed.
 *
 * @details A false expression aborts the enclosing scenario. Fixture functions already report any action mismatch at the exact
 *          failing line, so this macro deliberately adds no duplicate diagnostic.
 *
 * @param   Expression - Boolean expression that shall evaluate true.
 *
 * @note    This macro may be used only inside a function that returns bool.
 */
#define LCS_TEST_REQUIRE(Expression)                                                                            \
    do                                                                                                          \
    {                                                                                                           \
        if(!(Expression))                                                                                       \
        {                                                                                                       \
            return false;                                                                                       \
        }                                                                                                       \
    }while(0)

/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**
 * @brief  Signature implemented by every isolated LCS test scenario.
 *
 * @return true  - Every expectation in the scenario passed;
 * @return false - At least one expectation or required fixture failed.
 */
typedef bool (*LCS_TestFunction_t)(void);

/**
 * @brief   Associates one stable command-line scenario name with its test function.
 *
 * @details The command-line name is also registered by CTest with the `lcs.` prefix. Keeping the name independent from the C
 *          function identifier produces short, stable filters for local and CI execution.
 */
typedef struct
{
    const char*        name;      /**< Scenario name accepted by the test executable. */
    LCS_TestFunction_t function;  /**< Function that executes the isolated scenario.  */

}LCS_TestCase_t;

/**********************************************************************************************************************************
 Private Function Definitions: Expectation Support
 **********************************************************************************************************************************/
/**
 * @brief   Dispatches one event to the production FSM and compares its returned action.
 *
 * @param   Event        - Semantic event supplied to LCS_Process().
 * @param   Expected     - Exact semantic action required by the test step.
 * @param   EventName    - Source spelling of Event used in a failure diagnostic.
 * @param   ExpectedName - Source spelling of Expected used in a failure diagnostic.
 * @param   Line         - Test source line containing the expectation.
 *
 * @return  true  - The actual action equals Expected;
 * @return  false - The actions differ and a diagnostic was written to standard error.
 */
static bool LCS_test_expect_action(LCS_Event_t Event, LCS_Action_t Expected, const char* EventName, const char* ExpectedName, uint32_t Line)
{
    LCS_Action_t actual = LCS_Process(Event);

    if(actual == Expected)
    {
        return true;
    }

    (void)fprintf(stderr,
                  "line %u: %s returned action %u; expected %s (%u)\n",
                  (unsigned int)Line,
                  EventName,
                  (unsigned int)actual,
                  ExpectedName,
                  (unsigned int)Expected);

    return false;
}

/**********************************************************************************************************************************
 Private Function Definitions: Reusable FSM Paths
 **********************************************************************************************************************************/
/**
 * @brief   Activates normal locked operation from the fresh boot state.
 *
 * @details Dispatches LCS_EVENT_INIT_OK and verifies the intentionally silent transition from boot to locked idle.
 *
 * @return  true  - Boot activation returned LCS_ACTION_NONE as specified;
 * @return  false - Boot activation produced another action.
 */
static bool LCS_test_activate_locked(void)
{
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_INIT_OK, 
                           LCS_ACTION_NONE);

    return true;
}

/**
 * @brief   Drives locked idle to the state awaiting an authentication result.
 *
 * @details Starts normal credential entry. When RegistrationRequested is true, the active request is first reclassified as
 *          credential-register authorization. A complete candidate then requests Authentication Service coordination.
 *
 * @param   RegistrationRequested - true to authorize credential registration; false to authorize normal unlock.
 *
 * @return  true  - Every action along the shared entry path matched the contract;
 * @return  false - At least one path expectation failed.
 *
 * @pre     The FSM shall be in locked idle.
 * @post    The FSM is awaiting LCS_EVENT_AUTH_SUCCESS or LCS_EVENT_AUTH_FAILURE.
 */
static bool LCS_test_reach_authentication_from_locked(bool RegistrationRequested)
{
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED,
                           LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION);

    if(RegistrationRequested)
    {
        LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_REGISTER_REQUESTED,
                               LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_TO_REGISTER_SESSION);
    }

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_READY,
                           LCS_ACTION_REQUEST_AUTHENTICATION);

    return true;
}

/**
 * @brief   Drives locked idle to the first-entry phase of authorized credential registration.
 *
 * @details Reuses the registration-authorization path and accepts the installed credential through Authentication Service.
 *
 * @return  true  - Registration authorization selected the first-entry action;
 * @return  false - The shared path or authentication-success expectation failed.
 *
 * @pre     The FSM shall be in locked idle.
 * @post    The FSM is collecting the first new credential entry.
 */
static bool LCS_test_reach_first_entry_from_locked(void)
{
    LCS_TEST_REQUIRE(LCS_test_reach_authentication_from_locked(true));

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_AUTH_SUCCESS,
                           LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION);

    return true;
}

/**
 * @brief   Drives credential registration from first entry to staging validation.
 *
 * @details Completes the first candidate, which stages it and starts confirmation, then completes the confirmation candidate,
 *          which asks the application to compare both registration stages.
 *
 * @return  true  - Both phase-completion actions matched the contract;
 * @return  false - At least one returned action differed.
 *
 * @pre     The FSM shall be collecting the first new credential entry.
 * @post    The FSM is awaiting a staging-validation result.
 */
static bool LCS_test_reach_validation_from_first_entry(void)
{
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_READY,
                           LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_TO_CONFIRM_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_READY,
                           LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STAGES_VALIDATION);

    return true;
}

/**
 * @brief   Drives credential registration from first entry to persistent-storage wait.
 *
 * @details Reuses the staging-validation path and reports a successful match, which shall request persistence through Credential
 *          Storage Service.
 *
 * @return  true  - Validation success selected the credential-storage action;
 * @return  false - The validation path or storage-action expectation failed.
 *
 * @pre     The FSM shall be collecting the first new credential entry.
 * @post    The FSM is awaiting a credential-storage result.
 */
static bool LCS_test_reach_persistence_from_first_entry(void)
{
    LCS_TEST_REQUIRE(LCS_test_reach_validation_from_first_entry());

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_STAGING_VALIDATION_SUCCESS,
                           LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STORAGE);

    return true;
}

/**
 * @brief   Executes one rejected normal-unlock authentication cycle.
 *
 * @details Starts entry for normal unlock, reports authentication failure, verifies denial feedback, then verifies the action
 *          selected when that feedback expires. The caller chooses whether the accumulated count shall return to locked idle or
 *          enter lockout.
 *
 * @param   TimeoutAction - Expected action for LCS_EVENT_DENIED_ACCESS_TIMEOUT.
 *
 * @return  true  - The complete rejection cycle matched the expected policy branch;
 * @return  false - At least one action differed.
 *
 * @pre     The FSM shall be in locked idle.
 * @post    The FSM is locked idle when TimeoutAction is LCS_ACTION_RETURN_TO_LOCKED, or in lockout when it is
 *          LCS_ACTION_ENTER_LOCKOUT.
 */
static bool LCS_test_reject_one_authentication(LCS_Action_t TimeoutAction)
{
    LCS_TEST_REQUIRE(LCS_test_reach_authentication_from_locked(false));

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_AUTH_FAILURE, 
                           LCS_ACTION_DENY_ACCESS);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_DENIED_ACCESS_TIMEOUT, 
                           TimeoutAction);

    return true;
}

/**
 * @brief   Completes the common unlocked-access path and restores locked idle.
 *
 * @details Reports door-position confirmation, advances through the bounded confirmation timeout into the ready-to-lock state and
 *          finally reports that relocking may proceed. The helper validates the complete shared post-unlock sequence used by both
 *          authenticated access and request-to-exit flows.
 *
 * @return  true  - Every action in the shared relock path matched the contract;
 * @return  false - At least one returned action differed.
 *
 * @pre     The FSM shall be in the unlocked-access phase.
 * @post    The FSM is in locked idle.
 */
static bool LCS_test_complete_unlocked_access(void)
{
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_DOOR_POSITION_CONFIRMED,
                           LCS_ACTION_BEGIN_DOOR_SENSOR_CONFIRMATION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_DOOR_SENSOR_CONFIRMATION_TIMEOUT,
                           LCS_ACTION_REQUEST_DOOR_SENSOR_CONFIRMATION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_READY_TO_LOCK,
                           LCS_ACTION_RETURN_TO_LOCKED_FROM_GRANTED_ACCESS);

    return true;
}

/**********************************************************************************************************************************
 Private Function Definitions: Test Scenarios
 **********************************************************************************************************************************/
/**
 * @brief   Validates that boot gates operational events until normal initialization succeeds.
 *
 * @details Sends representative events while inactive and expects no action. After LCS_EVENT_INIT_OK, a credential-entry request
 *          shall be accepted, proving that the silent boot transition reached locked idle.
 *
 * @return  true  - Every boot-gate expectation passed;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_inactive_gate(void)
{
    /* Operational events shall not bypass the initial boot decision. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_NONE, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_READY, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_SUCCESS, 
                           LCS_ACTION_NONE);

    /* Successful initialization shall activate normal locked operation exactly once. */
    LCS_TEST_REQUIRE(LCS_test_activate_locked());

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED,
                           LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION);

    return true;
}

/**
 * @brief   Validates fail-safe boot failure and the absorbing behavior of the fault state.
 *
 * @details Initialization failure shall request a controlled reset. Subsequent startup and operational events shall be ignored,
 *          proving that no event can resume normal operation from fault.
 *
 * @return  true  - The fault path and every ignored-event expectation passed;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_boot_failure(void)
{
    /* The first failure selects the only externally visible fault action. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_INIT_FAIL, 
                           LCS_ACTION_REQUEST_CONTROLLED_RESET);

    /* Fault is terminal for this runtime instance. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_INIT_FAIL, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_INIT_OK,
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_NOT_REGISTERED, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED, 
                           LCS_ACTION_NONE);

    return true;
}

/**
 * @brief   Validates successful normal access from locked idle through bounded unlock completion.
 *
 * @details Exercises entry refresh for an incomplete candidate, authentication request, unlock routing and the complete shared
 *          relock sequence through door confirmation, ready-to-lock authorization and locked idle. Events meaningful only in other
 *          states are interleaved to prove they do not disturb the active path.
 *
 * @return  true  - The normal-access sequence produced every expected action;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_normal_access(void)
{
    LCS_TEST_REQUIRE(LCS_test_activate_locked());

    /* Authentication feedback has no meaning before a candidate is submitted. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_AUTH_SUCCESS, 
                           LCS_ACTION_NONE);

    /* An incomplete candidate refreshes entry; a complete one requests authentication. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED,
                           LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_INCOMPLETE,
                           LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_READY,
                           LCS_ACTION_REQUEST_AUTHENTICATION);

    /* Late request reclassification cannot modify an authentication already in progress. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_REGISTER_REQUESTED, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_AUTH_SUCCESS, 
                           LCS_ACTION_REQUEST_UNLOCK);

    /* Ignore unrelated feedback before completing the shared relock path. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_REGISTER_DONE, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_NONE, 
                           LCS_ACTION_NONE);

    LCS_TEST_REQUIRE(LCS_test_complete_unlocked_access());

    return true;
}

/**
 * @brief   Validates cancellation and inactivity-timeout exits from normal credential entry.
 *
 * @details Proves both terminal paths independently by returning to locked idle after cancellation, entering again, and then
 *          returning through the timeout-specific feedback action.
 *
 * @return  true  - Both normal-entry exit routes matched the contract;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_normal_exit_paths(void)
{
    LCS_TEST_REQUIRE(LCS_test_activate_locked());

    /* Empty-candidate cancellation ends and erases the active entry session. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED,
                           LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_CANCELLED,
                           LCS_ACTION_END_CREDENTIAL_ENTRY_SESSION);

    /* Re-entry proves cancellation restored locked idle before timeout is validated. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED,
                           LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_ENTRY_TIMEOUT,
                           LCS_ACTION_RETURN_TO_LOCKED_FROM_ENTRY_TIMEOUT);

    return true;
}

/**
 * @brief   Validates the three-failure authentication limit, lockout gate and post-lockout counter reset.
 *
 * @details The first two denial-feedback expirations return to locked idle; the third enters lockout. Entry is rejected during
 *          lockout. After lockout timeout, one additional failure remains below the limit, proving the counter was reset.
 *
 * @return  true  - Failure counting, lockout and reset behavior matched policy;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_authentication_lockout(void)
{
    LCS_TEST_REQUIRE(LCS_test_activate_locked());

    /* Three consecutive rejections consume the complete authentication budget. */
    LCS_TEST_REQUIRE(LCS_test_reject_one_authentication(LCS_ACTION_RETURN_TO_LOCKED));

    LCS_TEST_REQUIRE(LCS_test_reject_one_authentication(LCS_ACTION_RETURN_TO_LOCKED));

    LCS_TEST_REQUIRE(LCS_test_reject_one_authentication(LCS_ACTION_ENTER_LOCKOUT));

    /* Lockout rejects entry until its own timeout expires. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_LOCKOUT_TIMEOUT, 
                           LCS_ACTION_RETURN_TO_LOCKED);

    /* A single new rejection shall not immediately re-enter lockout. */
    LCS_TEST_REQUIRE(LCS_test_reject_one_authentication(LCS_ACTION_RETURN_TO_LOCKED));

    return true;
}

/**
 * @brief   Validates that successful authentication clears earlier consecutive failures.
 *
 * @details Two failures are followed by a successful unlock. Two further failures shall each remain below the lockout limit. The
 *          result proves counter reset indirectly through later guard selection without inspecting private runtime state.
 *
 * @return  true  - Later behavior proved that authentication success reset failure history;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_authentication_success_resets_counter(void)
{
    LCS_TEST_REQUIRE(LCS_test_activate_locked());

    /* Establish failure history below the limit. */
    LCS_TEST_REQUIRE(LCS_test_reject_one_authentication(LCS_ACTION_RETURN_TO_LOCKED));

    LCS_TEST_REQUIRE(LCS_test_reject_one_authentication(LCS_ACTION_RETURN_TO_LOCKED));

    /* Successful authentication shall reset the private counter. */
    LCS_TEST_REQUIRE(LCS_test_reach_authentication_from_locked(false));

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_AUTH_SUCCESS, 
                           LCS_ACTION_REQUEST_UNLOCK);

    LCS_TEST_REQUIRE(LCS_test_complete_unlocked_access());

    /* Two new failures remaining under the limit prove the reset occurred. */
    LCS_TEST_REQUIRE(LCS_test_reject_one_authentication(LCS_ACTION_RETURN_TO_LOCKED));

    LCS_TEST_REQUIRE(LCS_test_reject_one_authentication(LCS_ACTION_RETURN_TO_LOCKED));

    return true;
}

/**
 * @brief   Validates rejected credential-register authorization and cleanup of its pending purpose.
 *
 * @details A registration request is authenticated and rejected. After denial feedback, a new normal entry followed by successful
 *          authentication shall unlock rather than enter registration, proving the earlier pending purpose was cleared.
 *
 * @return  true  - Denial and pending-purpose cleanup were both observed;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_registration_authorization_failure(void)
{
    LCS_TEST_REQUIRE(LCS_test_activate_locked());

    /* Reject authorization for credential replacement. */
    LCS_TEST_REQUIRE(LCS_test_reach_authentication_from_locked(true));

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_AUTH_FAILURE, 
                           LCS_ACTION_DENY_ACCESS);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_DENIED_ACCESS_TIMEOUT, 
                           LCS_ACTION_RETURN_TO_LOCKED);

    /* The next normal success shall follow unlock routing, not stale registration routing. */
    LCS_TEST_REQUIRE(LCS_test_reach_authentication_from_locked(false));

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_AUTH_SUCCESS, 
                           LCS_ACTION_REQUEST_UNLOCK);

    LCS_TEST_REQUIRE(LCS_test_complete_unlocked_access());

    return true;
}

/**
 * @brief   Validates successful initial credential registration when storage contains no credential.
 *
 * @details LCS_EVENT_CREDENTIAL_NOT_REGISTERED shall bypass authentication and start first entry. Matching stages are persisted,
 *          success feedback completes, and a subsequent entry request proves the FSM reached normal locked idle.
 *
 * @return  true  - The complete first-boot registration route matched the contract;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_first_boot_registration_success(void)
{
    /* First boot enters registration directly because no installed secret can authorize it. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_NOT_REGISTERED,
                           LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION);

    /* Matching candidates shall be stored and followed by bounded success feedback. */
    LCS_TEST_REQUIRE(LCS_test_reach_persistence_from_first_entry());

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_SUCCESS,
                           LCS_ACTION_END_CREDENTIAL_REGISTER_SAVING_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_REGISTER_DONE,
                           LCS_ACTION_RETURN_TO_LOCKED_FROM_CREDENTIAL_REGISTER_SESSION);

    /* Acceptance of normal entry proves feedback completion restored locked idle. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED,
                           LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION);

    return true;
}

/**
 * @brief   Validates successful credential replacement after authenticating the installed credential.
 *
 * @details Normal boot is followed by registration authorization, first entry, confirmation, validation, persistence and success
 *          feedback. An event invalid during feedback is ignored before completion restores locked idle.
 *
 * @return  true  - The complete authorized-registration route matched the contract;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_authorized_registration_success(void)
{
    LCS_TEST_REQUIRE(LCS_test_activate_locked());

    LCS_TEST_REQUIRE(LCS_test_reach_first_entry_from_locked());

    LCS_TEST_REQUIRE(LCS_test_reach_persistence_from_first_entry());

    /* Persistent verification success starts registration-success feedback. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_SUCCESS,
                           LCS_ACTION_END_CREDENTIAL_REGISTER_SAVING_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_READY, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_REGISTER_DONE,
                           LCS_ACTION_RETURN_TO_LOCKED_FROM_CREDENTIAL_REGISTER_SESSION);

    return true;
}

/**
 * @brief   Validates confirmation retries, third-mismatch abortion and mismatch-counter reset.
 *
 * @details The first two staging mismatches restart only confirmation entry while retaining the staged first candidate. The third
 *          mismatch aborts registration. A new authorized session then receives another first-mismatch retry, proving independent
 *          mismatch history was reset by the terminal path.
 *
 * @return  true  - Retry-limit guards and terminal cleanup behaved as specified;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_registration_mismatch_limit(void)
{
    /* First boot provides the shortest deterministic route into registration. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_NOT_REGISTERED,
                           LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION);

    LCS_TEST_REQUIRE(LCS_test_reach_validation_from_first_entry());

    /* First mismatch restarts confirmation and permits an incomplete-entry refresh. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_STAGING_VALIDATION_FAILURE,
                           LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_INCOMPLETE,
                           LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_READY,
                           LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STAGES_VALIDATION);

    /* Second mismatch consumes the final retry but still returns to confirmation entry. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_STAGING_VALIDATION_FAILURE,
                           LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_READY,
                           LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STAGES_VALIDATION);

    /* Third mismatch aborts registration and restores locked idle. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_STAGING_VALIDATION_FAILURE,
                           LCS_ACTION_END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_REGISTER_DONE, 
                           LCS_ACTION_NONE);

    /* A new session shall receive a fresh first-mismatch retry. */
    LCS_TEST_REQUIRE(LCS_test_reach_first_entry_from_locked());

    LCS_TEST_REQUIRE(LCS_test_reach_validation_from_first_entry());

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_STAGING_VALIDATION_FAILURE,
                           LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION);

    return true;
}

/**
 * @brief   Validates refresh, cancellation and inactivity-timeout behavior during registration first entry.
 *
 * @details An incomplete first candidate restarts that phase. Cancellation aborts the session and restores locked idle. A new
 *          authorized session is then timed out to prove the independent timeout exit uses the same safe cleanup action.
 *
 * @return  true  - Every first-entry lifecycle path returned the expected action;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_registration_first_entry_exit_paths(void)
{
    LCS_TEST_REQUIRE(LCS_test_activate_locked());

    LCS_TEST_REQUIRE(LCS_test_reach_first_entry_from_locked());

    /* Incomplete candidate stays in first-entry collection; cancellation terminates it. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_INCOMPLETE,
                           LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_CANCELLED,
                           LCS_ACTION_END_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION);

    /* Reauthorization proves cancellation restored locked idle before timeout is tested. */
    LCS_TEST_REQUIRE(LCS_test_reach_first_entry_from_locked());

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_ENTRY_TIMEOUT,
                           LCS_ACTION_END_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION);

    return true;
}

/**
 * @brief   Validates refresh, cancellation and inactivity-timeout behavior during registration confirmation entry.
 *
 * @details Cancellation aborts one confirmation session. A second authorized session demonstrates that an incomplete candidate
 *          refreshes only confirmation entry and that its timeout erases both transient registration stages.
 *
 * @return  true  - Every confirmation-entry lifecycle path returned the expected action;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_registration_confirm_entry_exit_paths(void)
{
    LCS_TEST_REQUIRE(LCS_test_activate_locked());

    LCS_TEST_REQUIRE(LCS_test_reach_first_entry_from_locked());

    /* Reach confirmation and validate its explicit cancellation action. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_READY,
                           LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_TO_CONFIRM_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_CANCELLED,
                           LCS_ACTION_END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION);

    /* A second session validates confirmation refresh followed by timeout cleanup. */
    LCS_TEST_REQUIRE(LCS_test_reach_first_entry_from_locked());

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_READY,
                           LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_TO_CONFIRM_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_INCOMPLETE,
                           LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_ENTRY_TIMEOUT,
                           LCS_ACTION_END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION);

    return true;
}

/**
 * @brief   Validates fail-safe behavior after Credential Storage reports registration failure.
 *
 * @details A valid first-boot registration is driven to persistence. Storage failure shall request controlled reset and enter the
 *          fault state. Later feedback and entry events are ignored, proving normal operation cannot resume in the same runtime.
 *
 * @return  true  - Storage failure selected and preserved the fault policy;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_registration_storage_failure(void)
{
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_NOT_REGISTERED,
                           LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION);

    LCS_TEST_REQUIRE(LCS_test_reach_persistence_from_first_entry());

    /* Persistent failure is critical and shall move the FSM to fault. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_FAILURE,
                           LCS_ACTION_REQUEST_CONTROLLED_RESET);

    /* No ordinary event may escape fault. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_REGISTER_DONE, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED, 
                           LCS_ACTION_NONE);

    return true;
}

/**
 * @brief   Validates range rejection and state preservation for invalid or out-of-context events.
 *
 * @details Dispatches the enumeration sentinel, an out-of-range value and valid events that do not belong to the current state.
 *          Valid follow-up events advance through authentication, unlock, door confirmation and ready-to-lock. Additional invalid
 *          events in those states prove ignored input neither transitions nor corrupts the active path.
 *
 * @return  true  - Every invalid event returned no action and the valid path remained intact;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_invalid_events_preserve_state(void)
{
    LCS_TEST_REQUIRE(LCS_test_activate_locked());

    /* Sentinel, out-of-range and wrong-state events shall preserve locked idle. */
    LCS_TEST_EXPECT_ACTION((LCS_Event_t)LCS_EVENT_COUNT, 
                                        LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION((LCS_Event_t)((uint32_t)LCS_EVENT_COUNT + 1U), 
                                                   LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_AUTH_SUCCESS, 
                           LCS_ACTION_NONE);

    /* A valid request proves the ignored events did not prevent normal entry. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED,
                           LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_INIT_OK, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_READY,
                           LCS_ACTION_REQUEST_AUTHENTICATION);

    /* Wrong-state events during authentication, unlock and confirmation shall preserve progress. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CANDIDATE_READY, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_AUTH_SUCCESS, 
                           LCS_ACTION_REQUEST_UNLOCK);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_ENTRY_TIMEOUT, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_DOOR_SENSOR_CONFIRMATION_TIMEOUT, 
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_DOOR_POSITION_CONFIRMED,
                       LCS_ACTION_BEGIN_DOOR_SENSOR_CONFIRMATION);

    /* Wrong-state event shall preserve door-sensor confirmation. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_ENTRY_TIMEOUT,
                           LCS_ACTION_NONE);

    /* Confirmation timeout now advances to the ready-to-lock state. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_DOOR_SENSOR_CONFIRMATION_TIMEOUT,
                           LCS_ACTION_REQUEST_DOOR_SENSOR_CONFIRMATION);

    /* Events unrelated to READY_TO_LOCK shall preserve that state. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED,
                           LCS_ACTION_NONE);

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_DOOR_POSITION_CONFIRMED,
                           LCS_ACTION_NONE);

    /* Explicit readiness finally authorizes return to locked idle. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_READY_TO_LOCK,
                           LCS_ACTION_RETURN_TO_LOCKED_FROM_GRANTED_ACCESS);

    /* A valid entry request proves that the ready-to-lock transition actually restored locked idle. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED,
                           LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION);

    return true;
}

/**
 * @brief   Validates request-to-exit access from locked idle through bounded relock completion.
 *
 * @details A request-to-exit event shall unlock access without credential authentication. The shared relock helper then advances
 *          through door-position confirmation, the bounded confirmation timeout and explicit ready-to-lock authorization. A
 *          subsequent credential-entry request proves the FSM returned to normal locked idle.
 *
 * @return  true  - Every request-to-exit transition and final locked-idle restoration matched the contract;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_exit_request_access(void)
{
    LCS_TEST_REQUIRE(LCS_test_activate_locked());

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_EXIT_REQUEST,
                           LCS_ACTION_EXIT_REQUEST_UNLOCK);

    LCS_TEST_REQUIRE(LCS_test_complete_unlocked_access());

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED,
                           LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION);

    return true;
}

/**
 * @brief   Validates that request-to-exit access preserves prior authentication-failure history.
 *
 * @details Two rejected authentication cycles establish failure history below the lockout threshold. A request-to-exit cycle then
 *          unlocks and relocks the door without resetting that history. One subsequent authentication failure shall therefore
 *          consume the third consecutive attempt and enter lockout when denial feedback completes.
 *
 * @return  true  - Request-to-exit preserved the authentication counter and the next failure entered lockout;
 * @return  false - At least one returned action differed.
 */
static bool LCS_test_exit_request_preserves_auth_failure_counter(void)
{
    LCS_TEST_REQUIRE(LCS_test_activate_locked());

    /* Establish two consecutive authentication failures below the lockout limit. */
    LCS_TEST_REQUIRE(LCS_test_reject_one_authentication(LCS_ACTION_RETURN_TO_LOCKED));

    LCS_TEST_REQUIRE(LCS_test_reject_one_authentication(LCS_ACTION_RETURN_TO_LOCKED));

    LCS_TEST_EXPECT_ACTION(LCS_EVENT_EXIT_REQUEST,
                           LCS_ACTION_EXIT_REQUEST_UNLOCK);

    LCS_TEST_REQUIRE(LCS_test_complete_unlocked_access());

    LCS_TEST_REQUIRE(LCS_test_reject_one_authentication(LCS_ACTION_ENTER_LOCKOUT));

    return true;
}

/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**
 * @brief   Complete registry of scenarios implemented by this executable.
 *
 * @details main() uses this immutable table for command-line lookup and usage output. Tests/CMakeLists.txt contains the matching
 *          CTest registry because CMake must know the individual processes at configuration time.
 *
 * @note    Keep names unique and synchronized with LCS_FSM_TEST_SCENARIOS and the scenario catalog in Tests/README.md.
 */
static const LCS_TestCase_t LCS_TestCases[] =
{
    {"inactive_gate",                          LCS_test_inactive_gate},
    {"boot_failure",                           LCS_test_boot_failure},
    {"normal_access",                          LCS_test_normal_access},
    {"normal_exit_paths",                      LCS_test_normal_exit_paths},
    {"authentication_lockout",                 LCS_test_authentication_lockout},
    {"authentication_success_resets_counter",  LCS_test_authentication_success_resets_counter},
    {"registration_authorization_failure",     LCS_test_registration_authorization_failure},
    {"first_boot_registration_success",        LCS_test_first_boot_registration_success},
    {"authorized_registration_success",        LCS_test_authorized_registration_success},
    {"registration_mismatch_limit",            LCS_test_registration_mismatch_limit},
    {"registration_first_entry_exit_paths",    LCS_test_registration_first_entry_exit_paths},
    {"registration_confirm_entry_exit_paths",  LCS_test_registration_confirm_entry_exit_paths},
    {"registration_storage_failure",           LCS_test_registration_storage_failure},
    {"invalid_events_preserve_state",          LCS_test_invalid_events_preserve_state},
    {"exit_request_access",                    LCS_test_exit_request_access},
    {"exit_request_preserves_failure_counter", LCS_test_exit_request_preserves_auth_failure_counter}
};

/**
 * @brief   Number of entries in the immutable scenario registry.
 *
 * @details Deriving the count from LCS_TestCases prevents lookup and usage iteration bounds from diverging when a scenario is
 *          added or removed.
 */
#define LCS_TEST_CASE_COUNT (sizeof(LCS_TestCases) / sizeof(LCS_TestCases[0]))

/**********************************************************************************************************************************
 Private Function Definitions: Executable Front End
 **********************************************************************************************************************************/
/**
 * @brief Writes command syntax and every available scenario name to standard error.
 *
 * @param ProgramName - Executable path or name supplied by the host runtime as argv[0].
 */
static void LCS_TestPrintAvailableScenarios(const char* ProgramName)
{
    (void)fprintf(stderr, "usage: %s <scenario>\navailable scenarios:\n", ProgramName);

    for(size_t index = 0U; index < LCS_TEST_CASE_COUNT; index++)
    {
        (void)fprintf(stderr, "  %s\n", LCS_TestCases[index].name);
    }
}

/**
 * @brief   Selects and runs exactly one isolated LCS test scenario.
 *
 * @details CTest invokes this executable once per registered scenario. Requiring exactly one name prevents multiple scenarios
 *          from sharing the singleton state in one process. Unknown or missing names produce usage output and a failing process
 *          status. A selected scenario prints one concise PASS or FAIL record suitable for local execution and CI logs.
 *
 * @param   ArgumentCount - Number of command-line arguments supplied by the host runtime.
 * @param   Arguments     - Command-line argument array; Arguments[1] shall name one registered scenario.
 *
 * @return  EXIT_SUCCESS - The requested scenario exists and every expectation passed;
 * @return  EXIT_FAILURE - Invocation was invalid, the scenario was unknown, or at least one expectation failed.
 */
int main(int ArgumentCount, char** Arguments)
{
    if(ArgumentCount != 2)
    {
        LCS_TestPrintAvailableScenarios(Arguments[0]);

        return EXIT_FAILURE;
    }

    /* Resolve the stable external name without exposing function identifiers to CTest. */
    for(size_t index = 0U; index < LCS_TEST_CASE_COUNT; index++)
    {
        if(strcmp(Arguments[1], LCS_TestCases[index].name) == 0)
        {
            if(!LCS_TestCases[index].function())
            {
                (void)fprintf(stderr, "[FAIL] %s\n", LCS_TestCases[index].name);
                return EXIT_FAILURE;
            }

            (void)printf("[PASS] %s\n", LCS_TestCases[index].name);
            return EXIT_SUCCESS;
        }
    }

    (void)fprintf(stderr, "unknown scenario: %s\n", Arguments[1]);
    LCS_TestPrintAvailableScenarios(Arguments[0]);

    return EXIT_FAILURE;
}