/**********************************************************************************************************************************
 * @file    Authentication_Service.c
 * @brief   Authentication Service implementation.
 *
 * @details Implements synchronous fixed-length comparison between one borrowed candidate and one borrowed installed runtime
 *          credential. The operation validates both pointers, compares the six uint8_t elements in order and returns on the first
 *          difference without modifying or retaining either caller-owned buffer.
 *
 *          The implementation owns no configured credential, mutable runtime data or service lifecycle. It performs no hardware
 *          access, persistent-storage operation, inter-service call, dynamic allocation or RTOS operation.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    Aug 21, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Authentication_Service.h"
#include "stddef.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
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
 * @brief   Authenticates one complete candidate against a caller-supplied runtime credential.
 *
 * @details Validates both pointers and then compares exactly AS_CREDENTIAL_LENGTH uint8_t elements at matching indices. The first
 *          difference returns AS_RESULT_REJECTED; reaching the fixed bound without a difference returns
 *          AS_RESULT_AUTHENTICATED.
 *
 *          Candidate and Credential are borrowed only for this synchronous call. The service modifies neither array, retains
 *          neither pointer and creates no internal credential copy.
 *
 * @param   Candidate  - Read-only array containing exactly AS_CREDENTIAL_LENGTH candidate elements in their original order.
 * @param   Credential - Read-only array containing exactly AS_CREDENTIAL_LENGTH installed runtime credential elements.
 *
 * @pre     Each non-NULL argument shall point to at least AS_CREDENTIAL_LENGTH readable uint8_t elements and remain unchanged
 *          until this function returns.
 *
 * @note    Array notation documents the required capacity but C adjusts both parameters to pointers. The caller remains
 *          responsible for supplying complete buffers and normalized digit values.
 *
 * @warning The caller shall erase the temporary Candidate after processing the result. Credential normally remains available as
 *          the installed runtime reference for subsequent authentication requests.
 *
 * @return  AS_RESULT_AUTHENTICATED    - Every candidate element matches the supplied runtime credential;
 * @return  AS_RESULT_REJECTED         - At least one element differs;
 * @return  AS_RESULT_INVALID_ARGUMENT - Candidate or Credential is NULL.
 */
AS_Result_t AS_Authenticate(const uint8_t Candidate[AS_CREDENTIAL_LENGTH],
                            const uint8_t Credential[AS_CREDENTIAL_LENGTH])
{
    if(Candidate == NULL || Credential == NULL)
    {
        return AS_RESULT_INVALID_ARGUMENT;
    }

    for(uint8_t digit = 0; digit < AS_CREDENTIAL_LENGTH; digit++)
    {
        if(Candidate[digit] != Credential[digit])
        {
            return AS_RESULT_REJECTED;
        }
    }

    return AS_RESULT_AUTHENTICATED;
}
