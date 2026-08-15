/**********************************************************************************************************************************
 * @file    Authentication_Service.h
 * @brief   Public interface of the Authentication Service.
 *
 * @details The Authentication Service is a synchronous, stateless and
 *          hardware-independent domain module responsible for validating one
 *          caller-provided candidate credential against the credential
 *          configured in the firmware.
 *
 *          Candidate storage remains entirely owned by the caller. The
 *          service reads the candidate only for the duration of
 *          AS_Authenticate() and does not retain its address or create an
 *          internal copy.
 *
 *          The service is independent from the Credential Entry Service and
 *          therefore does not include, receive or expose CES types. The Lock
 *          Controller is responsible for obtaining a complete candidate from
 *          CES, passing its digit buffer to this service and erasing the
 *          caller-owned copy after authentication.
 *
 *          The service does not collect credential digits, manage entry
 *          sessions, count rejected attempts, apply lockout policy, control
 *          the lock actuator or produce display, sound or status-indication
 *          side effects. These responsibilities belong to the Credential
 *          Entry Service and the Lock Controller.
 *
 *          The acronym AS means Authentication Service and is used as the
 *          public symbol prefix throughout this module.
 *
 * @note    This module does not create tasks, use RTOS primitives, access
 *          STM32 peripherals or perform dynamic memory allocation.
 *
 * @note    Because the module has no mutable service state, it requires
 *          neither public instances nor initialization.
 *
 * @warning The configured credential is compiled into the firmware. This
 *          implementation does not provide secure credential storage or
 *          protection against firmware extraction.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 15, 2026
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
 * @details Defines both the required candidate length and the size of the
 *          configured credential stored privately by the service.
 */
#define AS_CREDENTIAL_LENGTH (6U)

/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   Result of a candidate authentication request.
 *
 * @details Distinguishes a matching candidate from a rejected candidate and
 *          from an invalid API argument.
 */
typedef enum
{
    AS_RESULT_AUTHENTICATED,     /*< Candidate matches the configured credential.        */
    AS_RESULT_REJECTED,          /*< Candidate does not match the configured credential. */
    AS_RESULT_INVALID_ARGUMENT   /*< Candidate points to NULL.                           */

}AS_Result_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
AS_Result_t AS_Authenticate(const uint8_t Candidate[AS_CREDENTIAL_LENGTH]);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_SERVICES_AUTHENTICATION_INC_AUTHENTICATION_SERVICE_H_ */
