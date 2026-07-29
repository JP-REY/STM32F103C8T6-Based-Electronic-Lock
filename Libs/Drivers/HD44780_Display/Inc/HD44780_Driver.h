/**********************************************************************************************************************************
 * @file    HD44780_Driver.h
 * @brief   Public API for the HD44780 LCD driver.
 *
 * @details Declares the public types and function prototypes required to
 *          configure and control HD44780-compatible character LCD modules.
 *          This interface is hardware-independent and relies on an external
 *          bus interface implementation to communicate with the display.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Jul 15, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_DRIVERS_HD44780_LCDDISP_INC_LCD_DRIVER_H_
#define LIBS_DRIVERS_HD44780_LCDDISP_INC_LCD_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "HD44780_BusInterface.h"
#include "stdint.h"
#include "stdbool.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   HD44780 Operation Status type.
 *
 * @details Indicates the status of HD44780 driver operation.
 *
 * @note    HD44780_OPERATION_OK/HD44780_OPERATION_FAIL indicates whether
 *          HD44780 operation was succeed or not.
 *
 **********************************************************************************************************************************/
typedef enum
{
    HD44780_OPERATION_OK,
    HD44780_OPERATION_FAIL

}HD44780_OpStatusTypeDef;

/**********************************************************************************************************************************
 * @brief   HD44780 data interface mode.
 *
 * @details Selects whether the HD44780 communicates through an 8-bit or a 4-bit
 *          parallel data interface.
 *
 * @note    The selected mode must match the physical hardware connection
 *          between the controller and the display.
 **********************************************************************************************************************************/
typedef enum
{
    HD44780_8BIT_MODE,
    HD44780_4BIT_MODE

}HD44780_InterfaceModeTypeDef;

/**********************************************************************************************************************************
 * @brief   HD44780 character font configuration.
 *
 * @details Selects the character font used by HD44780.
 *
 * @note    The 5x10 font is supported only in single-line display mode,
 *          according to the HD44780 specification.
 **********************************************************************************************************************************/
typedef enum
{
    HD44780_5X10_FONT,
    HD44780_5X8_FONT

}HD44780_CharacterFontTypeDef;

/**********************************************************************************************************************************
 * @brief   HD44780 display line configuration.
 *
 * @details Selects whether the display operates in single-line or two-line
 *          mode.
 *
 * @note    The enumeration values match the bit encoding required by the
 *          Function Set instruction.
 **********************************************************************************************************************************/
typedef enum
{
    HD44780_2LINE = 0x01,
    HD44780_1LINE = 0x00

}HD44780_LineNumberTypeDef;

/**********************************************************************************************************************************
 * @brief   HD44780 device instance.
 *
 * @details Stores the configuration and private runtime data associated with
 *          a single HD44780 LCD controller instance.
 *
 * @warning Members identified as private driver data are intended for internal
 *          driver use only and must not be accessed or modified directly by
 *          the application after initialization.
 **********************************************************************************************************************************/
typedef struct
{
    // << Private data. Do not read or modify ! >>
    /* Communication bus interface used by the driver. */ HD44780_BusInterfaceTypeDef  _bus;
    /* Number of display lines configure for HD44780.  */ HD44780_LineNumberTypeDef    _rows;
    /* Number of display columns.                      */ uint8_t                      _cols;
    /* Selected HD44780 data interface mode.           */ HD44780_InterfaceModeTypeDef _interface_mode;
    /* Selected character font configuration.          */ HD44780_CharacterFontTypeDef _font_dot_size;
    /* Indicates if HD44780 instance has initialized   */ bool                         _initialized;

}HD44780_HandleTypeDef;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_Init            (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_Clear           (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_Home            (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_DisplayOn       (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_DisplayOff      (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_CursorOn        (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_CursorOff       (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_BlinkOn         (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_BlinkOff        (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_IncrementCursor (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_DecrementCursor (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_EnableShift     (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_DisableShift    (HD44780_HandleTypeDef* Device);
HD44780_OpStatusTypeDef HD44780_SetCursor       (HD44780_HandleTypeDef* Device, uint8_t Row, uint8_t Col);
HD44780_OpStatusTypeDef HD44780_WriteChar       (HD44780_HandleTypeDef* Device, uint8_t Char);
HD44780_OpStatusTypeDef HD44780_WriteString     (HD44780_HandleTypeDef* Device, const char* String);
HD44780_OpStatusTypeDef HD44780_PrintLine       (HD44780_HandleTypeDef* Device, uint8_t Row, const char* Text);
HD44780_OpStatusTypeDef HD44780_ClearLine       (HD44780_HandleTypeDef* Device, uint8_t Row);


#ifdef __cplusplus
}
#endif

#endif /* LIBS_DRIVERS_HD44780_LCDDISP_INC_LCD_DRIVER_H_ */
