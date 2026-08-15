/**********************************************************************************************************************************
 * @file    Authentication_Service.c
 * @brief   Authentication Service implementation.
 *
 * @details Implements synchronous fixed-length credential validation without
 *          retaining candidate data or depending on the Credential Entry
 *          Service, application state or hardware resources.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 15, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Authentication_Service.h"
#include "stddef.h"
#include "string.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**
 * @brief Internal representation of one configured credential digit.
 */
typedef uint8_t AS_CandidateDigit_t;

/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**
 * @brief   Credential used as the authentication reference.
 *
 * @details The credential is immutable and stored directly in the firmware.
 *          Each element represents a normalized decimal value rather than an
 *          ASCII character.
 *
 * @warning This V1 representation is not secure against firmware inspection
 *          or extraction.
 */
static const AS_CandidateDigit_t AS_ConfiguredPin[AS_CREDENTIAL_LENGTH] = {1U, 3U, 0U, 6U, 0U, 3U};

/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
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
 * @brief   Authenticates one complete candidate credential.
 *
 * @details Compares exactly AS_CREDENTIAL_LENGTH bytes from Candidate with
 *          the credential configured privately by the service.
 *
 *          Candidate is borrowed only for this synchronous call. The service
 *          does not modify the supplied digits, retain the pointer or create
 *          an internal candidate copy.
 *
 * @param   Candidate - Pointer to an array containing exactly
 *                      AS_CREDENTIAL_LENGTH normalized decimal digits in
 *                      their original entry order.
 *
 * @pre     Candidate shall either be NULL or point to storage containing at
 *          least AS_CREDENTIAL_LENGTH readable bytes.
 *
 * @note    The array notation documents the required size but is adjusted to
 *          a pointer by the C language. The caller remains responsible for
 *          supplying a complete buffer.
 *
 * @warning The caller shall erase sensitive candidate storage after this
 *          function returns. The function does not erase caller-owned data.
 *
 * @return  AS_RESULT_AUTHENTICATED     - When every candidate digit matches;
 *          AS_RESULT_REJECTED          - When at least one digit differs;
 *          AS_RESULT_INVALID_ARGUMENT  - When Candidate is NULL.
 */
AS_Result_t AS_Authenticate(const uint8_t Candidate[AS_CREDENTIAL_LENGTH])
{
    if(Candidate == NULL)
    {
        return AS_RESULT_INVALID_ARGUMENT;
    }

    return (memcmp(Candidate, AS_ConfiguredPin, sizeof(AS_ConfiguredPin)) == 0) ?

                AS_RESULT_AUTHENTICATED : AS_RESULT_REJECTED;
}
