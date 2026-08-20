/**********************************************************************************************************************************
 * @file    FLASH_Platform_Interface.c
 * @brief   Implementation of the STM32 internal Flash platform interface.
 *
 * @details Implements the hardware-dependent Flash operations required by
 *          upper software layers using the STM32 HAL Flash driver.
 *
 *          The module provides Flash locking, unlocking, programming and
 *          readout protection configuration while keeping Flash memory
 *          organization and persistent storage policies outside the
 *          platform layer.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-20
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "FLASH_Platform_Interface.h"
#include "stm32f4xx_hal.h"

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
 * @brief    Locks the internal Flash programming interface.
 *
 * @details  Invokes the STM32 HAL Flash lock operation to prevent additional
 *           Flash programming or erase operations.
 *
 * @note     The interface should normally be locked whenever no Flash write
 *           operation is required.
 *
 * @return   FLASH_OPERATION_OK   - Flash interface successfully locked.
 * @return   FLASH_OPERATION_FAIL - Flash interface locking failed.
 */
FLASH_OpStatus_t PFLASH_Lock(void)
{
    if(HAL_FLASH_Lock() != HAL_OK)
    {
        return FLASH_OPERATION_FAIL;
    }

    return FLASH_OPERATION_OK;
}

/**
 * @brief    Unlocks the internal Flash programming interface.
 *
 * @details  Invokes the STM32 HAL Flash unlock operation, allowing subsequent
 *           erase or programming operations.
 *
 * @note     The caller is responsible for locking the Flash interface again
 *           after completing the required operation.
 *
 * @return   FLASH_OPERATION_OK   - Flash interface successfully unlocked.
 * @return   FLASH_OPERATION_FAIL - Flash interface unlocking failed.
 */
FLASH_OpStatus_t PFLASH_Unlock(void)
{
    if(HAL_FLASH_Unlock() != HAL_OK)
    {
        return FLASH_OPERATION_FAIL;
    }

    return FLASH_OPERATION_OK;
}

/**
 * @brief    Configures the Flash readout protection level.
 *
 * @details  Programs the STM32 RDP Option Byte according to the requested
 *           protection level. After successful programming, the Option Byte
 *           loader is launched so that the new configuration becomes active.
 *
 * @param    ProtectionLevel - Readout protection level to be configured.
 *
 * @note     Changing from RDP Level 1 to RDP Level 0 causes a complete
 *           mass erase of the internal Flash memory.
 *
 * @note     A successful Option Byte launch causes a system reset.
 *
 * @return   FLASH_OPERATION_FAIL - Invalid protection level or HAL operation failed.
 *
 * @warning  Execution is not expected to return from HAL_FLASH_OB_Launch()
 *           when the Option Byte reload succeeds.
 */
FLASH_OpStatus_t PFLASH_SetProtection(FLASH_ProtectionLevel_t ProtectionLevel)
{
    FLASH_OBProgramInitTypeDef OBConfig = {0};

    switch(ProtectionLevel)
    {
        case FLASH_PROTECTION_LEVEL_0:
            OBConfig.RDPLevel = OB_RDP_LEVEL_0;
            break;

        case FLASH_PROTECTION_LEVEL_1:
            OBConfig.RDPLevel = OB_RDP_LEVEL_1;
            break;

        default:
            return FLASH_OPERATION_FAIL;
    }

    OBConfig.OptionType = OPTIONBYTE_RDP;

    if(HAL_FLASH_Unlock() != HAL_OK)
    {
        return FLASH_OPERATION_FAIL;
    }

    if(HAL_FLASH_OB_Unlock() != HAL_OK)
    {
        (void)HAL_FLASH_Lock();

        return FLASH_OPERATION_FAIL;
    }

    if(HAL_FLASHEx_OBProgram(&OBConfig) != HAL_OK)
    {
        (void)HAL_FLASH_OB_Lock();
        (void)HAL_FLASH_Lock();

        return FLASH_OPERATION_FAIL;
    }

    /*
     * A successful Option Byte reload causes a system reset.
     * Therefore normal execution should not continue beyond this call.
     */
    if(HAL_FLASH_OB_Launch() != HAL_OK)
    {
        (void)HAL_FLASH_OB_Lock();
        (void)HAL_FLASH_Lock();

        return FLASH_OPERATION_FAIL;
    }

    /*
     * Defensive return. This point should not normally be reached after
     * a successful Option Byte launch.
     */
    return FLASH_OPERATION_FAIL;
}

/**
 * @brief    Programs data into the internal Flash memory.
 *
 * @details  Uses the STM32 HAL Flash programming operation to write data
 *           at the specified internal Flash address.
 *
 *           The amount of data written depends on the programming type
 *           supplied through TypeProgram.
 *
 * @param    TypeProgram - STM32 HAL Flash programming type.
 * @param    Address     - Destination Flash memory address.
 * @param    Data        - Data value to be programmed.
 *
 * @note     The Flash interface shall be unlocked before calling this
 *           function.
 *
 * @note     Flash erase is not performed automatically. The destination
 *           location shall already be prepared for programming.
 *
 * @return   FLASH_OPERATION_OK   - Flash programming completed successfully.
 * @return   FLASH_OPERATION_FAIL - Flash programming operation failed.
 */
FLASH_OpStatus_t PFLASH_Program(uint32_t TypeProgram, uint32_t Address, uint64_t Data)
{
    if(HAL_FLASH_Program(TypeProgram, Address, Data) != HAL_OK)
    {
        return FLASH_OPERATION_FAIL;
    }

    return FLASH_OPERATION_OK;
}