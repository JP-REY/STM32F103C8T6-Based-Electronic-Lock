/**********************************************************************************************************************************
 * @file    Authentication_Service.h
 * @brief   Public interface of the Authentication Service.
 *
 * @details The Authentication Service is a synchronous, stateless and hardware-independent domain module responsible for
 *          comparing one caller-provided candidate credential with one caller-provided installed runtime credential.
 *
 *          Both buffers remain entirely owned by the caller. AS_Authenticate() borrows them only for the duration of one
 *          synchronous comparison, modifies neither buffer, retains neither address and creates no internal credential copy.
 *
 *          App Core normally obtains the candidate from Credential Entry and the installed reference from caller-owned runtime
 *          storage populated through Credential Storage. AS includes, receives and exposes none of those module-specific types;
 *          it accepts only two fixed-length uint8_t arrays.
 *
 *          The service does not collect digits, load or persist credentials, own runtime storage, manage entry or registration
 *          sessions, count rejected attempts, apply lockout policy, select Lock Control transitions, control the lock actuator or
 *          produce display, sound or status-indication effects.
 *
 *          The acronym AS means Authentication Service and is used as the public symbol prefix throughout this module.
 *
 * @note    The module creates no task, uses no RTOS primitive, accesses no STM32 peripheral and performs no dynamic allocation.
 *
 * @note    Because the module owns no mutable state or credential data, it requires neither public instances, initialization nor
 *          a credential-configuration operation.
 *
 * @warning Candidate and runtime credential data are sensitive. The caller shall erase the temporary candidate after use and
 *          protect the longer-lived runtime credential according to the application security policy.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    Aug 21, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_SERVICES_AUTHENTICATION_INC_AUTHENTICATION_SERVICE_H_
#define LIBS_SERVICES_AUTHENTICATION_INC_AUTHENTICATION_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "stdint.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**
 * @brief   Exact number of decimal digits in an authentication credential.
 *
 * @details Defines the exact number of bytes read from both Candidate and Credential by AS_Authenticate(). Each caller-owned array
 *          shall provide at least this many readable uint8_t elements.
 */
#define AS_CREDENTIAL_LENGTH (6U)

/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   Result of a candidate authentication request.
 *
 * @details Distinguishes complete equality, an ordinary credential mismatch and invalid caller input. The service exposes no
 *          partial-match information.
 */
typedef enum
{
    AS_RESULT_AUTHENTICATED,     /*< Candidate matches the supplied runtime credential.                */
    AS_RESULT_REJECTED,          /*< At least one candidate byte differs from the runtime credential.  */
    AS_RESULT_INVALID_ARGUMENT   /*< Candidate or runtime Credential points to NULL.                   */

}AS_Result_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
AS_Result_t AS_Authenticate(const uint8_t Candidate[AS_CREDENTIAL_LENGTH],
                            const uint8_t Credential[AS_CREDENTIAL_LENGTH]);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_SERVICES_AUTHENTICATION_INC_AUTHENTICATION_SERVICE_H_ */
