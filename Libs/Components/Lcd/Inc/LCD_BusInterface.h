/**********************************************************************************************************************************
 * @file    LCD_BusInterface.h
 * @brief   Defines the abstract bus interface used by the LCD LCD driver.
 *
 * @details This module declares the hardware abstraction layer between the
 *          LCD driver and the underlying communication interface.
 *          It defines the common bus interface, status codes and data types
 *          required by concrete bus adapter implementations.
 *
 *          The LCD driver interacts exclusively through this interface,
 *          remaining independent of the physical hardware used to communicate
 *          with the display.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_COMPONENTS_LCD_INC_LCD_BUSINTERFACE_H_
#define LIBS_COMPONENTS_LCD_INC_LCD_BUSINTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
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
 * @brief   Status returned by LCD bus interface operations.
 *
 * @note    Returned by bus interface implementations to indicate whether the
 *          requested transfer was successfully completed.
 */
typedef enum
{
    LCD_BUS_OPERATION_OK,
    LCD_BUS_OPERATION_FAIL

}LCD_BusOpStatus_t;

/**
 * @brief   Selects the target LCD internal register.
 *
 * @details Defines the state of the Register Select (RS) signal during a bus
 *          transaction.
 *
 * @note    LCD_BUS_COMMAND selects the Instruction Register (RS = 0).
 *          LCD_BUS_DATA selects the Data Register (RS = 1).
 */
typedef enum
{
    LCD_BUS_COMMAND = 0,
    LCD_BUS_DATA

}LCD_RegisterSelect_t;

/**
 * @brief   Abstract interface used by the LCD driver to access the display bus.
 *
 * @details This structure provides the hardware abstraction layer between the
 *          LCD driver and a specific bus implementation. Concrete adapters
 *          populate this interface with their own context and callback
 *          implementation.
 *
 * @note    The LCD driver interacts exclusively through this interface and
 *          is independent of the underlying hardware implementation.
 */
typedef struct
{
   /**
    * @brief  	Transfer a 4-bit value to the display data bus (D4-D7).
    *
    * @details	Performs a complete nibble write transaction using the underlying
    *           hardware implementation. This includes all bus-specific operations
    *           required to latch the data into the LCD controller.
    *
    * @param   Context - Pointer to the adapter-specific context.
    * @param   Nibble  - 4-bit value to be transmitted.
    * @param   Rs      - Selects whether the transfer targets the instruction  or data register.
    *
    * @return  LCD_BUS_OPERATION_OK   - Indicates that LCD bus transfer operation has been succeed.
    * @return  LCD_BUS_OPERATION_FAIL - Indicates that LCD bus transfer operation has been failed.
    */
    LCD_BusOpStatus_t (*TransferNibble) (void* Context, uint8_t Nibble, LCD_RegisterSelect_t Rs);

   /**
    * @brief   Pointer to the concrete bus adapter context.
    *
    * @details Opaque pointer owned by the adapter implementation. Its actual
    *          type depends on the selected hardware backend (e.g. PCF8574,
    *          direct GPIO, shift register, etc.).
    */
    void* Context;

}LCD_BusInterface_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_LCD_INC_LCD_BUSINTERFACE_H_ */
