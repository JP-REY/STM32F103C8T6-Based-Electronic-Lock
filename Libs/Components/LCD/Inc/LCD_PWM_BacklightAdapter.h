/**********************************************************************************************************************************
 * @file    LCD_PWM_BacklightAdapter.h
 * @brief   PWM-based backlight adapter for the LCD driver.
 *
 * @details Implements the LCD backlight interface using a generic PWM
 *          platform interface.
 *
 *          This module acts as an adapter between the hardware-independent
 *          backlight interface expected by the LCD driver and a PWM-based
 *          implementation provided by the underlying platform.
 *
 *          The adapter translates generic backlight operations such as turning
 *          the backlight on, turning it off and adjusting its brightness into
 *          the corresponding PWM control operations.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_COMPONENTS_LCD_INC_LCD_PWM_BACKLIGHTADAPTER_H_
#define LIBS_COMPONENTS_LCD_INC_LCD_PWM_BACKLIGHTADAPTER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "LCD_BacklightInterface.h"
#include "PWM_Platform_Interface.h"

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
LCD_BacklightOpStatus_t LCD_PWM_BacklightAdapterInit(LCD_BacklightInterface_t* Backlight, PWM_Handle_t* Context);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_LCD_INC_LCD_PWM_BACKLIGHTADAPTER_H_ */
