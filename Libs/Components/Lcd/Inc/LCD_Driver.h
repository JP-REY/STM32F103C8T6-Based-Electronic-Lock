/**********************************************************************************************************************************
 * @file    LCD_Driver.h
 * @brief   Public API for the LCD LCD driver.
 *
 * @details Declares the public types and function prototypes required to
 *          configure and control LCD-compatible character LCD modules.
 *          This interface is hardware-independent and relies on an external
 *          bus interface implementation to communicate with the display.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_COMPONENTS_LCD_INC_LCD_DRIVER_H_
#define LIBS_COMPONENTS_LCD_INC_LCD_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "LCD_BusInterface.h"
#include "LCD_BacklightInterface.h"
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
 * @brief   LCD Operation Status type.
 *
 * @details Indicates the status of LCD driver operation.
 *
 * @note    LCD_OPERATION_OK/LCD_OPERATION_FAIL indicates whether
 *          LCD operation was succeed or not.
 *
 */
typedef enum
{
    LCD_OPERATION_OK,
    LCD_OPERATION_FAIL

}LCD_OpStatus_t;

/**
 * @brief   LCD data interface mode.
 *
 * @details Selects whether the LCD communicates through an 8-bit or a 4-bit
 *          parallel data interface.
 *
 * @note    The selected mode must match the physical hardware connection
 *          between the controller and the display.
 */
typedef enum
{
    LCD_8BIT_MODE,
    LCD_4BIT_MODE

}LCD_InterfaceMode_t;

/**
 * @brief   LCD character font configuration.
 *
 * @details Selects the character font used by LCD.
 *
 * @note    The 5x10 font is supported only in single-line display mode,
 *          according to the LCD specification.
 */
typedef enum
{
    LCD_5X10_FONT,
    LCD_5X8_FONT

}LCD_CharacterFont_t;

/**
 * @brief   LCD display line configuration.
 *
 * @details Selects whether the display operates in single-line or two-line
 *          mode.
 *
 * @note    The enumeration values match the bit encoding required by the
 *          Function Set instruction.
 */
typedef enum
{
    LCD_2LINE = 0x01,
    LCD_1LINE = 0x00

}LCD_LineNumber_t;

/**
 * @brief   LCD device instance.
 *
 * @details Stores the configuration and private runtime data associated with
 *          a single LCD LCD controller instance.
 *
 * @warning Members identified as private driver data are intended for internal
 *          driver use only and must not be accessed or modified directly by
 *          the application after initialization.
 */
typedef struct
{
                                                /*< Private data. Do not read or modify !           */
    LCD_BusInterface_t       _bus;              /*< Communication bus interface used by the driver. */
    LCD_BacklightInterface_t _backlight;        /*< Backlight control interface used by the driver. */
    LCD_LineNumber_t         _rows;             /*< Number of display lines configure for LCD.      */
    uint8_t                  _cols;             /*< Number of display columns.                      */
    LCD_InterfaceMode_t      _interface_mode;   /*< Selected LCD data interface mode.               */
    LCD_CharacterFont_t      _font_dot_size;    /*< Selected character font configuration.          */
    bool                     _initialized;      /*< Indicates if LCD instance has initialized       */

}LCD_Handle_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
LCD_OpStatus_t LCD_Init            (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_Clear           (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_Home            (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_DisplayOn       (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_DisplayOff      (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_CursorOn        (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_CursorOff       (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_BlinkOn         (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_BlinkOff        (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_IncrementCursor (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_DecrementCursor (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_EnableShift     (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_DisableShift    (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_SetCursor       (LCD_Handle_t* Device, uint8_t Row, uint8_t Col);
LCD_OpStatus_t LCD_WriteChar       (LCD_Handle_t* Device, uint8_t Char);
LCD_OpStatus_t LCD_WriteString     (LCD_Handle_t* Device, const char* String);
LCD_OpStatus_t LCD_PrintLine       (LCD_Handle_t* Device, uint8_t Row, const char* Text);
LCD_OpStatus_t LCD_ClearLine       (LCD_Handle_t* Device, uint8_t Row);
LCD_OpStatus_t LCD_CreateChar      (LCD_Handle_t* Device, uint8_t Position, const uint8_t* PatternBitMap);
LCD_OpStatus_t LCD_WriteCustomChar (LCD_Handle_t* Device, uint8_t CharPosition);
LCD_OpStatus_t LCD_BacklightOn     (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_BacklightOff    (LCD_Handle_t* Device);
LCD_OpStatus_t LCD_SetBrightness   (LCD_Handle_t* Device, uint16_t BrightPercent);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_LCD_INC_LCD_DRIVER_H_ */
