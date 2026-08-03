/**********************************************************************************************************************************
 * @file    HD44780_PWM_BacklightAdapter.h
 * @brief   PWM-based backlight adapter for the HD44780 driver.
 *
 * @details Implements the HD44780 backlight interface using a generic PWM
 *          platform interface.
 *
 *          This module acts as an adapter between the hardware-independent
 *          backlight interface expected by the HD44780 driver and a PWM-based
 *          implementation provided by the underlying platform.
 *
 *          The adapter translates generic backlight operations such as turning
 *          the backlight on, turning it off and adjusting its brightness into
 *          the corresponding PWM control operations.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Jul 29, 2026
 **********************************************************************************************************************************/

#ifndef COMPONENTS_HD44780_INC_HD44780_PWM_BACKLIGHTADAPTER_H_
#define COMPONENTS_HD44780_INC_HD44780_PWM_BACKLIGHTADAPTER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "HD44780_BacklightInterface.h"
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
HD44780_BacklightOpStatusTypeDef HD44780_PWM_BacklightAdapterInit(HD44780_BacklightInterfaceTypeDef* Backlight, void* Context);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_HD44780_INC_HD44780_PWM_BACKLIGHTADAPTER_H_ */
