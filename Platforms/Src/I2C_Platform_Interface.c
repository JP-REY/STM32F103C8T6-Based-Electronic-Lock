/**********************************************************************************************************************************
 * @file    I2C_Interface.c
 *
 * @brief   This module implements the platform-specific I2C communication interface using the STM32 HAL library.
 *
 * @details It provides generic read and write operations that abstract the underlying STM32 HAL I2C functions, allowing
 *          higher-level drivers to perform I2C communication without directly depending on HAL-specific types and return codes.
 *
 *          The platform interface receives the peripheral context through a generic void pointer, which is internally cast to
 *          an STM32 I2C_HandleTypeDef pointer before accessing the hardware peripheral.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Jul 20, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "I2C_Platform_Interface.h"
#include "i2c.h"

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
/**********************************************************************************************************************************
 * @brief  	Transmits a sequence of bytes to an I2C slave device in blocking mode.
 *
 * @details	This function provides a platform abstraction for I2C master transmission in blocking mode. The generic context pointer
 *          is interpreted as an STM32 HAL I2C handle and used to perform the transmission through HAL_I2C_Master_Transmit().
 *
 *          The STM32 HAL return status is translated into the corresponding platform-independent I2C status.
 *
 * @param   Context - Pointer to the platform-specific I2C peripheral context. For STM32 platforms, this shall
 *                    point to a valid I2C_HandleTypeDef instance.
 * @param   Address - I2C slave address in the 7-bit format.
 * @param   Data    - Pointer to the data buffer containing the bytes to be transmitted.
 * @param   Size    - Number of bytes to transmit.
 * @param   Timeout - Maximum transmission timeout, in milliseconds.
 *
 * @note   	The Context pointer must reference a valid and initialized STM32 I2C_HandleTypeDef instance.
 *
 * @return  PLATFORM_I2C_OK       Transmission completed successfully.
 * @return  PLATFORM_I2C_ERROR    An I2C communication error occurred.
 * @return  PLATFORM_I2C_BUSY     The I2C peripheral is currently busy.
 * @return  PLATFORM_I2C_TIMEOUT  The transmission did not complete within the specified timeout.
 ********** ************************************************************************************************************************/
PI2C_BOpStatusTypeDef PI2C_WriteBlocking(void* Context, uint8_t Address, uint8_t *Data, uint16_t Size, uint32_t Timeout)
{
    HAL_StatusTypeDef HAL_I2C_Status = HAL_I2C_Master_Transmit((I2C_HandleTypeDef*)(Context), (Address << 1), Data, Size, Timeout);

    switch(HAL_I2C_Status)
    {
        case HAL_OK:
        {
            return I2C_BLOCKING_OPERATION_OK;
        }
        case HAL_ERROR:
        {
            return I2C_BLOCKING_OPERATION_ERROR;
        }
        case HAL_BUSY:
        {
            return I2C_BLOCKING_OPERATION_BUSY;
        }
        case HAL_TIMEOUT:
        {
            return I2C_BLOCKING_OPERATION_TIMEOUT;
        }
        default:
        {
            return I2C_BLOCKING_OPERATION_ERROR;
        }
    }
}

/**********************************************************************************************************************************
 * @brief   Receives a sequence of bytes from an I2C slave device in blocking mode.
 *
 * @details This function provides a platform abstraction for I2C master reception in blocking mode. The generic context pointer
 *          is interpreted as an STM32 HAL I2C handle and used to perform the transmission through HAL_I2C_Master_Receive().
 *
 *          The STM32 HAL return status is translated into the corresponding platform-independent I2C status.
 *
 * @param   Context - Pointer to the platform-specific I2C peripheral context. For STM32 platforms, this shall
 *                    point to a valid I2C_HandleTypeDef instance.
 * @param   Address - I2C slave address in the 7-bit format.
 * @param   Data    - Pointer to the buffer where the received data will be stored.
 * @param   Size    - Number of bytes to receive.
 * @param   Timeout - Maximum transmission timeout, in milliseconds.
 *
 * @note    The Context pointer must reference a valid and initialized STM32 I2C_HandleTypeDef instance.
 *
 * @return  PLATFORM_I2C_OK       Transmission completed successfully.
 * @return  PLATFORM_I2C_ERROR    An I2C communication error occurred.
 * @return  PLATFORM_I2C_BUSY     The I2C peripheral is currently busy.
 * @return  PLATFORM_I2C_TIMEOUT  The transmission did not complete within the specified timeout.
 ********** ************************************************************************************************************************/
PI2C_BOpStatusTypeDef PI2C_ReadBlocking(void* Context, uint8_t Address, uint8_t *Data, uint16_t Size, uint32_t Timeout)
{
    HAL_StatusTypeDef HAL_I2C_Status = HAL_I2C_Master_Receive((I2C_HandleTypeDef*)(Context), (Address << 1), Data, Size, Timeout);

    switch(HAL_I2C_Status)
    {
        case HAL_OK:
        {
            return I2C_BLOCKING_OPERATION_OK;
        }
        case HAL_ERROR:
        {
            return I2C_BLOCKING_OPERATION_ERROR;
        }
        case HAL_BUSY:
        {
            return I2C_BLOCKING_OPERATION_BUSY;
        }
        case HAL_TIMEOUT:
        {
            return I2C_BLOCKING_OPERATION_TIMEOUT;
        }
        default:
        {
            return I2C_BLOCKING_OPERATION_ERROR;
        }
    }
}


















