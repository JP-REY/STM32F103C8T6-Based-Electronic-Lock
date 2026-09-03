/**********************************************************************************************************************************
 * @file    App_Executor.c
 * @brief   Executes semantic Lock Control actions for the electronic-lock application.
 *
 * @details Converts each LCS_Action_t selected by the authoritative Lock Control state machine into concrete application side effects
 *          across Credential Entry, Authentication, Credential Register, Credential Storage, application-owned timeout management,
 *          Door Control and presentation services.
 *
 *          Operations with an immediate semantic result return an LCS_Event_t follow-up to App Core, allowing the bounded synchronous
 *          Lock Control action/event dispatch chain to continue without moving product-state decisions into the executor.
 *
 *          The executor borrows the runtime-object registry and credential buffers owned by App Config. It also owns the common
 *          controlled-reset endpoint used for terminal application faults, including best-effort restoration of the configured safe
 *          locked request, cancellation of transient activity and explicit erasure of sensitive runtime credential material before
 *          requesting system reset.
 *
 * @note    Product-state transitions and policy remain exclusively owned by the Lock Control Service. App Executor performs only the
 *          side effects selected by LCS actions and reports immediate outcomes as semantic events.
 *
 * @note    Execution is synchronous, serialized and non-reentrant. App Core is the only intended caller.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    Aug 30, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "App_Executor.h"
#include "App_Core_Internal.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
/*---------------------------------------------------------------------------------------------------------------------------------
 Credential Entry and Authentication
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Copies and erases the CES candidate, authenticates it and returns the corresponding LCS event. */
static LCS_Event_t App_ProcessAuthentication(void);

/** @brief Explicitly erases the application-owned transient candidate digits. */
static void App_ClearRuntime_Candidate(void);

/*---------------------------------------------------------------------------------------------------------------------------------
 Lock Actuator Control
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Forces the lock-actuator GPIO into its safe locked state. */
static DCS_RequestLockStatus_t App_RequestLock(void);

/** @brief Requests actuator unlock after a finite unlock timeout has been established. */
static bool App_RequestUnlock(void);

/** @brief */
static bool App_ForceLock(void);

/*---------------------------------------------------------------------------------------------------------------------------------
 Presentation Coordination
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Restores the locked-idle display, backlight and lock-status indication policy. */
static void App_SetLockedPresentation(void);

/** @brief Presents a newly opened normal Credential Entry Service session. */
static void App_SetCESPresentation(void);

/** @brief Presents installed-credential authorization for a credential-replacement request. */
static void App_SetCRSAuthPresentation(void);

/** @brief Presents the first-entry phase of credential registration. */
static void App_SetCRSFirstEntryPresentation(void);

/** @brief Presents the confirmation-entry phase while CRS retains the staged credential. */
static void App_SetCRSConfirmEntryPresentation(void);

/** @brief Presents successful credential persistence during the bounded saved-feedback interval. */
static void App_SetCRSSavedPresentation(void);

/*---------------------------------------------------------------------------------------------------------------------------------
 Fail-Safe Reset Endpoint
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Disables interrupts and requests the target system reset after fail-safe cleanup is complete. */
static void App_RequestControlledReset(void);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Explicitly erases the application-owned candidate credential digits.
 *
 * @details Writes zero through a volatile byte pointer over the CES_CREDENTIAL_LENGTH digit bytes. Volatile access prevents the
 *          compiler from removing the erasure merely because the digits are not read afterward. The non-secret Length metadata
 *          member is not reset by this helper and is replaced by the next successful CES_GetCandidate() copy.
 *
 * @warning This helper erases only the registry's Runtime_Candidate digit array. CES_EndSession() remains responsible for erasing
 *          CES internal storage.
 */
static void App_ClearRuntime_Candidate(void)
{
    volatile uint8_t* candidate_bytes = (volatile uint8_t*)App_Instance->Runtime_Candidate;

    for(size_t index = 0U; index < CES_CREDENTIAL_LENGTH; index++)
    {
        candidate_bytes[index] = 0U;
    }
}

/**
 * @brief   Completes candidate transfer, authentication and credential erasure.
 *
 * @details Copies the complete candidate from CES, immediately ends the session to erase service-owned candidate storage,
 *          authenticates it against the installed runtime credential, and then explicitly erases the application candidate copy.
 *          The AS result is mapped into the semantic Lock Control event expected by the authoritative FSM.
 *
 * @note    App_ExecuteAction() ensures the installed runtime credential is valid before calling this helper. Candidate-copy or
 *          session-ending failures conservatively produce LCS_EVENT_AUTH_FAILURE; the installed runtime credential remains
 *          available for later authentication attempts.
 *
 * @return  LCS_EVENT_AUTH_SUCCESS - When AS authenticates the complete candidate.
 * @return  LCS_EVENT_AUTH_FAILURE - When candidate transfer, session cleanup or authentication does not succeed.
 */
static LCS_Event_t App_ProcessAuthentication(void)
{
    App_ClearRuntime_Candidate();

    if(CES_GetCandidate(App_Instance->Runtime_Candidate) != CES_OPERATION_OK)
    {
        (void)CES_EndSession();
        App_ClearRuntime_Candidate();

        return LCS_EVENT_AUTH_FAILURE;
    }

    if(CES_EndSession() != CES_OPERATION_OK)
    {
        App_ClearRuntime_Candidate();

        return LCS_EVENT_AUTH_FAILURE;
    }

    AS_Result_t authentication_result = AS_Authenticate(App_Instance->Runtime_Candidate->Digits, 
                                                        App_Instance->Runtime_Credential);

    App_ClearRuntime_Candidate();

    return (authentication_result == AS_RESULT_AUTHENTICATED) ?
                                     LCS_EVENT_AUTH_SUCCESS   :
                                     LCS_EVENT_AUTH_FAILURE   ;
}

/**
 * @brief   Requests a normal door-position-aware lock operation through the Door Control Service.
 *
 * @details Delegates the relock request to DCS_RequestLock(), which samples the current normalized Door Sensor state before commanding
 *          the Lock Actuator Driver. Only the configured lock-permissive sensor condition allows the actuator command; an idle or
 *          unknown sensor state denies the request without commanding the lock.
 *
 *          This function is intended for the normal relock path after Lock Control and App Core have completed the required
 *          door-position confirmation sequence. It does not bypass the Door Control safety interlock.
 *
 * @return  true  - When Door Control approves the request and the actuator accepts the lock command.
 * @return  false - When the door-sensor condition denies locking or the delegated actuator operation fails.
 */
static DCS_RequestLockStatus_t App_RequestLock(void)
{
    return DCS_RequestLock();
}

/**
 * @brief   Forces the lock actuator to the configured locked command through the Door Control Service.
 *
 * @details Delegates directly to DCS_ForceLock(), intentionally bypassing the normal door-sensor interlock. This operation exists for
 *          explicit fail-safe paths in which the application must request the configured safe actuator output independently from the
 *          current Door Sensor condition.
 *
 *          The function does not perform Door Sensor validation, retry the command or confirm the resulting mechanical lock position.
 *
 * @warning This function shall not replace App_RequestLock() in normal relock flow because it intentionally bypasses the door-position
 *          safety policy enforced by DCS_RequestLock().
 *
 * @return  true  - When the Door Control Service successfully commands the actuator to the locked state.
 * @return  false - When the delegated force-lock operation fails.
 */
static bool App_ForceLock(void)
{
    if(DCS_ForceLock() != DCS_OPERATION_OK)
    {
        return false;
    }

    return true;
}

/**
 * @brief   Requests an already-authorized physical unlock through the Door Control Service.
 *
 * @details Delegates the actuator command to DCS_RequestUnlock(). Authorization is established before this function is called by the
 *          Lock Control and application orchestration flow, whether through successful credential authentication or an accepted
 *          request-to-exit condition.
 *
 *          This function performs no authentication, request-to-exit validation, Door Sensor policy decision or relock timing. After
 *          unlock, door-position observation and the subsequent relock sequence remain coordinated separately by App Core, Door Control
 *          and Lock Control.
 *
 * @return  true  - When the Door Control Service successfully commands the actuator to the unlocked state.
 * @return  false - When the delegated actuator operation fails.
 */
static bool App_RequestUnlock(void)
{
    if(DCS_RequestUnlock() != DCS_OPERATION_OK)
    {
        return false;
    }

    return true;
}

/**
 * @brief   Restores the normal locked-idle presentation policy.
 *
 * @details Requests the blank locked-idle view, clears retained mask progress, synchronizes the LCD while its backlight is still
 *          available, turns the backlight off and selects the locked LED indication.
 *
 * @note    Presentation failures are treated as degraded UI behavior and cannot prevent the separate actuator safe-state request.
 * @note    This helper does not stop sound, allowing action-specific feedback to finish after the display returns to idle.
 */
static void App_SetLockedPresentation(void)
{
    (void)DRS_SetScreen(DRS_SCREEN_IDLE);
    (void)DRS_SetEnteredDigits(0U);
    (void)DRS_Update();
    (void)LCD_BacklightOff(App_Instance->Lcd);
    (void)SIS_SetIndication(App_Instance->Lock_Status_Indication, SIS_INDICATION_LOCKED);
}

/**
 * @brief   Presents a newly opened credential-entry session.
 *
 * @details Enables the LCD backlight, requests the empty masked-password screen, selects the normal locked LED baseline and emits a
 *          short keypress acknowledgement for the wake key. The initiating key is not passed to CES.
 */
static void App_SetCESPresentation(void)
{
    uint32_t current_time_ms = Platform_GetMillis();

    (void)SGS_Ring(SGS_RINGTONE_KEYPRESS, current_time_ms);
    (void)LCD_BacklightOn(App_Instance->Lcd);
    (void)DRS_SetScreen(DRS_SCREEN_PASSWORD_ENTRY);
    (void)DRS_SetEnteredDigits(0U);
    (void)DRS_Update();
    (void)SIS_SetIndication(App_Instance->Lock_Status_Indication, SIS_INDICATION_LOCKED);
}

/**
 * @brief   Presents installed-credential authorization for a replacement request.
 *
 * @details Acknowledges the registration command, enables the backlight, requests the fixed Access PIN prompt and clears retained
 *          mask progress before the authorization candidate is collected.
 */
static void App_SetCRSAuthPresentation(void)
{
    uint32_t current_time_ms = Platform_GetMillis();

    (void)SGS_Ring(SGS_RINGTONE_KEYPRESS, current_time_ms);
    (void)LCD_BacklightOn(App_Instance->Lcd);
    (void)DRS_SetScreen(DRS_SCREEN_CREDENTIAL_REGISTER_AUTH);
    (void)DRS_SetEnteredDigits(0U);
    (void)DRS_Update();
}

/**
 * @brief   Presents a newly opened credential-register first-entry session.
 *
 * @details Acknowledges the phase transition, enables the backlight, requests the fixed Update PIN prompt and clears retained mask
 *          progress before the proposed credential is collected.
 */
static void App_SetCRSFirstEntryPresentation(void)
{
    uint32_t current_time_ms = Platform_GetMillis();

    (void)SGS_Ring(SGS_RINGTONE_KEYPRESS, current_time_ms);
    (void)LCD_BacklightOn(App_Instance->Lcd);
    (void)DRS_SetScreen(DRS_SCREEN_CREDENTIAL_REGISTER_FIRST_ENTRY);
    (void)DRS_SetEnteredDigits(0U);
    (void)DRS_Update();
}

/**
 * @brief   Presents a newly opened credential-register confirmation-entry session.
 *
 * @details Acknowledges the phase transition, enables the backlight, requests the fixed Confirm PIN prompt and clears retained mask
 *          progress while CRS preserves the staged first entry.
 */
static void App_SetCRSConfirmEntryPresentation(void)
{
    uint32_t current_time_ms = Platform_GetMillis();

    (void)SGS_Ring(SGS_RINGTONE_KEYPRESS, current_time_ms);
    (void)LCD_BacklightOn(App_Instance->Lcd);
    (void)DRS_SetScreen(DRS_SCREEN_CREDENTIAL_REGISTER_CONFIRM_ENTRY);
    (void)DRS_SetEnteredDigits(0U);
    (void)DRS_Update();
}

/**
 * @brief   Presents successful credential-persistence feedback.
 *
 * @details Enables the backlight, requests the fixed PIN-updated screen and starts the access-granted sound pattern. The action
 *          executor separately owns the bounded APP_TIMEOUT_CRS_SAVED interval.
 */
static void App_SetCRSSavedPresentation(void)
{
    uint32_t current_time_ms = Platform_GetMillis();

    (void)SGS_Ring(SGS_RINGTONE_ACCESS_GRANTED, current_time_ms);
    (void)LCD_BacklightOn(App_Instance->Lcd);
    (void)DRS_SetScreen(DRS_SCREEN_CREDENTIAL_REGISTER_SAVED);
    (void)DRS_Update();
}

/**
 * @brief   Executes the target-controlled reset endpoint after outputs are safe.
 *
 * @details Disables interrupt activity and invokes the CMSIS system-reset request. Callers must force the actuator safe, erase
 *          credentials, stop sound and disable the LCD backlight before entering this endpoint.
 *
 * @note    This function is not expected to return on the STM32 target.
 */
static void App_RequestControlledReset(void)
{
    __disable_irq();
    NVIC_SystemReset();

    for(;;){}
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Executes one semantic Lock Control action and optionally produces a synchronous follow-up event.
 *
 * @details Coordinates CES sessions, CRS staging and validation, CSS loading and persistence, authentication, timeout ownership,
 *          actuator safety and presentation according to the action selected by LCS. Synchronous outcomes are returned as
 *          follow-up events so they are dispatched only after the current LCS_Process() call has returned.
 *
 * @note    The function-local runtime_credential_valid flag prevents repeated Flash reads after the installed credential has been
 *          loaded or successfully replaced. The controlled-reset path erases the runtime reference and clears that flag.
 *
 * @param   Action - Semantic action returned by LCS_Process().
 *
 * @return  A synchronous semantic follow-up event, or LCS_EVENT_NONE when the action is complete.
 */
LCS_Event_t App_ExecuteAction(LCS_Action_t Action)
{
    static bool runtime_credential_valid = false;

    uint32_t current_time_ms = Platform_GetMillis();

    switch(Action)
    {
        case LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION:

            (void)CRS_ClearStaging();

            App_ClearRuntime_Candidate();

            if(CES_BeginSession() != CES_OPERATION_OK ||
              !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                (void)CES_EndSession();
                (void)CRS_ClearStaging();

                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            App_SetCRSFirstEntryPresentation();

        break;

        case LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION:

            if(CES_RefreshSession() != CES_OPERATION_OK ||
              !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            (void)SGS_Ring(SGS_RINGTONE_ENTRY_INCOMPLETE, current_time_ms);

            App_SetCRSFirstEntryPresentation();

        break;

        case LCS_ACTION_END_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)CRS_ClearStaging();

            App_ClearRuntime_Candidate();

            (void)App_RequestLock();

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_TO_CONFIRM_ENTRY_SESSION:
        {
            App_CancelTimeout();

            App_ClearRuntime_Candidate();

            if(CES_GetCandidate(App_Instance->Runtime_Candidate) != CES_OPERATION_OK)
            {
                goto controlled_reset;
            }

            CRS_OpStatus_t staging_status = CRS_StageCredential(App_Instance->Runtime_Candidate->Digits);

            App_ClearRuntime_Candidate();

            if(staging_status != CRS_OPERATION_OK)
            {
                goto controlled_reset;
            }

            if(CES_RefreshSession() != CES_OPERATION_OK ||
              !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                (void)CES_EndSession();
                (void)CRS_ClearStaging();

                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            App_SetCRSConfirmEntryPresentation();
        }
        break;

        case LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION:

            if(CES_RefreshSession() != CES_OPERATION_OK ||
              !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            (void)SGS_Ring(SGS_RINGTONE_ENTRY_INCOMPLETE, current_time_ms);

            App_SetCRSConfirmEntryPresentation();

        break;

        case LCS_ACTION_END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)CRS_ClearStaging();

            App_ClearRuntime_Candidate();

            (void)App_RequestLock();

            (void)SGS_Ring(SGS_RINGTONE_ERROR, current_time_ms);

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_SAVING_SESSION:

            App_CancelTimeout();

        break;

        case LCS_ACTION_END_CREDENTIAL_REGISTER_SAVING_SESSION:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)CRS_ClearStaging();

            App_ClearRuntime_Candidate();

            (void)App_RequestLock();

            (void)SIS_SetIndication(App_Instance->Lock_Status_Indication, SIS_INDICATION_LOCKED);

            App_SetCRSSavedPresentation();

            (void)SGS_Ring(SGS_RINGTONE_ACCESS_GRANTED, current_time_ms);

            if(!App_StartTimeout(APP_TIMEOUT_CRS_SAVED))
            {
                return LCS_EVENT_CREDENTIAL_REGISTER_DONE;
            }

        break;

        case LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_TO_REGISTER_SESSION:

            if(CES_RefreshSession() != CES_OPERATION_OK ||
              !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                (void)CES_EndSession();

                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            App_SetCRSAuthPresentation();

        break;

        case LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION:

            if(CES_BeginSession() != CES_OPERATION_OK ||
              !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                (void)CES_EndSession();

                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            App_SetCESPresentation();

        break;

        case LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_SESSION:

            if(CES_RefreshSession() != CES_OPERATION_OK ||
              !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            (void)SGS_Ring(SGS_RINGTONE_ENTRY_INCOMPLETE, current_time_ms);

            App_SetCESPresentation();

        break;

        case LCS_ACTION_END_CREDENTIAL_ENTRY_SESSION:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)App_RequestLock();

            (void)SIS_SetIndication(App_Instance->Lock_Status_Indication, SIS_INDICATION_LOCKED);

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_REQUEST_AUTHENTICATION:

            App_CancelTimeout();

            if(!runtime_credential_valid)
            {
                if(CSS_GetCredential(App_Instance->Runtime_Credential) != CSS_OPERATION_OK)
                {
                    goto controlled_reset;
                }

                runtime_credential_valid = true;
            }

            return App_ProcessAuthentication();

        case LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STAGES_VALIDATION:
        {
            App_CancelTimeout();

            App_ClearRuntime_Candidate();

            if(CES_GetCandidate(App_Instance->Runtime_Candidate) != CES_OPERATION_OK)
            {
                goto controlled_reset;
            }

            CRS_ValidationResult_t validation_result =
                CRS_ValidateConfirmation(App_Instance->Runtime_Candidate->Digits);

            App_ClearRuntime_Candidate();

            if(validation_result == CRS_VALIDATION_MATCH)
            {
                return LCS_EVENT_STAGING_VALIDATION_SUCCESS;
            }

            if(validation_result == CRS_VALIDATION_MISMATCH)
            {
                return LCS_EVENT_STAGING_VALIDATION_FAILURE;
            }

            goto controlled_reset;
        }

        case LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STORAGE:
        {
            uint8_t temporary_credential[CRS_CREDENTIAL_LENGTH] = {0U};

            LCS_Event_t storage_event = LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_FAILURE;

            App_CancelTimeout();

            if(CRS_GetValidatedCredential(temporary_credential) == CRS_OPERATION_OK &&
               CSS_SaveCredential(temporary_credential) == CSS_OPERATION_OK)
            {
                for(size_t index = 0U; index < CES_CREDENTIAL_LENGTH; index++)
                {
                    App_Instance->Runtime_Credential[index] = temporary_credential[index];
                }

                runtime_credential_valid = true;

                storage_event = LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_SUCCESS;
            }

            (void)CRS_ClearStaging();

            volatile uint8_t* temporary_bytes = (volatile uint8_t*)temporary_credential;

            for(size_t index = 0U; index < sizeof(temporary_credential); index++)
            {
                temporary_bytes[index] = 0U;
            }

            return storage_event;
        }

        case LCS_ACTION_REQUEST_UNLOCK:

            if(!App_StartTimeout(APP_TIMEOUT_UNLOCK_HOLD))
            {
                return LCS_EVENT_UNLOCK_REQUEST_FAILED;
            }

            if(!App_RequestUnlock())
            {
                if(!App_ForceLock())
                {
                    return LCS_EVENT_CRITICAL_FAULT;
                }

                return LCS_EVENT_UNLOCK_REQUEST_FAILED;
            }

            (void)LCD_BacklightOn(App_Instance->Lcd);
            (void)DRS_SetScreen(DRS_SCREEN_ACCESS_GRANTED);
            (void)DRS_Update();
            (void)SIS_SetIndication(App_Instance->Lock_Status_Indication, SIS_INDICATION_ACCESS_GRANTED);
            (void)SGS_Ring(SGS_RINGTONE_ACCESS_GRANTED, current_time_ms);

        break;

        case LCS_ACTION_EXIT_REQUEST_UNLOCK:

            if(!App_StartTimeout(APP_TIMEOUT_UNLOCK_HOLD))
            {
                return LCS_EVENT_UNLOCK_REQUEST_FAILED;
            }

            if(!App_RequestUnlock())
            {
                if(!App_ForceLock())
                {
                    return LCS_EVENT_CRITICAL_FAULT;
                }

                return LCS_EVENT_UNLOCK_REQUEST_FAILED;
            }

            (void)SGS_Ring(SGS_RINGTONE_UNLOCKING, current_time_ms);

        break;

        case LCS_ACTION_FORCE_ACTUATOR_LOCK:

            App_CancelTimeout();

            if(!App_ForceLock())
            {
                return LCS_EVENT_CRITICAL_FAULT;
            }

            (void)SGS_Ring(SGS_RINGTONE_ENTRY_TIMEOUT, current_time_ms);

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_RESTART_UNLOCK_HOLD_TIMEOUT:

            if(!App_StartTimeout(APP_TIMEOUT_UNLOCK_HOLD))
            {
                return LCS_EVENT_CRITICAL_FAULT;
            }

        break;

        case LCS_ACTION_DENY_ACCESS:

            if(!App_StartTimeout(APP_TIMEOUT_ACCESS_DENIED))
            {
                return LCS_EVENT_DENIED_ACCESS_TIMEOUT;
            }

            (void)LCD_BacklightOn(App_Instance->Lcd);
            (void)DRS_SetScreen(DRS_SCREEN_ACCESS_DENIED);
            (void)DRS_Update();
            (void)SIS_SetIndication(App_Instance->Lock_Status_Indication, SIS_INDICATION_ACCESS_DENIED);
            (void)SGS_Ring(SGS_RINGTONE_ERROR, current_time_ms);

        break;

        case LCS_ACTION_ENTER_LOCKOUT:

            if(!App_StartTimeout(APP_TIMEOUT_LOCKOUT))
            {
                return LCS_EVENT_LOCKOUT_TIMEOUT;
            }

            (void)LCD_BacklightOn(App_Instance->Lcd);
            (void)DRS_SetScreen(DRS_SCREEN_LOCKOUT);
            (void)DRS_Update();
            (void)SIS_SetIndication(App_Instance->Lock_Status_Indication, SIS_INDICATION_LOCKOUT_ENTRY);
            (void)SGS_Ring(SGS_RINGTONE_LOCKOUT, current_time_ms);

        break;

        case LCS_ACTION_RETURN_TO_LOCKED_FROM_ENTRY_TIMEOUT:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)App_RequestLock();
            (void)SGS_Ring(SGS_RINGTONE_ENTRY_TIMEOUT, current_time_ms);

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_RETURN_TO_LOCKED:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)App_RequestLock();

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_BEGIN_DOOR_SENSOR_CONFIRMATION:

            (void)App_StartTimeout(APP_DOOR_SENSOR_CONFIRMATION_TIMEOUT);

        break;

        case LCS_ACTION_REQUEST_DOOR_SENSOR_CONFIRMATION:

            if(DCS_GetSensorStatus(App_Instance->Door_Sensor_Status) != DCS_OPERATION_OK)
            {
                goto controlled_reset;
            }

            if(*(App_Instance->Door_Sensor_Status) == DCS_SENSOR_STATUS_ACTIVE)
            {
                return LCS_EVENT_READY_TO_LOCK;
            }

            if(*(App_Instance->Door_Sensor_Status) == DCS_SENSOR_STATUS_IDLE)
            {
                return LCS_EVENT_DOOR_POSITION_NOT_CONFIRMED;
            }

            goto controlled_reset;

        case LCS_ACTION_RETURN_TO_LOCKED_FROM_GRANTED_ACCESS:

            (void)CES_EndSession();

            DCS_RequestLockStatus_t lock_status = App_RequestLock();

            if(lock_status == DCS_LOCK_REQUEST_DENIED)
            {
                return LCS_EVENT_DOOR_POSITION_NOT_CONFIRMED;
            }

            if(lock_status == DCS_LOCK_REQUEST_FAILED)
            {
                goto controlled_reset;
            }

            (void)SGS_Ring(SGS_RINGTONE_LOCKING, current_time_ms);

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_RETURN_FROM_CREDENTIAL_REGISTER_SESSION_FIRST_BOOT:

            App_RequestControlledReset();

        break;

        case LCS_ACTION_RETURN_TO_LOCKED_FROM_CREDENTIAL_REGISTER_SESSION:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)CRS_ClearStaging();

            App_ClearRuntime_Candidate();

            (void)App_RequestLock();

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_REQUEST_CONTROLLED_RESET:

            goto controlled_reset;

        default: case LCS_ACTION_NONE: break;
    }

    return LCS_EVENT_NONE;

controlled_reset:

    /* CRS integration faults share the same fail-safe cleanup as an LCS-requested reset. */
    runtime_credential_valid = false;

    App_CancelTimeout();

    (void)CES_EndSession();
    (void)CRS_ClearStaging();
    
    App_ClearRuntime_Candidate();

    volatile uint8_t* runtime_credential_bytes = (volatile uint8_t*)App_Instance->Runtime_Credential;

    for(size_t index = 0U; index < CES_CREDENTIAL_LENGTH; index++)
    {
        runtime_credential_bytes[index] = 0U;
    }

    (void)App_ForceLock();
    (void)SGS_Stop();
    (void)LCD_BacklightOff(App_Instance->Lcd);

    App_RequestControlledReset();

    return LCS_EVENT_NONE;
}
