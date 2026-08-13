/**********************************************************************************************************************************
 * @file    HD44780_BusInterface.h
 * @brief   Defines the abstract bus interface used by the HD44780 LCD driver.
 *
 * @details This module declares the hardware abstraction layer between the
 *          HD44780 driver and the underlying communication interface.
 *          It defines the common bus interface, status codes and data types
 *          required by concrete bus adapter implementations.
 *
 *          The HD44780 driver interacts exclusively through this interface,
 *          remaining independent of the physical hardware used to communicate
 *          with the display.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Jul 24, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_COMPONENTS_HD44780_INC_HD44780_BUSINTERFACE_H_
#define LIBS_COMPONENTS_HD44780_INC_HD44780_BUSINTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "stdint.h"
#include "stdbool.h"
/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Status returned by HD44780 bus interface operations.
 *
 * @note    Returned by bus interface implementations to indicate whether the
 *          requested transfer was successfully completed.
 **********************************************************************************************************************************/
typedef enum
{
    HD44780_BUS_OPERATION_OK,
    HD44780_BUS_OPERATION_FAIL

}HD44780_BusOpStatus_t;

/**********************************************************************************************************************************
 * @brief   Selects the target HD44780 internal register.
 *
 * @details Defines the state of the Register Select (RS) signal during a bus
 *          transaction.
 *
 * @note    HD44780_BUS_COMMAND selects the Instruction Register (RS = 0).
 *          HD44780_BUS_DATA selects the Data Register (RS = 1).
 **********************************************************************************************************************************/
typedef enum
{
    HD44780_BUS_COMMAND = 0,
    HD44780_BUS_DATA

}HD44780_RegisterSelect_t;

/**********************************************************************************************************************************
 * @brief   Abstract interface used by the HD44780 driver to access the display bus.
 *
 * @details This structure provides the hardware abstraction layer between the
 *          HD44780 driver and a specific bus implementation. Concrete adapters
 *          populate this interface with their own context and callback
 *          implementation.
 *
 * @note    The HD44780 driver interacts exclusively through this interface and
 *          is independent of the underlying hardware implementation.
 **********************************************************************************************************************************/
typedef struct
{
   /**********************************************************************************************************************************
    * @brief  	Transfer a 4-bit value to the display data bus (D4-D7).
    *
    * @details	Performs a complete nibble write transaction using the underlying
    *           hardware implementation. This includes all bus-specific operations
    *           required to latch the data into the HD44780 controller.
    *
    * @param   Context - Pointer to the adapter-specific context.
    * @param   Nibble  - 4-bit value to be transmitted.
    * @param   Rs      - Selects whether the transfer targets the instruction  or data register.
    *
    * @return  HD44780_BUS_OPERATION_OK   - Indicates that HD44780 bus transfer operation has been succeed.
    * @return  HD44780_BUS_OPERATION_FAIL - Indicates that HD44780 bus transfer operation has been failed.
    **********************************************************************************************************************************/
    HD44780_BusOpStatus_t (*TransferNibble) (void* Context, uint8_t Nibble, HD44780_RegisterSelect_t Rs);

   /**********************************************************************************************************************************
    * @brief   Pointer to the concrete bus adapter context.
    *
    * @details Opaque pointer owned by the adapter implementation. Its actual
    *          type depends on the selected hardware backend (e.g. PCF8574,
    *          direct GPIO, shift register, etc.).
    **********************************************************************************************************************************/
    void* Context;

}HD44780_BusInterface_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_HD44780_INC_HD44780_BUSINTERFACE_H_ */
