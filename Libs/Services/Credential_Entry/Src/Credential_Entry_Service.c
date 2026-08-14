/**********************************************************************************************************************************
 * @file    Credential_Entry_Service.c
 * @brief   Credential Entry Service implementation.
 *
 * @details Implements the singleton runtime state and internal processing rules
 *          of the Credential Entry Service.
 *
 *          This module classifies semantic input commands, updates the active
 *          candidate credential when applicable and produces CES domain events
 *          for consumption by the Lock Controller.
 *
 *          The implementation contains no hardware access, time handling,
 *          authentication policy, lockout policy or application state-machine
 *          transitions.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Credential_Entry_Service.h"
#include "stdbool.h"
#include "stddef.h"
#include "string.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**
 * @brief   Internal classification of a semantic input command.
 *
 * @details Represents the intermediate result produced while validating an
 *          input command before the candidate credential or session state is
 *          updated.
 *
 *          This type is private to the module. Public callers receive only the
 *          final CES_Event_t returned by CES_ProcessInput().
 */
typedef enum
{
    CES_INPUT_RESULT_IDLE,           /*< No actionable input command was provided.            */

    CES_INPUT_RESULT_INVALID,        /*< The supplied digit is outside the decimal range.     */

    CES_INPUT_RESULT_DIGIT_ACCEPTED, /*< A valid decimal digit may be appended.               */

    CES_INPUT_RESULT_CONFIRM,        /*< Confirmation of the current candidate was requested. */

    CES_INPUT_RESULT_CLEAR_CANCEL,   /*< Clear-or-cancel processing was requested.            */

}CES_Result_t;

/**
 * @brief   Internal runtime state of the Credential Entry Service singleton.
 *
 * @details Owns the mutable state of the single credential-entry session
 *          supported by this module.
 *
 *          The candidate buffer contains normalized decimal digits and Length
 *          indicates how many entries of that buffer are currently valid.
 *
 * @note    All storage is statically allocated. This state is private and
 *          shall be accessed only through the CES public API and private
 *          helper functions in this translation unit.
 */
typedef struct
{
    CES_Input_t  input;                             /*< Most recently received semantic input command.   */
    CES_Digit_t  candidate[CES_CREDENTIAL_LENGTH];  /*< Storage for the current candidate credential.    */
    CES_Length_t length;                            /*< Number of valid digits in candidate.             */
    bool         session_is_active;                 /*< Indicates whether a session is currently active. */

}CES_Handle_t;

/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/**
 * @brief   Singleton runtime instance of the Credential Entry Service.
 *
 * @details Holds all mutable state owned by the module. The instance starts
 *          with no active session and an empty candidate credential.
 */
static CES_Handle_t CES_Instance =
{
    .candidate         = {0},
    .length            =  0,
    .session_is_active = false,
};

/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
static CES_Event_t CES_Update(CES_Result_t Result);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Reports whether a credential-entry session is active.
 *
 * @note    This helper does not modify the Credential Entry Service state.\
 * 
 * @return  true when a session is active; otherwise false.
 */
static inline bool CES_IsActive(void)
{
    return CES_Instance.session_is_active;
}

/**
 * @brief   Validates a normalized decimal credential digit.
 *
 * @param   Digit -  Digit value to validate.
 *
 * @note    A value of 0U is a valid credential digit.
 * 
 * @return  true when Digit is in the inclusive range from 0U through 9U;
 *          otherwise false.
 */
static inline bool CES_IsDigitValid(CES_Digit_t Digit)
{
    return (Digit <= 9U) ? true : false;
}

/**
 * @brief   Reports whether the candidate credential reached its fixed capacity.
 *
 * @note    Buffer fullness shall be determined from the stored candidate length,
 *          not from candidate digit values, because 0U is a valid digit.
 * 
 * @return  true when the candidate contains CES_CREDENTIAL_LENGTH valid digits;
 *          otherwise false.
 */
static inline bool CES_BufferIsFull(void)
{
    return CES_Instance.length == CES_CREDENTIAL_LENGTH ? true : false;
}

/**
 * @brief   Erases the current candidate credential.
 *
 * @details Clears all candidate buffer storage and sets the current candidate
 *          length to zero.
 *
 * @note    This helper does not change the active-session state.
 */
static inline void CES_EraseCandidate(void)
{
    memset(CES_Instance.candidate, 0, sizeof(CES_Instance.candidate));

    CES_Instance.length = 0;
}

/**
 * @brief   Applies an internal input result to the active CES session.
 *
 * @details Updates the candidate credential or session state when required by
 *          Result. A command that cannot modify the current session state,
 *          including an accepted digit when the candidate is full, returns
 *          CES_EVENT_NONE.
 *
* @param    Result - Internal classification of the most recently processed
 *                   semantic input command.
 * 
 * @note    This function is private to the module. Public callers shall use
 *          CES_ProcessInput().
 * 
 * @return  Domain event describing the observable outcome of the operation.
 */
static CES_Event_t CES_Update(CES_Result_t Result)
{
    if(!CES_IsActive())

        return CES_EVENT_NONE;

    switch(Result)
    {
        case CES_INPUT_RESULT_DIGIT_ACCEPTED:
        {
            if(!CES_BufferIsFull())
            {
                CES_Instance.candidate[CES_Instance.length] = CES_Instance.input.Digit;

                CES_Instance.length++;

                return CES_EVENT_INPUT_ACCEPTED;
            }

            else
            {
                return CES_EVENT_NONE;
            }
        }

        case CES_INPUT_RESULT_CONFIRM:
        {
            return ((CES_Instance.length) == CES_CREDENTIAL_LENGTH) ?
                                             CES_EVENT_READY        :
                                             CES_EVENT_INCOMPLETE   ;
        }

        case CES_INPUT_RESULT_CLEAR_CANCEL:
        {
            if(CES_Instance.length != 0)
            {
                CES_EraseCandidate();

                return CES_EVENT_CLEARED;
            }

            else 
            {
                CES_EndSession();

                return CES_EVENT_CANCELLED;
            }
        }

        default: 
            return CES_EVENT_NONE;
    }
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Starts a new credential-entry session.
 *
 * @details Clears any residual candidate credential and marks the CES session
 *          as active.
 *
 * @note    A successful call establishes an empty candidate credential.
 *
 * @return  CES_OPERATION_OK   - When a new session was started successfully;
 *          CES_OPERATION_FAIL - When another session is already active.
 */
CES_OpStatus_t CES_BeginSession(void)
{
    if(CES_IsActive())

        return CES_OPERATION_FAIL;

    CES_EraseCandidate();

    CES_Instance.session_is_active = true;
    
    return CES_OPERATION_OK;
}

/**
 * @brief   Ends the active credential-entry session.
 *
 * @details Erases the current candidate credential and marks the CES session
 *          as inactive.
 *
 * @note    The Lock Controller shall call this function when the user cancels,
 *          the Timeout Validation Service reports expiration, authentication
 *          processing is complete or the application leaves credential entry.
 *
 * @return  CES_OPERATION_OK   - When the active session was ended successfully;
 *          CES_OPERATION_FAIL - When no session is active.
 */
CES_OpStatus_t CES_EndSession(void)
{
    if(!CES_IsActive())

        return CES_OPERATION_FAIL;

    CES_EraseCandidate();

    CES_Instance.session_is_active = false;
 
    return CES_OPERATION_OK;
}

/**
 * @brief   Processes one semantic credential-entry input command.
 *
 * @details Classifies the received command and updates the active candidate
 *          credential or session when applicable.
 *
 *          CES_EVENT_NONE is returned when no session is active, the input kind
 *          is unsupported, a supplied digit is invalid or a digit is submitted
 *          after the candidate buffer is full.
 *
 * @param   Input - Pointer to the semantic command supplied by the application
 *                  layer.
 *
 * @note    This function does not validate timeout state, authenticate the
 *          candidate credential or perform application state transitions.
 *
 * @return  A CES domain event describing the observable processing outcome.
 */
CES_Event_t CES_ProcessInput(const CES_Input_t* Input)
{
    if(Input == NULL || !CES_IsActive())
    {
        return CES_EVENT_NONE;
    }

    CES_Instance.input.Kind  = Input->Kind;
    CES_Instance.input.Digit = Input->Digit;

    CES_Result_t result = CES_INPUT_RESULT_IDLE;

    switch(CES_Instance.input.Kind)
    {
        case CES_INPUT_KIND_DIGIT:
            result = (!CES_IsDigitValid(CES_Instance.input.Digit)) ?
                       CES_INPUT_RESULT_INVALID                    :
                       CES_INPUT_RESULT_DIGIT_ACCEPTED             ;
        break;

        case CES_INPUT_KIND_CONFIRM: 
            result = CES_INPUT_RESULT_CONFIRM; 
        break;

        case CES_INPUT_KIND_CLEAR_CANCEL: 
            result = CES_INPUT_RESULT_CLEAR_CANCEL; 
        break;

        default: 
            result =  CES_INPUT_RESULT_IDLE; 
        break;
    }

    return CES_Update(result);
}

/**
 * @brief   Returns the number of valid digits in the active candidate.
 *
 * @details Reports the current amount of valid digits stored in the internal
 *          candidate credential buffer.
 *
 * @note    The returned length is always in the inclusive range from 0U through
 *          CES_CREDENTIAL_LENGTH.
 *
 * @return  Current candidate length when a session is active; otherwise 0XFFU.
 */
CES_Length_t CES_GetCurrentLength(void)
{
    return (!CES_IsActive()) ? 0xFFU : CES_Instance.length;
}

/**
 * @brief   Copies the complete candidate into a caller-provided object.
 *
 * @details Copies the internal candidate credential, preserving digit order,
 *          into the fixed-size Digits array owned by Candidate.
 *
 *          When the operation succeeds, Candidate->Length is set to
 *          CES_CREDENTIAL_LENGTH.
 *
 *          This operation does not erase the internal candidate, change its
 *          length or end the active credential-entry session.
 *
 * @param   Candidate - Pointer to the caller-owned object that receives the
 *                      complete candidate credential.
 *
 * @note    This function shall be called only after CES_ProcessInput()
 *          returns CES_EVENT_READY.
 *
 * @note    After a successful copy, the Lock Controller shall immediately call
 *          CES_EndSession() to erase the internal candidate and end the active
 *          session before continuing authentication with the copied data.
 *
 * @note    Ownership of Candidate and its credential storage remains with the
 *          caller.
 *
 * @warning The caller is responsible for erasing the copied credential after
 *          authentication processing is complete.
 *
 * @return  CES_OPERATION_OK   - When the complete candidate was copied
 *                               successfully;
 *          CES_OPERATION_FAIL - When Candidate is NULL, no session is active
 *                               or the internal candidate is incomplete.
 */
CES_OpStatus_t CES_GetCandidate(CES_Candidate_t* Candidate)
{
    if(Candidate == NULL || !CES_IsActive() || !CES_BufferIsFull())

        return CES_OPERATION_FAIL; 

    memcpy(Candidate->Digits,CES_Instance.candidate,sizeof(CES_Instance.candidate));

    Candidate->Length =  CES_Instance.length;

    return CES_OPERATION_OK;
}
