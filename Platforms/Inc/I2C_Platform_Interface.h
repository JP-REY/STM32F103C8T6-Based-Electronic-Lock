/**********************************************************************************************************************************
 * @file    I2C_Interface.h
 *
 * @brief   This module defines the public interface for platform-independent I2C communication.
 *
 * @details It provides generic I2C read and write operations that allow higher-level modules and device drivers to perform
 *          I2C communication without directly depending on platform-specific peripheral types or communication APIs.
 *
 *          Platform-specific resources are provided through a generic context pointer and interpreted by the corresponding
 *          platform implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Jul 20, 2026
 **********************************************************************************************************************************/

#ifndef INC_I2C_PLATFORM_INTERFACE_H_
#define INC_I2C_PLATFORM_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "stm32f4xx.h"
#include "stdint.h"
#include "stdbool.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief  	Platform I2C Blocking Operation Status type.
 *
 * @details	Translates platform-specific HAL status into platform I2C interface.
 *
 * @note    This type is valid for I2C blocking operation.
 *
 **********************************************************************************************************************************/
typedef enum
{
    I2C_OPERATION_OK = 0,
    I2C_OPERATION_ERROR,
    I2C_OPERATION_BUSY,
    I2C_OPERATION_TIMEOUT

}PI2C_OpStatusTypeDef;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
PI2C_OpStatusTypeDef PI2C_Write (void* Context, uint8_t Address, uint8_t *Data, uint16_t Size, uint32_t Timeout);
PI2C_OpStatusTypeDef PI2C_Read  (void* Context, uint8_t Address, uint8_t *Data, uint16_t Size, uint32_t Timeout);

#ifdef __cplusplus
}
#endif

#endif /* INC_I2C_PLATFORM_INTERFACE_H_ */
