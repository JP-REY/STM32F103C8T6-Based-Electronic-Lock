/**********************************************************************************************************************************
 * @file    FLASH_Platform_Interface.c
 * @brief   Implementation of the STM32 internal Flash platform interface.
 *
 * @details Implements the hardware-dependent Flash operations required by
 *          upper software layers using the STM32 HAL Flash driver.
 *
 *          The module provides Flash locking, unlocking, page erase,
 *          programming, reading and readout-protection operations while
 *          keeping persistent record layout and storage policies outside the
 *          platform layer.
 *
 * @author  Joao Pedro Rey
 * @version 2.0.0
 * @date    2026-08-25
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "FLASH_Platform_Interface.h"
#include "stm32f1xx_hal.h"
#include "stdbool.h"
#include "stddef.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/** @brief First byte address of STM32F103C8T6 internal Flash. */
#define PFLASH_START_ADDRESS    (0x08000000U)

/** @brief Last byte address of the official 64 KiB C8 Flash. */
#define PFLASH_END_ADDRESS      (0x0800FFFFU)

/** @brief STM32F103 medium-density physical erase-page size. */
#define PFLASH_PAGE_SIZE_BYTES  (0x00000400U)

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
static bool     PFLASH_IsRangeValid       (uint32_t Address, uint32_t Length);
static bool     PFLASH_IsAddressAligned   (uint32_t Address, uint32_t Alignment);
static bool     PFLASH_GetProgrammingType (FLASH_ProgramType_t ProgramType, uint32_t* HalProgramType, uint32_t* ProgramLength);
static uint32_t PFLASH_GetPageStart       (uint32_t Address);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Checks whether a byte range is contained in internal Flash.
 *
 * @details Rejects a zero length, a start address outside the STM32F103C8T6
 *          internal-Flash interval and any range whose final byte would exceed
 *          PFLASH_END_ADDRESS. The subtraction form avoids address-addition
 *          overflow while checking the upper boundary.
 *
 * @param   Address - First byte address of the candidate range.
 * @param   Length  - Number of consecutive bytes in the candidate range.
 *
 * @return  true  - The complete nonempty range belongs to internal Flash;
 *          false - Length is zero or any range byte lies outside internal Flash.
 */
static bool PFLASH_IsRangeValid(uint32_t Address, uint32_t Length)
{
    if(Length == 0U || Address < PFLASH_START_ADDRESS || Address > PFLASH_END_ADDRESS)
    {
        return false;
    }

    return (Length - 1U) <= (PFLASH_END_ADDRESS - Address);
}

/**
 * @brief   Checks whether an address begins on the required byte boundary.
 *
 * @param   Address   - Address to validate.
 * @param   Alignment - Required boundary in bytes: 2, 4 or 8.
 *
 * @return  true  - Address is an exact multiple of Alignment;
 *          false - Alignment is zero or Address is not properly aligned.
 */
static bool PFLASH_IsAddressAligned(uint32_t Address, uint32_t Alignment)
{
    return Alignment != 0U && (Address % Alignment) == 0U;
}

/**
 * @brief   Resolves Platform programming metadata into STM32 HAL metadata.
 *
 * @details Maps one FLASH_ProgramType_t value to its STM32 HAL programming
 *          constant and byte width. Both output parameters are populated only
 *          for a recognized programming type.
 *
 * @param   ProgramType    - Project-owned programming-width selector.
 * @param   HalProgramType - Destination for the matching FLASH_TYPEPROGRAM_x
 *                           constant.
 * @param   ProgramLength  - Destination for the matching byte width.
 *
 * @return  true  - Both destinations are valid and ProgramType was mapped;
 *          false - An output pointer is NULL or ProgramType is unsupported.
 */
static bool PFLASH_GetProgrammingType(FLASH_ProgramType_t ProgramType, uint32_t* HalProgramType, uint32_t* ProgramLength)
{
    if(HalProgramType == NULL || ProgramLength == NULL)
    {
        return false;
    }

    switch(ProgramType)
    {
        case FLASH_PROGRAM_HALFWORD:
            *HalProgramType = FLASH_TYPEPROGRAM_HALFWORD;
            *ProgramLength  = 2U;
            break;

        case FLASH_PROGRAM_WORD:
            *HalProgramType = FLASH_TYPEPROGRAM_WORD;
            *ProgramLength  = 4U;
            break;

        case FLASH_PROGRAM_DOUBLEWORD:
            *HalProgramType = FLASH_TYPEPROGRAM_DOUBLEWORD;
            *ProgramLength  = 8U;
            break;

        default:
            return false;
    }

    return true;
}

/**
 * @brief   Resolves the STM32F103 Flash page containing an address.
 *
 * @details Rounds Address down to the start of its containing 1 KiB page. The
 *          subtraction is relative to the internal-Flash base so the page
 *          calculation remains explicit even if the memory base changes.
 *
 * @param   Address - Internal-Flash address whose page start is required.
 *
 * @pre     PFLASH_IsRangeValid(Address, 1U) shall have returned true.
 *
 * @return  First byte address of the containing erase page.
 */
static uint32_t PFLASH_GetPageStart(uint32_t Address)
{
    return Address - ((Address - PFLASH_START_ADDRESS) % PFLASH_PAGE_SIZE_BYTES);
}
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
    (void)HAL_FLASH_OB_Launch();

    (void)HAL_FLASH_OB_Lock();
    (void)HAL_FLASH_Lock();

    /*
     * Defensive return. This point should not normally be reached after
     * a successful Option Byte launch.
     */
    return FLASH_OPERATION_FAIL;
}

/**
 * @brief    Erases the internal Flash page containing the supplied address.
 *
 * @details Resolves the containing STM32F103C8T6 1 KiB page and requests one
 *          page erase through the STM32F1 HAL.
 *
 * @param    Address - Any address contained in the page to erase.
 *
 * @note     The Flash interface shall be unlocked before this call.
 *
 * @warning  Erasing a page destroys every byte in that page. Persistent
 *           storage regions shall be reserved by the linker and exclusively
 *           owned by their upper-layer storage policy.
 *
 * @return   FLASH_OPERATION_OK   - Page erase completed successfully.
 * @return   FLASH_OPERATION_FAIL - Address is invalid or the HAL erase failed.
 */
FLASH_OpStatus_t PFLASH_ErasePageAt(uint32_t Address)
{
    if(!PFLASH_IsRangeValid(Address, 1U))
    {
        return FLASH_OPERATION_FAIL;
    }

    FLASH_EraseInitTypeDef erase_config =
    {
        .TypeErase   = FLASH_TYPEERASE_PAGES,
        .Banks       = FLASH_BANK_1,
        .PageAddress = PFLASH_GetPageStart(Address),
        .NbPages     = 1U
    };

    uint32_t page_error = UINT32_MAX;

    if(HAL_FLASHEx_Erase(&erase_config, &page_error) != HAL_OK ||
       page_error != UINT32_MAX)
    {
        return FLASH_OPERATION_FAIL;
    }

    return FLASH_OPERATION_OK;
}

/**
 * @brief    Programs data into the internal Flash memory.
 *
 * @details  Uses the STM32 HAL Flash programming operation to write data
 *           at the specified internal Flash address.
 *
 *           The amount of data written depends on the programming type
 *           supplied through ProgramType.
 *
 * @param    ProgramType - Platform programming width.
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
FLASH_OpStatus_t PFLASH_Program(FLASH_ProgramType_t ProgramType, uint32_t Address, uint64_t Data)
{
    uint32_t hal_program_type = 0U;
    uint32_t program_length   = 0U;

    if(!PFLASH_GetProgrammingType(ProgramType, &hal_program_type, &program_length) ||
       !PFLASH_IsRangeValid(Address, program_length) ||
       !PFLASH_IsAddressAligned(Address, program_length))
    {
        return FLASH_OPERATION_FAIL;
    }

    if(HAL_FLASH_Program(hal_program_type, Address, Data) != HAL_OK)
    {
        return FLASH_OPERATION_FAIL;
    }

    return FLASH_OPERATION_OK;
}

/**
 * @brief    Reads one 64-bit double word from internal Flash.
 *
 * @param    Address - Eight-byte-aligned source Flash address.
 * @param    Data    - Destination that receives the read value.
 *
 * @return   FLASH_OPERATION_OK   - Read completed successfully.
 * @return   FLASH_OPERATION_FAIL - Argument, range or alignment is invalid.
 */
FLASH_OpStatus_t PFLASH_ReadDoubleWord(uint32_t Address, uint64_t* Data)
{
    if(Data == NULL ||
       !PFLASH_IsRangeValid(Address, sizeof(*Data)) ||
       !PFLASH_IsAddressAligned(Address, sizeof(*Data)))
    {
        return FLASH_OPERATION_FAIL;
    }

    const volatile uint32_t* words = (const volatile uint32_t*)(uintptr_t)Address;

    *Data = (uint64_t)words[0] | ((uint64_t)words[1] << 32U);

    return FLASH_OPERATION_OK;
}

_Static_assert(PFLASH_PAGE_SIZE_BYTES == FLASH_PAGE_SIZE,
               "Platform page size shall match the STM32F1 HAL geometry");
_Static_assert((PFLASH_END_ADDRESS - PFLASH_START_ADDRESS + 1U) ==
               (64U * PFLASH_PAGE_SIZE_BYTES),
               "STM32F103C8T6 Flash range shall contain exactly 64 pages");
