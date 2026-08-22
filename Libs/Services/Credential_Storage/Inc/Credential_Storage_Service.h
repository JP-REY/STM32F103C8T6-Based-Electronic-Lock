/**********************************************************************************************************************************
 * @file    Credential_Storage_Service.h
 * @brief   Public interface of the Credential Storage Service.
 *
 * @details Defines a synchronous service for validating, persisting, detecting
 *          and retrieving exactly one six-digit credential. Each digit is a
 *          normalized numeric byte from 0U through 9U rather than an ASCII
 *          character.
 *
 *          The service owns the persistent record layout, integrity marker,
 *          CRC calculation, Flash write sequence and record validation policy.
 *          It uses the Platform Flash Interface for hardware-dependent read,
 *          erase, program, lock and unlock mechanics.
 *
 *          Runtime credential storage remains owned by the caller.
 *          CSS_GetCredential() copies a valid registered credential into a
 *          caller-provided buffer and does not retain that buffer address.
 *
 *          The service does not collect digits, manage a registration session,
 *          authenticate candidates, select Lock Control transitions, produce
 *          user-interface effects or configure Flash readout protection.
 *
 *          The acronym CSS means Credential Storage Service and prefixes every
 *          public symbol exposed by this module.
 *
 * @note    The service is stateless and therefore requires no initialization,
 *          public handle or runtime instance.
 *
 * @note    Operations are synchronous, use no dynamic allocation and create no
 *          task, queue, mutex, timer or other RTOS object.
 *
 * @note    One eight-byte record is stored at the start of linker-reserved
 *          STM32F411 sector 7. The complete sector is exclusively owned by CSS.
 *
 * @warning The stored representation is not encrypted or cryptographically
 *          authenticated. Its marker and CRC detect format errors and likely
 *          corruption; they do not protect credential confidentiality.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-21
 **********************************************************************************************************************************/

#ifndef LIBS_SERVICES_CREDENTIAL_STORAGE_INC_CREDENTIAL_STORAGE_SERVICE_H_
#define LIBS_SERVICES_CREDENTIAL_STORAGE_INC_CREDENTIAL_STORAGE_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "stdbool.h"
#include "stdint.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**
 * @brief   Exact number of normalized decimal digits in one credential.
 *
 * @details Defines the number of bytes read by CSS_SaveCredential() and written
 *          by CSS_GetCredential(). Each element shall contain a value from 0U
 *          through 9U in the original entry order.
 */
#define CSS_CREDENTIAL_LENGTH (6U)

/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   Result of a Credential Storage operation.
 *
 * @details Distinguishes successful completion, invalid caller input, absence
 *          of a valid registered record and failure of the underlying Flash
 *          transaction. CSS_HasCredential() intentionally reduces the latter
 *          record states to a Boolean availability result.
 */
typedef enum
{
    CSS_OPERATION_OK,                /*< Requested operation completed successfully.         */
    CSS_OPERATION_INVALID_ARGUMENT,  /*< A pointer is NULL or a supplied digit exceeds 9U.   */
    CSS_OPERATION_NOT_FOUND,         /*< Flash contains no complete valid credential record. */
    CSS_OPERATION_STORAGE_ERROR      /*< A required Platform Flash operation failed.         */

}CSS_OpStatus_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
CSS_OpStatus_t CSS_SaveCredential(const uint8_t Credential[CSS_CREDENTIAL_LENGTH]);
CSS_OpStatus_t CSS_GetCredential (uint8_t Credential[CSS_CREDENTIAL_LENGTH]);
bool           CSS_HasCredential (void);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_SERVICES_CREDENTIAL_STORAGE_INC_CREDENTIAL_STORAGE_SERVICE_H_ */
