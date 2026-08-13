/**********************************************************************************************************************************
 * @file    PCF8574_Driver.c
 * @brief   PCF8574_Driver.h module implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Jul 15, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "PCF8574_Driver.h"

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
static bool PCF8754_IsInit(PCF8574_Handle_t* Device);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief Check whether the PCF8574 device instance is initialized.
 *
 * @param  Device - Device pointer to the PCF8574 device handle.
 *
 * @return true   - The device instance is initialized.
 * @return false  - The device instance is not initialized.
 **********************************************************************************************************************************/
static bool PCF8754_IsInit(PCF8574_Handle_t* Device)
{
    return Device->_initialized;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Initializes a PCF8574 device instance.
 *
 * @details Configures the device handle with the specified I2C address and initializes
 *          the internal driver state required for subsequent operations.
 *
 * @param   Device      - Device pointer to the PCF8574 device handle.
 * @param   Address     - Address I2C address assigned to the PCF8574 device.
 * @param   I2C_Context - Pointer to I2C platform-specific context.
 *
 * @note    This function must be called before using any other driver operation.
 *
 * @return  PCF8574_OPERATION_OK   - Indicates that PCF8574 operation has been succeed.
 * @return  PCF8574_OPERATION_FAIL - Indicates that PCF8574 operation has been failed.
 **********************************************************************************************************************************/
PCF8574_OpStatus_t PCF8574_Init(PCF8574_Handle_t* Device, uint8_t Address, void* I2C_Context)
{
    if(Device == NULL || I2C_Context == NULL)
    {
        return PCF8574_OPERATION_FAIL;
    }
    /* If PCF8754 instace has been already initialized return */
    if(Device->_initialized)
    {
        return PCF8574_OPERATION_OK;
    }

    Device->_i2c_context    = I2C_Context;
    Device->_device_address = Address;
    Device->_port_shadow    = 0x00;
    Device->_initialized    = true;

    if(PCF8574_ClearPort(Device) != PCF8574_OPERATION_OK)
    {
        return PCF8574_OPERATION_FAIL;
    }

    return PCF8574_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief  	Deinitializes a PCF8574 device instance.
 *
 * @details	Resets the device handle and invalidates its internal initialization state,
 *          preventing further driver operations until the device is initialized again.
 *
 * @param  	Device  - Device pointer to the PCF8574 device handle.
 *
 * @note   	None.
 *
 * @return  PCF8574_OPERATION_OK   - Indicates that PCF8574 operation has been succeed.
 * @return  PCF8574_OPERATION_FAIL - Indicates that PCF8574 operation has been failed.
 **********************************************************************************************************************************/
PCF8574_OpStatus_t PCF8574_Deinit(PCF8574_Handle_t* Device)
{
    if(Device == NULL || !(PCF8754_IsInit(Device)))
    {
      return PCF8574_OPERATION_FAIL;
    }

    if(PCF8574_ClearPort(Device) != PCF8574_OPERATION_OK)
    {
        return PCF8574_OPERATION_FAIL;
    }

    /* Deinitialize device instance */
    Device->_i2c_context    = NULL;
    Device->_device_address = 0x00;
    Device->_port_shadow    = 0x00;
    Device->_initialized    = false;

    return PCF8574_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief  	Writes an 8-bit value to the PCF8574 I/O port.
 *
 * @details	Updates all eight PCF8574 I/O pins according to the bit values provided
 *          in the port massk.
 *
 * @param  	Device  - Device pointer to the PCF8574 device handle.
 * @param  	Mask    - Mask 8-bit value representing the desired port state.
 *
 * @note   	The internal port shadow is updated only after a succesful I2C transmission,
 *          preventing corrupted port data.
 *
 * @return  PCF8574_OPERATION_OK   - Indicates that PCF8574 operation has been succeed.
 * @return  PCF8574_OPERATION_FAIL - Indicates that PCF8574 operation has been failed.
 **********************************************************************************************************************************/
PCF8574_OpStatus_t PCF8574_WritePort(PCF8574_Handle_t* Device, uint8_t Mask)
{
    uint8_t data_buffer = Mask;

    if(Device == NULL || !(PCF8754_IsInit(Device)))
    {
        return PCF8574_OPERATION_FAIL;
    }

    I2C_OpStatus_t PCF8574_WriteStatus = PI2C_Write(Device->_i2c_context,Device->_device_address,&data_buffer,1,10);

    if(PCF8574_WriteStatus != I2C_OPERATION_OK)
    {
        return PCF8574_OPERATION_FAIL;
    }

    Device->_port_shadow = data_buffer;

    return PCF8574_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief  	Reads the current state of the PCF8574 I/O port.
 *
 * @details	Retrieves the logic state of all eight PCF8574 I/O pins through the I2C interface.
 *
 * @param  	Device  - Device pointer to the PCF8574 device handle.
 * @param   Buffer  - Pointer to the output buffer where the data 
 *                    read from the hardware port will be stored.
 *
 * @note   	Relevant implementation note, constraint or side effect.
 *
 * @return  PCF8574_OPERATION_OK   - Indicates that PCF8574 operation has been succeed.
 * @return  PCF8574_OPERATION_FAIL - Indicates that PCF8574 operation has been failed.
 **********************************************************************************************************************************/
PCF8574_OpStatus_t PCF8574_ReadPort(PCF8574_Handle_t* Device, uint8_t* Buffer)
{
    if(Device == NULL || Buffer == NULL || !(PCF8754_IsInit(Device)) )
    {
        return PCF8574_OPERATION_FAIL;
    }

    I2C_OpStatus_t PCF8574_ReadStatus =  PI2C_Read(Device->_i2c_context,Device->_device_address,Buffer,1,10);

    if(PCF8574_ReadStatus != I2C_OPERATION_OK)
    {
        return PCF8574_OPERATION_FAIL;
    }

    return PCF8574_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief  	Clears all PCF8574 I/O port bits.
 *
 * @details	Writes logic low to all eight PCF8574 I/O pins.
 *
 * @param  	Device  - Device pointer to the PCF8574 device handle.
 *
 * @note   	None.
 *
 * @return  PCF8574_OPERATION_OK   - Indicates that PCF8574 operation has been succeed.
 * @return  PCF8574_OPERATION_FAIL - Indicates that PCF8574 operation has been failed.
 **********************************************************************************************************************************/
PCF8574_OpStatus_t PCF8574_ClearPort(PCF8574_Handle_t* Device)
{
    if(Device == NULL || !(PCF8754_IsInit(Device)))
    {
        return PCF8574_OPERATION_FAIL;
    }

    return PCF8574_WritePort(Device, 0x00);
}

/**********************************************************************************************************************************
 * @brief  	Sets a specific PCF8574 I/O port bit.
 *
 * @details	Sets the selected I/O bit to logic high while preserving the state of the
 *          remaining port bits using the internal port shadow.
 *
 * @param  	Device  - Device pointer to the PCF8574 device handle.
 * @param  	Bit     - Port bit position to be set.
 *
 * @note   	Valid bit positions range from 0 to 7.
 *
 * @return  PCF8574_OPERATION_OK   - Indicates that PCF8574 operation has been succeed.
 * @return  PCF8574_OPERATION_FAIL - Indicates that PCF8574 operation has been failed.
 **********************************************************************************************************************************/
PCF8574_OpStatus_t PCF8574_WriteBit(PCF8574_Handle_t* Device, uint8_t Bit)
{
    uint8_t data_buffer = 0x00;

    if(Device == NULL || !(PCF8754_IsInit(Device)))
    {
        return PCF8574_OPERATION_FAIL;
    }

    data_buffer = Device->_port_shadow |= (1 << Bit);

    return PCF8574_WritePort(Device, data_buffer);
}

/**********************************************************************************************************************************
 * @brief  	Reads the state of a specific PCF8574 I/O port bit.
 *
 * @details	Reads the PCF8574 port and extracts the logic state corresponding to the
 *          specified bit position.
 *
 * @param   Device  - Device pointer to the PCF8574 device handle.
 * @param   Bit     - Port bit position to be read.
 * @param   Buffer  - Pointer to an output buffer that will receive 
 *                    the extracted bit state (represented as 0 or 1).
 *
 * @note   	Valid bit positions range from 0 to 7.
 *
 * @return  PCF8574_OPERATION_OK   - Indicates that PCF8574 operation has been succeed.
 * @return  PCF8574_OPERATION_FAIL - Indicates that PCF8574 operation has been failed.
 **********************************************************************************************************************************/
PCF8574_OpStatus_t PCF8574_ReadBit(PCF8574_Handle_t* Device, uint8_t Bit, uint8_t* Buffer)
{
    uint8_t data_buffer = 0x00;

    if(Device == NULL || Buffer == NULL || !(PCF8754_IsInit(Device)))
    {
        return PCF8574_OPERATION_FAIL;
    }

    if(PCF8574_ReadPort(Device, &data_buffer) != PCF8574_OPERATION_OK)
    {
        return PCF8574_OPERATION_FAIL;
    }

    *Buffer = (data_buffer) & (1 << Bit);

    return PCF8574_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief  	Clears a specific PCF8574 I/O port bit.
 *
 * @details	Sets the selected I/O bit to logic low while preserving the state of the
 *          remaining port bits using the internal port shadow.
 *
 * @param   Device  - Device pointer to the PCF8574 device handle.
 * @param   Bit     - Port bit position to be cleared.
 *
 * @note   	Valid bit positions range from 0 to 7.
 *
 * @return  PCF8574_OPERATION_OK   - Indicates that PCF8574 operation has been succeed.
 * @return  PCF8574_OPERATION_FAIL - Indicates that PCF8574 operation has been failed.
 **********************************************************************************************************************************/
PCF8574_OpStatus_t PCF8574_ClearBit(PCF8574_Handle_t* Device, uint8_t Bit)
{
    uint8_t data_buffer = 0x00;

    if(Device == NULL || !(PCF8754_IsInit(Device)))
    {
        return PCF8574_OPERATION_FAIL;
    }

    data_buffer = Device->_port_shadow &=  ~(1 << Bit);

    return PCF8574_WritePort(Device, data_buffer);
}

/**********************************************************************************************************************************
 * @brief  	Toggles a specific PCF8574 I/O port bit.
 *
 * @details	Toggles the selected I/O bit while preserving the state of the
 *          remaining port bits using the internal port shadow.
 *
 * @param   Device  - Device pointer to the PCF8574 device handle.
 * @param   Bit     - Port bit position to be toggled.
 *
 * @note   	Valid bit positions range from 0 to 7.
 *
 * @return  PCF8574_OPERATION_OK   - Indicates that PCF8574 operation has been succeed.
 * @return  PCF8574_OPERATION_FAIL - Indicates that PCF8574 operation has been failed.
 **********************************************************************************************************************************/
PCF8574_OpStatus_t PCF8574_ToggleBit(PCF8574_Handle_t* Device, uint8_t Bit)
{
    uint8_t data_buffer = 0x00;

    if(Device == NULL || !(PCF8754_IsInit(Device)))
    {
        return PCF8574_OPERATION_FAIL;
    }

    if(PCF8574_ReadBit(Device, Bit, &data_buffer) != PCF8574_OPERATION_OK)
    {
        return PCF8574_OPERATION_FAIL;
    }
    else
    {
        if(data_buffer == 0x00)
        {
            return PCF8574_WriteBit(Device, Bit);
        }
        else
        {
            return PCF8574_ClearBit(Device, Bit);
        }
    }
}

