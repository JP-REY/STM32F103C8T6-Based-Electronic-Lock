/**********************************************************************************************************************************
 * @file    Credential_Register_Service.h
 * @brief   Public interface of the Credential Register Service.
 *
 * @details The Credential Register Service is a synchronous, hardware-independent domain module responsible only for temporarily
 *          staging the first entry of a proposed credential, comparing a confirmation entry with that staged value and exposing
 *          the credential after a successful comparison.
 *
 *          A staged credential contains exactly six normalized decimal digits. Each digit is stored as its numeric value from 0U
 *          through 9U rather than as an ASCII character. The service owns one private copy while registration is active; caller
 *          buffers remain owned by the caller and are read or written only for the duration of the corresponding API call.
 *
 *          The private data lifecycle is EMPTY, STAGED and VALIDATED. CRS_StageCredential() accepts the first entry only while the
 *          service is empty. CRS_ValidateConfirmation() retains the staged value after a mismatch so the application may collect
 *          another confirmation. A match makes the staged value retrievable through CRS_GetValidatedCredential().
 *          CRS_ClearStaging() erases the private copy and restores the empty state from any lifecycle phase.
 *
 *          The service does not collect digits, persist credentials, authenticate normal access candidates, count confirmation
 *          mismatches, apply retry limits, select Lock Control transitions, own timeouts or produce user-interface effects. App
 *          Core coordinates Credential Entry, this staging service, Credential Storage and Lock Control. LCS owns registration
 *          phase and retry policy; CSS alone owns persistent Flash storage.
 *
 *          The acronym CRS means Credential Register Service and prefixes every public symbol exposed by this module.
 *
 * @note    The module owns one private singleton and exposes no public handle or initialization operation. Public calls shall be
 *          serialized by the application.
 *
 * @note    Operations are synchronous, bounded, use static storage only and create no task, queue, mutex, timer or other RTOS
 *          object.
 *
 * @warning Staged and copied credentials are sensitive data. The application shall call CRS_ClearStaging() on every successful,
 *          cancelled, timed-out, aborted or faulted registration path and shall erase every caller-owned credential copy after
 *          its final synchronous consumer has finished.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-21
 **********************************************************************************************************************************/

#ifndef CREDENTIAL_REGISTER_SERVICE_H
#define CREDENTIAL_REGISTER_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "stdint.h"

/**********************************************************************************************************************************
 Defines
 **********************************************************************************************************************************/
/**
 * @brief Exact number of normalized decimal digits in one credential.
 *
 * @details Defines the number of bytes read by CRS_StageCredential() and CRS_ValidateConfirmation(), and the number of bytes
 *          written by CRS_GetValidatedCredential(). Every element shall contain a numeric value from 0U through 9U.
 */
#define CRS_CREDENTIAL_LENGTH (6U)

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief Execution status of a Credential Register staging operation.
 *
 * @details Reports whether staging, validated-credential retrieval or staging cleanup completed according to the API argument and
 *          lifecycle preconditions. Confirmation comparison has its own CRS_ValidationResult_t because match and mismatch are
 *          normal domain outcomes rather than generic operation failures.
 */
typedef enum
{
    CRS_OPERATION_OK,                /*< The requested staging operation completed successfully.                      */
    CRS_OPERATION_INVALID_ARGUMENT,  /*< A pointer is NULL or a supplied credential digit is outside 0U through 9U.   */
    CRS_OPERATION_INVALID_STATE      /*< The current private lifecycle state does not permit the requested operation. */

}CRS_OpStatus_t;

/**
 * @brief Result of comparing a confirmation credential with the staged first entry.
 *
 * @details Distinguishes a successful match, an ordinary mismatch, absence of a staged first entry and invalid caller input. A
 *          mismatch preserves the staged credential so LCS may authorize another confirmation attempt without recollecting the
 *          first entry.
 */
typedef enum
{
    CRS_VALIDATION_MATCH,             /*< Confirmation matches the staged entry; the credential becomes validated.  */
    CRS_VALIDATION_MISMATCH,          /*< Confirmation differs; the staged entry remains available for another try. */
    CRS_VALIDATION_NOT_STAGED,        /*< No first entry is currently available for confirmation comparison.        */
    CRS_VALIDATION_INVALID_ARGUMENT   /*< Confirmation is NULL or contains a value outside 0U through 9U.           */

}CRS_ValidationResult_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
CRS_OpStatus_t         CRS_StageCredential        (const uint8_t Candidate[CRS_CREDENTIAL_LENGTH]);
CRS_ValidationResult_t CRS_ValidateConfirmation   (const uint8_t Confirmation[CRS_CREDENTIAL_LENGTH]);
CRS_OpStatus_t         CRS_GetValidatedCredential (uint8_t Credential[CRS_CREDENTIAL_LENGTH]);
CRS_OpStatus_t         CRS_ClearStaging           (void);

#ifdef __cplusplus
}
#endif

#endif /* CREDENTIAL_REGISTER_SERVICE_H */
