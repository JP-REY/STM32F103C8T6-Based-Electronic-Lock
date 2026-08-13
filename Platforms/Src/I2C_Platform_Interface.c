/**********************************************************************************************************************************
 * @file    I2C_Interface.c
 *
 * @brief   This module implements the platform-specific I2C communication interface using the STM32 HAL library.
 *
 * @details It provides generic I2C read and write operations that abstract the underlying platform-specific communication
 *          mechanism, allowing higher-level drivers to perform I2C communication without directly depending on platform-specific
 *          types or APIs.
 *
 *          The platform interface defines only the I2C communication operations required by higher-level drivers. The execution
 *          model used to perform these operations, such as blocking, interrupt-driven, DMA-based, or RTOS-synchronized
 *          communication, is determined by the concrete platform implementation.
 *
 *          The platform interface receives the peripheral context through a generic void pointer, which is internally interpreted
 *          according to the target platform implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Ago 07, 2026
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "I2C_Platform_Interface.h"
#include "stm32f4xx.h"

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
 * @brief   Transmits a sequence of bytes to an I2C slave device.
 *
 * @details This function provides the platform-independent I2C write operation required by higher-level drivers.
 *
 *          The interface does not define how the transmission is performed. The concrete platform implementation is responsible
 *          for selecting the appropriate communication mechanism, such as blocking, interrupt-driven, DMA-based, or
 *          RTOS-synchronized communication.
 *
 *          The platform-specific result is translated into a platform-independent I2C operation status.
 *
 * @param   Context - Pointer to the platform-specific I2C peripheral context.
 * @param   Address - I2C slave address in the 7-bit format.
 * @param   Data    - Pointer to the data buffer containing the bytes to be transmitted.
 * @param   Size    - Number of bytes to transmit.
 * @param   Timeout - Implementation-defined timeout parameter. Its behavior and applicability depend on the concrete platform
 *                    implementation.
 *
 * @note    The Context pointer must reference a valid and properly initialized platform-specific I2C context.
 *
 * @return  I2C_OPERATION_OK       - The write operation completed successfully.
 * @return  I2C_OPERATION_ERROR    - An I2C communication error occurred.
 * @return  I2C_OPERATION_BUSY     - The I2C resource is currently unavailable.
 * @return  I2C_OPERATION_TIMEOUT  - The operation did not complete within the implementation-defined timeout.
 ********** ************************************************************************************************************************/
I2C_OpStatus_t PI2C_Write(void* Context, uint8_t Address, uint8_t *Data, uint16_t Size, uint32_t Timeout)
{
    HAL_StatusTypeDef HAL_I2C_Status = HAL_I2C_Master_Transmit((I2C_HandleTypeDef*)(Context), (Address << 1), Data, Size, Timeout);

    switch(HAL_I2C_Status)
    {
        case HAL_OK:
        {
            return I2C_OPERATION_OK;
        }
        case HAL_ERROR:
        {
            return I2C_OPERATION_ERROR;
        }
        case HAL_BUSY:
        {
            return I2C_OPERATION_BUSY;
        }
        case HAL_TIMEOUT:
        {
            return I2C_OPERATION_TIMEOUT;
        }
        default:
        {
            return I2C_OPERATION_ERROR;
        }
    }
}

/**********************************************************************************************************************************
 * @brief   Receives a sequence of bytes from an I2C slave device.
 *
 * @details This function provides the platform-independent I2C read operation required by higher-level drivers.
 *
 *          The interface does not define how the reception is performed. The concrete platform implementation is responsible
 *          for selecting the appropriate communication mechanism, such as blocking, interrupt-driven, DMA-based, or
 *          RTOS-synchronized communication.
 *
 *          The platform-specific result is translated into a platform-independent I2C operation status.
 *
 * @param   Context - Pointer to the platform-specific I2C peripheral context.
 * @param   Address - I2C slave address in the 7-bit format.
 * @param   Data    - Pointer to the buffer where the received data will be stored.
 * @param   Size    - Number of bytes to receive.
 * @param   Timeout - Implementation-defined timeout parameter. Its behavior and applicability depend on the concrete platform
 *                    implementation.
 *
 * @note    The Context pointer must reference a valid and properly initialized platform-specific I2C context.
 *
 * @return  I2C_OPERATION_OK       - The read operation completed successfully.
 * @return  I2C_OPERATION_ERROR    - An I2C communication error occurred.
 * @return  I2C_OPERATION_BUSY     - The I2C resource is currently unavailable.
 * @return  I2C_OPERATION_TIMEOUT  - The operation did not complete within the implementation-defined timeout.
 ********** ************************************************************************************************************************/
I2C_OpStatus_t PI2C_Read(void* Context, uint8_t Address, uint8_t *Data, uint16_t Size, uint32_t Timeout)
{
    HAL_StatusTypeDef HAL_I2C_Status = HAL_I2C_Master_Receive((I2C_HandleTypeDef*)(Context), (Address << 1), Data, Size, Timeout);

    switch(HAL_I2C_Status)
    {
        case HAL_OK:
        {
            return I2C_OPERATION_OK;
        }
        case HAL_ERROR:
        {
            return I2C_OPERATION_ERROR;
        }
        case HAL_BUSY:
        {
            return I2C_OPERATION_BUSY;
        }
        case HAL_TIMEOUT:
        {
            return I2C_OPERATION_TIMEOUT;
        }
        default:
        {
            return I2C_OPERATION_ERROR;
        }
    }
}


