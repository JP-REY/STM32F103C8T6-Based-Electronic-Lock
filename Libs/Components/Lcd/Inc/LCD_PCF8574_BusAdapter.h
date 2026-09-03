/**********************************************************************************************************************************
 * @file    LCD_PCF8574_BusAdapter.h
 * @brief   Implements the LCD bus interface using a PCF8574 I/O expander.
 *
 * @details This module provides a concrete implementation of the LCD bus
 *          interface for systems where the display is connected through a
 *          PCF8574 I²C I/O expander.
 *
 *          It translates generic LCD bus operations into the appropriate
 *          PCF8574 port transactions, handling all hardware-specific signaling
 *          required to communicate with the display.
 *
 * @note    This module depends on the PCF8574 driver and is intended to be
 *          used through the abstract LCD bus interface.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_COMPONENTS_LCD_INC_LCD_PCF8574_BUSADAPTER_H_
#define LIBS_COMPONENTS_LCD_INC_LCD_PCF8574_BUSADAPTER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "LCD_BusInterface.h"
#include "PCF8574_Driver.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
LCD_BusOpStatus_t LCD_PCF8574_BusAdapterInit(LCD_BusInterface_t* Bus, PCF8574_Handle_t* PCF8574_Instance);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_LCD_INC_LCD_PCF8574_BUSADAPTER_H_ */
