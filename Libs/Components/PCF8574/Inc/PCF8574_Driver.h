/**********************************************************************************************************************************
 * @file    PCF8574_Driver.h
 * @brief   PCF8574 8-bit I/O expander driver interface.
 *
 * @details This module provides the interface for controlling the PCF8574 I/O expander over the I2C bus.
 *          It provides functions for device initialization and for reading from and writing to the 8-bit
 *          quasi-bidirectional I/O port.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_COMPONENTS_PCF8574_INC_PCF8574_DRIVER_H_
#define LIBS_COMPONENTS_PCF8574_INC_PCF8574_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "I2C_Platform_Interface.h"
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   PCF8574 Operation Status type.
 *
 * @details Indicates the status of PCF8574 driver operation.
 *
 * @note    PCF8574_OPERATION_OK/PCF8574_OPERATION_FAIL indicates whether
 *          PCF8574 operation was succeed or not.
 *
 */
typedef enum
{
    PCF8574_OPERATION_OK,
    PCF8574_OPERATION_FAIL

}PCF8574_OpStatus_t;

/**
 * @brief   PCF8574 device handle.
 *
 * @details This structure represents a PCF8574 device instance and stores the information required by the driver
 *          to communicate with and maintain the state of the device.
 *
 * @note    The I2C context and device address must be properly configured before initializing the driver.
 *
 * @warning Members identified as private driver data are intended for internal driver use only and must
 *          not be accessed or modified directly by application.
 *
 */
typedef struct
{
                             /*< Private data. Do not read or modify!                   */ 
    void*   _i2c_context;    /*< Platform-specific I2C communication context.           */ 
    uint8_t _device_address; /*< PCF8574 I2C device address.                            */ 
    uint8_t _port_shadow;    /*< Software shadow of the current port state.             */ 
    bool    _initialized;    /*< Internal initialization state. Do not access directly. */ 

}PCF8574_Handle_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
PCF8574_OpStatus_t PCF8574_Init      (PCF8574_Handle_t* Device, uint8_t Address, void* I2C_Context);
PCF8574_OpStatus_t PCF8574_Deinit    (PCF8574_Handle_t* Device);
PCF8574_OpStatus_t PCF8574_WritePort (PCF8574_Handle_t* Device, uint8_t Mask);
PCF8574_OpStatus_t PCF8574_ReadPort  (PCF8574_Handle_t* Device, uint8_t* Buffer);
PCF8574_OpStatus_t PCF8574_ClearPort (PCF8574_Handle_t* Device);
PCF8574_OpStatus_t PCF8574_WriteBit  (PCF8574_Handle_t* Device, uint8_t Bit);
PCF8574_OpStatus_t PCF8574_ReadBit   (PCF8574_Handle_t* Device, uint8_t Bit, uint8_t* Buffer);
PCF8574_OpStatus_t PCF8574_ClearBit  (PCF8574_Handle_t* Device, uint8_t Bit);
PCF8574_OpStatus_t PCF8574_ToggleBit (PCF8574_Handle_t* Device, uint8_t Bit);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_PCF8574_INC_PCF8574_DRIVER_H_ */
