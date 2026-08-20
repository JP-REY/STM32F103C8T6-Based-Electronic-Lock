/**********************************************************************************************************************************
 * @file    FLASH_Platform_Interface.h
 * @brief   Public interface for STM32 internal Flash memory operations.
 *
 * @details Provides a platform abstraction over the STM32 HAL Flash driver,
 *          exposing basic Flash locking, unlocking, programming and readout
 *          protection configuration operations to upper software layers.
 *
 *          This module contains only hardware-dependent Flash access logic.
 *          Flash memory allocation, persistent data layout and storage policies
 *          shall be handled by higher-level modules.
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
 * @brief    Defines the operation status returned by the Flash Platform Interface.
 *
 * @details  Indicates whether a requested Flash operation was successfully
 *           completed by the underlying STM32 HAL driver.
 */
typedef enum
{
    FLASH_OPERATION_OK,
    FLASH_OPERATION_FAIL

} FLASH_OpStatus_t;


/**
 * @brief    Defines the supported Flash readout protection levels.
 *
 * @details  The platform exposes only STM32 RDP Level 0 and Level 1.
 *
 *           Level 0 leaves the Flash memory externally readable.
 *
 *           Level 1 enables readout protection against external debug and
 *           programming interfaces.
 *
 * @note     RDP Level 2 is intentionally not exposed because it provides
 *           irreversible protection.
 */
typedef enum
{
    FLASH_PROTECTION_LEVEL_0,
    FLASH_PROTECTION_LEVEL_1

} FLASH_ProtectionLevel_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
FLASH_OpStatus_t PFLASH_Lock          (void);
FLASH_OpStatus_t PFLASH_Unlock        (void);
FLASH_OpStatus_t PFLASH_SetProtection (FLASH_ProtectionLevel_t ProtectionLevel);
FLASH_OpStatus_t PFLASH_Program       (uint32_t TypeProgram, uint32_t Address,uint64_t Data);

#ifdef __cplusplus
}
#endif

#endif /* INC_FLASH_PLATFORM_INTERFACE_H */