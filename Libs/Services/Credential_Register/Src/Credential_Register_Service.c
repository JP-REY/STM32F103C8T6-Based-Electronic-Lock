/**********************************************************************************************************************************
 * @file    Credential_Register_Service.c
 * @brief   Credential Register Service implementation.
 *
 * @details Implements the private singleton, credential-staging lifecycle, decimal-digit validation, first-entry copy,
 *          confirmation comparison, validated-credential retrieval and explicit staging cleanup.
 *
 *          The runtime instance contains one six-byte credential buffer and one lifecycle discriminator. Public operations enforce
 *          the required EMPTY, STAGED and VALIDATED sequence without exposing either the buffer or state to the application. A
 *          mismatch retains the staged first entry, while explicit cleanup erases the buffer and permits a new registration.
 *
 *          This implementation performs no dynamic allocation, hardware access, persistent storage, inter-service call, timeout
 *          handling, retry counting or Lock Control transition. All external orchestration remains an App Core responsibility.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-21
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Credential_Register_Service.h"
#include "stddef.h"
#include "stdbool.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**
 * @brief Number of decimal-digit elements in the private staging buffer.
 *
 * @details Derives the iteration bound directly from CRS_RuntimeInstance.Credential so buffer traversal cannot diverge from the
 *          private array declaration if its storage representation changes.
 *
 * @note    The macro is expanded only after CRS_RuntimeInstance is declared and visible to the compiler.
 */
#define CRS_CREDENTIAL_SIZE (size_t)((sizeof(CRS_RuntimeInstance.Credential)) \
                                    /(sizeof(CRS_RuntimeInstance.Credential[0])))

/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**
 * @brief Private lifecycle states of the credential-staging singleton.
 *
 * @details Records whether the private buffer contains no usable data, a first entry awaiting confirmation or a credential whose
 *          confirmation matched and may therefore be copied for persistent storage.
 */
typedef enum
{
    CRS_STATE_EMPTY,      /*< No credential is staged; only staging and cleanup are permitted.          */
    CRS_STATE_STAGED,     /*< The first entry is retained and awaits one confirmation comparison.       */
    CRS_STATE_VALIDATED   /*< Confirmation matched; the credential may be retrieved before final clear. */

}CRS_State_t;

/**
 * @brief Private runtime representation of the Credential Register Service.
 *
 * @details Owns the only transient first-entry copy and the lifecycle state that controls whether staging, comparison or retrieval
 *          is legal. The structure is private so callers cannot bypass validation or observe partially managed credential data.
 */
typedef struct
{
    uint8_t     Credential[CRS_CREDENTIAL_LENGTH];  /*< Private copy of the proposed six-digit credential. */
    CRS_State_t State;                              /*< Current lifecycle of Credential.                   */

}CRS_Handle_t;

/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/**
 * @brief Singleton runtime instance owned exclusively by the Credential Register Service.
 *
 * @details Static initialization establishes an erased credential buffer and EMPTY lifecycle before any application call. The
 *          instance remains private and shall be accessed only through the serialized public API.
 */
static CRS_Handle_t CRS_RuntimeInstance =
{
    .Credential = {0},
    .State      = CRS_STATE_EMPTY
};

/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Checks whether one credential element is a normalized decimal digit.
 *
 * @param   Digit - Numeric credential element to validate.
 *
 * @return  true  - Digit is in the inclusive range from 0U through 9U;
 * @return  false - Digit is outside the normalized decimal range.
 */
static inline bool CRS_IsDigitValid(uint8_t Digit)
{
    return (Digit <= 9);
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Validates and stages the first entry of a proposed credential.
 *
 * @details Rejects a NULL pointer, rejects calls outside the EMPTY lifecycle and validates the complete caller buffer before
 *          mutating private storage. Once every digit is known to be valid, the function copies all six elements and publishes the
 *          STAGED lifecycle.
 *
 * @param   Candidate - Read-only six-element array containing normalized decimal digits.
 *
 * @return  CRS_OPERATION_OK               - Candidate was copied and the lifecycle is STAGED;
 * @return  CRS_OPERATION_INVALID_ARGUMENT - Candidate is NULL or contains an invalid digit;
 * @return  CRS_OPERATION_INVALID_STATE    - The lifecycle was already STAGED or VALIDATED.
 *
 * @note    Input validation and copying are separate passes so invalid input cannot partially replace the private credential.
 */
CRS_OpStatus_t CRS_StageCredential(const uint8_t Candidate[CRS_CREDENTIAL_LENGTH])
{
    if(Candidate == NULL)
    {
        return CRS_OPERATION_INVALID_ARGUMENT;
    }

    if(CRS_RuntimeInstance.State != CRS_STATE_EMPTY)
    {
        return CRS_OPERATION_INVALID_STATE;
    }

    for(size_t digit = 0U; digit < CRS_CREDENTIAL_SIZE; digit++)
    {
        if(!CRS_IsDigitValid(Candidate[digit]))
        {
            return CRS_OPERATION_INVALID_ARGUMENT;
        }
    }

    for(size_t digit = 0U; digit < CRS_CREDENTIAL_SIZE; digit++)
    {
        CRS_RuntimeInstance.Credential[digit] = Candidate[digit];
    }

    CRS_RuntimeInstance.State = CRS_STATE_STAGED;

    return CRS_OPERATION_OK;
}

/**
 * @brief   Validates a confirmation entry against the staged first credential entry.
 *
 * @details Rejects a NULL pointer and requires the STAGED lifecycle. The complete confirmation is validated before comparison so
 *          malformed input cannot be reported as an ordinary mismatch. A difference returns MISMATCH without changing the staged
 *          copy or lifecycle; complete equality publishes the VALIDATED lifecycle.
 *
 * @param   Confirmation - Read-only six-element confirmation credential.
 *
 * @return  CRS_VALIDATION_MATCH            - Every digit matches and the lifecycle is VALIDATED;
 * @return  CRS_VALIDATION_MISMATCH         - At least one digit differs and the lifecycle remains STAGED;
 * @return  CRS_VALIDATION_NOT_STAGED       - The current lifecycle does not permit comparison;
 * @return  CRS_VALIDATION_INVALID_ARGUMENT - Confirmation is NULL or contains an invalid digit.
 *
 * @note    CRS deliberately retains the first entry after mismatch. LCS owns the independent mismatch count and retry limit.
 */
CRS_ValidationResult_t CRS_ValidateConfirmation(const uint8_t Confirmation[CRS_CREDENTIAL_LENGTH])
{
    if(Confirmation == NULL)
    {
        return CRS_VALIDATION_INVALID_ARGUMENT;
    }

    if(CRS_RuntimeInstance.State != CRS_STATE_STAGED)
    {
        return CRS_VALIDATION_NOT_STAGED;
    }

    for(size_t digit = 0U; digit < CRS_CREDENTIAL_SIZE; digit++)
    {
        if(!CRS_IsDigitValid(Confirmation[digit]))
        {
            return CRS_VALIDATION_INVALID_ARGUMENT;
        }
    }

    for(size_t digit = 0U; digit < CRS_CREDENTIAL_SIZE; digit++)
    {
        if(Confirmation[digit] != CRS_RuntimeInstance.Credential[digit])
        {
            return CRS_VALIDATION_MISMATCH;
        }
    }

    CRS_RuntimeInstance.State = CRS_STATE_VALIDATED;

    return CRS_VALIDATION_MATCH;
}

/**
 * @brief   Copies the validated staged credential into caller-owned storage.
 *
 * @details Rejects a NULL destination and requires the VALIDATED lifecycle before copying all six private digits. The copy does
 *          not consume or clear private staging, allowing App Core to complete persistent storage and select the appropriate
 *          terminal cleanup path explicitly.
 *
 * @param   Credential - Writable six-element destination owned by the caller.
 *
 * @return  CRS_OPERATION_OK               - The validated credential was copied completely;
 * @return  CRS_OPERATION_INVALID_ARGUMENT - Credential is NULL;
 * @return  CRS_OPERATION_INVALID_STATE    - The lifecycle is not VALIDATED.
 *
 * @warning The returned copy is sensitive caller-owned data and shall be erased after persistence and runtime update complete.
 */
CRS_OpStatus_t CRS_GetValidatedCredential(uint8_t Credential[CRS_CREDENTIAL_LENGTH])
{
    if(Credential == NULL)
    {
        return CRS_OPERATION_INVALID_ARGUMENT;
    }

    if(CRS_RuntimeInstance.State != CRS_STATE_VALIDATED)
    {
        return CRS_OPERATION_INVALID_STATE;
    }

    for(size_t digit = 0; digit < CRS_CREDENTIAL_SIZE; digit++)
    {
        Credential[digit] = CRS_RuntimeInstance.Credential[digit];
    }

    return CRS_OPERATION_OK;
}

/**
 * @brief   Erases the private staging buffer and restores the EMPTY lifecycle.
 *
 * @details Writes 0U to every private credential element and then publishes EMPTY. The operation has no lifecycle precondition,
 *          which makes cleanup idempotent and safe on successful, cancelled, timed-out, aborted and faulted registration paths.
 *
 * @return  CRS_OPERATION_OK - Private staging is erased and the lifecycle is EMPTY.
 *
 * @note    This operation clears only CRS_RuntimeInstance. It does not erase any caller-owned copy previously returned by
 *          CRS_GetValidatedCredential().
 */
CRS_OpStatus_t CRS_ClearStaging(void)
{
    for(size_t digit = 0U; digit < CRS_CREDENTIAL_SIZE; digit++)
    {
        CRS_RuntimeInstance.Credential[digit] = 0U;
    }

    CRS_RuntimeInstance.State = CRS_STATE_EMPTY;

    return CRS_OPERATION_OK;
}
