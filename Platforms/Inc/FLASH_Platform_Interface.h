/**********************************************************************************************************************************
 * @file    FLASH_Platform_Interface.h
 * @brief   Public Platform interface for STM32 internal Flash operations.
 *
 * @details Defines the project-owned boundary used by upper software layers to
 *          lock and unlock the internal Flash programming interface, erase one
 *          address-selected sector, program one value with an explicit width,
 *          read one 64-bit value and configure supported readout-protection
 *          levels.
 *
 *          STM32 HAL programming constants, sector identifiers, option-byte
 *          structures and direct memory access remain private to the Platform
 *          implementation. Flash allocation, persistent record formats,
 *          credential validation, wear policy and recovery decisions remain
 *          responsibilities of higher-level storage modules.
 *
 *          Every operation is synchronous and uses no dynamic allocation,
 *          task, queue, mutex or interrupt callback. The interface has no
 *          initialization function and retains no project-owned runtime state.
 *
 *          The acronym PFLASH means Platform Flash and prefixes every public
 *          function exposed by this module.
 *
 * @note    The current backend targets the 512 KiB internal Flash organization
 *          of the STM32F411CEU6, from 0x08000000 through 0x0807FFFF.
 *
 * @note    Callers own serialization. Concurrent erase, program, protection or
 *          lock-state operations are outside this interface contract.
 *
 * @warning Flash erase and programming are destructive operations. Callers
 *          shall operate only on linker-reserved regions that they exclusively
 *          own and shall restore the programming interface to the locked state.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-20
 **********************************************************************************************************************************/

#ifndef INC_FLASH_PLATFORM_INTERFACE_H
#define INC_FLASH_PLATFORM_INTERFACE_H

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
/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   Result of a Platform Flash operation.
 *
 * @details Reports only whether the requested Platform operation completed.
 *          STM32 HAL error codes and Flash status-register details do not cross
 *          the Platform boundary.
 */
typedef enum
{
    FLASH_OPERATION_OK,   /*< Operation completed successfully.                         */
    FLASH_OPERATION_FAIL  /*< Arguments were invalid or the STM32 operation did not complete. */

} FLASH_OpStatus_t;

/**
 * @brief   Programming widths supported by the internal Flash interface.
 *
 * @details Selects how many least-significant bits from the 64-bit Data
 *          argument are programmed by PFLASH_Program(). The implementation
 *          maps each project-owned value to the corresponding STM32 HAL
 *          programming constant and enforces natural address alignment.
 *
 * @warning FLASH_PROGRAM_DOUBLEWORD requires the external Vpp conditions
 *          specified by the STM32F4 documentation. Normal 3.3 V-only operation
 *          shall use FLASH_PROGRAM_WORD or a narrower width.
 */
typedef enum
{
    FLASH_PROGRAM_BYTE,       /*< Program 8 bits at a one-byte-aligned address.     */
    FLASH_PROGRAM_HALFWORD,   /*< Program 16 bits at a two-byte-aligned address.    */
    FLASH_PROGRAM_WORD,       /*< Program 32 bits at a four-byte-aligned address.   */
    FLASH_PROGRAM_DOUBLEWORD  /*< Program 64 bits at an eight-byte-aligned address. */

} FLASH_ProgramType_t;


/**
 * @brief   Readout-protection levels exposed by the Platform interface.
 *
 * @details The interface intentionally exposes only STM32 RDP Level 0 and
 *          Level 1. Their vendor encodings remain private to the backend.
 *
 * @note    RDP Level 2 is not exposed because it is irreversible.
 */
typedef enum
{
    FLASH_PROTECTION_LEVEL_0, /*< External debug/programming readout remains available. */
    FLASH_PROTECTION_LEVEL_1  /*< External debug/programming readout is protected.      */

} FLASH_ProtectionLevel_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
FLASH_OpStatus_t PFLASH_Lock            (void);
FLASH_OpStatus_t PFLASH_Unlock          (void);
FLASH_OpStatus_t PFLASH_SetProtection   (FLASH_ProtectionLevel_t ProtectionLevel);
FLASH_OpStatus_t PFLASH_EraseSectorAt   (uint32_t Address);
FLASH_OpStatus_t PFLASH_Program         (FLASH_ProgramType_t ProgramType, uint32_t Address, uint64_t Data);
FLASH_OpStatus_t PFLASH_ReadDoubleWord  (uint32_t Address, uint64_t* Data);

#ifdef __cplusplus
}
#endif

#endif /* INC_FLASH_PLATFORM_INTERFACE_H */
