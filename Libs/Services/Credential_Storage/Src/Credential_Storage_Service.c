/**********************************************************************************************************************************
 * @file    Credential_Storage_Service.c
 * @brief   Credential Storage Service implementation.
 *
 * @details Persists one six-digit credential as one eight-byte Flash record:
 *          six normalized digit bytes, one format marker and one CRC-8 byte.
 *          The marker and CRC distinguish erased or corrupted Flash from a
 *          valid registered credential. The record is programmed as two
 *          32-bit words so normal 3.3 V operation does not require external
 *          Vpp for STM32F4 double-word programming.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-21
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Credential_Storage_Service.h"
#include "FLASH_Platform_Interface.h"
#include "stddef.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**
 * @brief   Start of STM32F411 sector 7, reserved by the linker for CSS.
 *
 * @details Both active linker scripts limit executable Flash to the preceding
 *          384 KiB and expose sector 7 as the exclusive credential region.
 */
#define CSS_FLASH_BASE_ADDRESS       (0x08060000U)

/** @brief Eight-byte value read from a completely erased record location. */
#define CSS_FLASH_ERASED_RECORD      (UINT64_MAX)

/** @brief Format marker stored after the six serialized credential bytes. */
#define CSS_RECORD_MARKER            (0xA5U)

/** @brief Polynomial used by the MSB-first CRC-8/ATM integrity calculation. */
#define CSS_CRC8_POLYNOMIAL          (0x07U)

/** @brief Number of bits contained in one serialized record byte. */
#define CSS_BITS_PER_BYTE            (8U)

/** @brief Serialized byte position occupied by the record marker. */
#define CSS_RECORD_MARKER_BYTE_INDEX (CSS_CREDENTIAL_LENGTH)

/** @brief Serialized byte position occupied by the record CRC. */
#define CSS_RECORD_CRC_BYTE_INDEX    (CSS_CREDENTIAL_LENGTH + 1U)

/** @brief Total serialized record length: six digits, marker and CRC. */
#define CSS_RECORD_LENGTH            (CSS_CREDENTIAL_LENGTH + 2U)

/** @brief Byte offset of the second normal-voltage 32-bit program operation. */
#define CSS_SECOND_WORD_OFFSET       (sizeof(uint32_t))

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
static uint8_t  CSS_UpdateCrc         (uint8_t Crc, uint8_t Data);
static bool     CSS_IsCredentialValid (const uint8_t Credential[CSS_CREDENTIAL_LENGTH]);
static uint64_t CSS_EncodeRecord      (const uint8_t Credential[CSS_CREDENTIAL_LENGTH]);
static bool     CSS_DecodeRecord      (uint64_t Record, uint8_t Credential[CSS_CREDENTIAL_LENGTH]);
static void     CSS_ClearCredential   (uint8_t Credential[CSS_CREDENTIAL_LENGTH]);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Updates a CRC-8/ATM accumulator with one serialized byte.
 *
 * @details XORs Data into the current accumulator and processes exactly eight
 *          bits in most-significant-bit-first order with polynomial 0x07. CSS
 *          starts a record calculation with accumulator 0U and invokes this
 *          helper once for each of the six digits followed by the marker.
 *
 * @param   Crc  - CRC accumulator before Data is processed.
 * @param   Data - Next serialized record byte.
 *
 * @return  Updated eight-bit CRC accumulator.
 */
static uint8_t CSS_UpdateCrc(uint8_t Crc, uint8_t Data)
{
    Crc ^= Data;

    for(uint8_t bit = 0U; bit < CSS_BITS_PER_BYTE; bit++)
    {
        Crc = ((Crc & 0x80U) != 0U) ?
              (uint8_t)((Crc << 1U) ^ CSS_CRC8_POLYNOMIAL) :
              (uint8_t) (Crc << 1U)                        ;
    }

    return Crc;
}

/**
 * @brief   Validates one caller-provided credential buffer.
 *
 * @details Accepts only a non-NULL buffer whose six elements are normalized
 *          decimal values in the inclusive range from 0U through 9U.
 *
 * @param   Credential - Candidate buffer containing CSS_CREDENTIAL_LENGTH bytes.
 *
 * @return  true  - Credential is non-NULL and all six digits are valid;
 *          false - Credential is NULL or at least one value exceeds 9U.
 */
static bool CSS_IsCredentialValid(const uint8_t Credential[CSS_CREDENTIAL_LENGTH])
{
    if(Credential == NULL)
    {
        return false;
    }

    for(size_t index = 0U; index < CSS_CREDENTIAL_LENGTH; index++)
    {
        if(Credential[index] > 9U)
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief   Serializes a validated credential into one eight-byte record.
 *
 * @details Places each credential byte at the matching little-endian byte
 *          position, places CSS_RECORD_MARKER in byte six and places the
 *          CRC-8/ATM of bytes zero through six in byte seven. Explicit shifts
 *          define the on-Flash layout independently from structure padding.
 *
 * @param   Credential - Six normalized digits to serialize.
 *
 * @pre     CSS_IsCredentialValid(Credential) shall have returned true.
 *
 * @return  Complete 64-bit persistent record value.
 */
static uint64_t CSS_EncodeRecord(const uint8_t Credential[CSS_CREDENTIAL_LENGTH])
{
    uint64_t record = 0U;
    uint8_t  crc    = 0U;

    for(size_t index = 0U; index < CSS_CREDENTIAL_LENGTH; index++)
    {
        record |= (uint64_t)Credential[index] << (index * CSS_BITS_PER_BYTE);
        crc = CSS_UpdateCrc(crc, Credential[index]);
    }

    record |= (uint64_t)CSS_RECORD_MARKER << (CSS_RECORD_MARKER_BYTE_INDEX * CSS_BITS_PER_BYTE);
    crc = CSS_UpdateCrc(crc, CSS_RECORD_MARKER);
    record |= (uint64_t)crc << (CSS_RECORD_CRC_BYTE_INDEX * CSS_BITS_PER_BYTE);

    return record;
}

/**
 * @brief   Validates and optionally decodes one persistent record.
 *
 * @details Rejects an erased value, an incorrect format marker, any digit
 *          outside the decimal range or a CRC mismatch. After every validation
 *          succeeds, the six digits are copied to Credential when that optional
 *          destination is non-NULL. Passing NULL performs validation only.
 *
 * @param   Record     - Complete eight-byte value read from Flash.
 * @param   Credential - Optional destination for six decoded digits, or NULL.
 *
 * @note    A non-NULL destination is not modified unless the complete record
 *          has already passed every validation step.
 *
 * @return  true  - Record is complete and valid;
 *          false - Record is erased, malformed or corrupted.
 */
static bool CSS_DecodeRecord(uint64_t Record, uint8_t Credential[CSS_CREDENTIAL_LENGTH])
{
    if(Record == CSS_FLASH_ERASED_RECORD)
    {
        return false;
    }

    uint8_t marker = (uint8_t)(Record >> (CSS_RECORD_MARKER_BYTE_INDEX * CSS_BITS_PER_BYTE));

    if(marker != CSS_RECORD_MARKER)
    {
        return false;
    }

    uint8_t crc = 0U;

    for(size_t index = 0U; index < CSS_CREDENTIAL_LENGTH; index++)
    {
        uint8_t digit = (uint8_t)(Record >> (index * CSS_BITS_PER_BYTE));

        if(digit > 9U)
        {
            return false;
        }

        crc = CSS_UpdateCrc(crc, digit);
    }

    crc = CSS_UpdateCrc(crc, marker);

    uint8_t stored_crc = (uint8_t)(Record >> (CSS_RECORD_CRC_BYTE_INDEX * CSS_BITS_PER_BYTE));

    if(crc != stored_crc)
    {
        return false;
    }

    if(Credential != NULL)
    {
        for(size_t index = 0U; index < CSS_CREDENTIAL_LENGTH; index++)
        {
            Credential[index] = (uint8_t)(Record >> (index * CSS_BITS_PER_BYTE));
        }
    }

    return true;
}

/**
 * @brief   Explicitly clears one caller-owned credential buffer.
 *
 * @details Writes zero to all CSS_CREDENTIAL_LENGTH bytes through a volatile
 *          pointer so the compiler cannot remove the stores merely because the
 *          destination is not read again by this module.
 *
 * @param   Credential - Writable six-byte buffer to clear.
 *
 * @pre     Credential shall not be NULL and shall reference at least
 *          CSS_CREDENTIAL_LENGTH writable bytes.
 */
static void CSS_ClearCredential(uint8_t Credential[CSS_CREDENTIAL_LENGTH])
{
    volatile uint8_t* credential_bytes = Credential;

    for(size_t index = 0U; index < CSS_CREDENTIAL_LENGTH; index++)
    {
        credential_bytes[index] = 0U;
    }
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief    Saves or replaces the registered credential.
 *
 * @details  Validates and encodes the six digits, then reads the current record.
 *           An identical valid record returns success without changing Flash.
 *           Otherwise the function unlocks the programming interface, erases
 *           the dedicated sector when the record location is not already
 *           erased, programs two 32-bit words, locks the interface and verifies
 *           the complete record by readback and decoding.
 *
 * @param    Credential - Six normalized decimal digits to persist.
 *
 * @pre      Credential shall either be NULL or reference at least
 *           CSS_CREDENTIAL_LENGTH readable bytes.
 *
 * @note     Saving the credential already present performs no Flash write.
 * 
 * @note     The supplied buffer is borrowed only for this synchronous call.
 * 
 * @note     Sector erase and word programming block until STM32 HAL reports
 *           completion or failure.
 * @note     After a successful unlock, PFLASH_Lock() is attempted whether the
 *           erase/program sequence succeeds or fails.
 *
 * @warning  Replacing an existing credential requires a sector erase. A power
 *           loss between erase and verified programming leaves no registered
 *           credential; the first-boot registration path shall then run again.
 *
 * @return   CSS_OPERATION_OK               - Credential is stored and verified.
 * @return   CSS_OPERATION_INVALID_ARGUMENT - Pointer is NULL or a digit exceeds nine.
 * @return   CSS_OPERATION_STORAGE_ERROR    - A Platform read, erase, program or lock failed.
 */
CSS_OpStatus_t CSS_SaveCredential(const uint8_t Credential[CSS_CREDENTIAL_LENGTH])
{
    if(!CSS_IsCredentialValid(Credential))
    {
        return CSS_OPERATION_INVALID_ARGUMENT;
    }

    uint64_t current_record = CSS_FLASH_ERASED_RECORD;

    if(PFLASH_ReadDoubleWord(CSS_FLASH_BASE_ADDRESS, &current_record) != FLASH_OPERATION_OK)
    {
        return CSS_OPERATION_STORAGE_ERROR;
    }

    uint64_t requested_record = CSS_EncodeRecord(Credential);

    if(current_record == requested_record && CSS_DecodeRecord(current_record, NULL))
    {
        return CSS_OPERATION_OK;
    }

    if(PFLASH_Unlock() != FLASH_OPERATION_OK)
    {
        return CSS_OPERATION_STORAGE_ERROR;
    }

    FLASH_OpStatus_t write_status = FLASH_OPERATION_OK;

    if(current_record != CSS_FLASH_ERASED_RECORD)
    {
        write_status = PFLASH_EraseSectorAt(CSS_FLASH_BASE_ADDRESS);
    }

    if(write_status == FLASH_OPERATION_OK)
    {
        write_status = PFLASH_Program(FLASH_PROGRAM_WORD,
                                      CSS_FLASH_BASE_ADDRESS,
                                     (uint32_t)requested_record);
    }

    if(write_status == FLASH_OPERATION_OK)
    {
        write_status = PFLASH_Program(FLASH_PROGRAM_WORD,
                                      CSS_FLASH_BASE_ADDRESS + CSS_SECOND_WORD_OFFSET,
                                     (uint32_t)(requested_record >> 32U));
    }

    FLASH_OpStatus_t lock_status = PFLASH_Lock();

    if(write_status != FLASH_OPERATION_OK || lock_status != FLASH_OPERATION_OK)
    {
        return CSS_OPERATION_STORAGE_ERROR;
    }

    uint64_t stored_record = CSS_FLASH_ERASED_RECORD;

    if(PFLASH_ReadDoubleWord(CSS_FLASH_BASE_ADDRESS, &stored_record) != FLASH_OPERATION_OK ||
       stored_record != requested_record ||
       !CSS_DecodeRecord(stored_record, NULL))
    {
        return CSS_OPERATION_STORAGE_ERROR;
    }

    return CSS_OPERATION_OK;
}

/**
 * @brief    Copies the registered credential into caller-owned runtime storage.
 *
 * @details  Reads the eight-byte record, validates its marker, digit values and
 *           CRC, and copies the digits only after complete validation succeeds.
 *           A failed read or invalid record clears the complete destination.
 *
 * @param    Credential - Destination for exactly CSS_CREDENTIAL_LENGTH bytes.
 *
 * @note     The destination is cleared when no valid credential can be read.
 * 
 * @note     The destination address is not retained after this synchronous call.
 *
 * @return   CSS_OPERATION_OK               - Credential was copied.
 * @return   CSS_OPERATION_INVALID_ARGUMENT - Destination is NULL.
 * @return   CSS_OPERATION_NOT_FOUND        - Flash contains no valid record.
 * @return   CSS_OPERATION_STORAGE_ERROR    - The Platform read failed.
 */
CSS_OpStatus_t CSS_GetCredential(uint8_t Credential[CSS_CREDENTIAL_LENGTH])
{
    if(Credential == NULL)
    {
        return CSS_OPERATION_INVALID_ARGUMENT;
    }

    uint64_t record = CSS_FLASH_ERASED_RECORD;

    if(PFLASH_ReadDoubleWord(CSS_FLASH_BASE_ADDRESS, &record) != FLASH_OPERATION_OK)
    {
        CSS_ClearCredential(Credential);

        return CSS_OPERATION_STORAGE_ERROR;
    }

    if(!CSS_DecodeRecord(record, Credential))
    {
        CSS_ClearCredential(Credential);

        return CSS_OPERATION_NOT_FOUND;
    }

    return CSS_OPERATION_OK;
}

/**
 * @brief    Checks whether Flash contains one valid registered credential.
 *
 * @details  A record exists only when its format marker, digit ranges and CRC
 *           are all valid. Erased, partially programmed or corrupted data is
 *           reported as no credential. A Platform read failure is also reduced
 *           to false by this Boolean convenience operation.
 *
 * @note     Call CSS_GetCredential() when the caller must distinguish an absent
 *           record from a Platform storage error.
 *
 * @return   true  - A complete valid registered credential is present;
 *           false - The record is unavailable, erased, invalid or unreadable.
 */
bool CSS_HasCredential(void)
{
    uint64_t record = CSS_FLASH_ERASED_RECORD;

    return PFLASH_ReadDoubleWord(CSS_FLASH_BASE_ADDRESS, &record) == FLASH_OPERATION_OK &&
           CSS_DecodeRecord(record, NULL);
}

_Static_assert(CSS_RECORD_LENGTH == sizeof(uint64_t),
               "Credential record shall occupy exactly eight Flash bytes");
