/**********************************************************************************************************************************
 * @file    Display_Render_Service.h
 * @brief   Public interface of the LCD Display Render Service.
 *
 * @details The Display Render Service translates semantic application screens
 *          into fixed 16x2 LCD views. The application selects a screen and, for
 *          password entry, supplies only the number of accepted digits. Raw
 *          credential digits are never received or retained by this module.
 *
 *          The password-entry view masks every accepted digit with a custom
 *          lock character stored in LCD CGRAM. The service owns both a requested
 *          view and the last successfully rendered view so unchanged content is
 *          not retransmitted over the LCD bus.
 *
 *          Screen selection and entry-length changes update only the requested
 *          view. DRS_Update() performs the bounded synchronous LCD operations
 *          required to make the physical display match that view.
 *
 * @note    The service does not initialize the LCD component or its bus and
 *          backlight adapters. A caller-owned, initialized LCD_Handle_t shall be
 *          injected through DRS_Init().
 *
 * @note    The current screen layout targets a 16-column, two-line display using
 *          the 5x8 character font.
 *
 * @note    One DRS_Handle_t instance shall be accessed by only one execution
 *          context at a time.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 16, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_SERVICES_DISPLAY_RENDER_INC_DISPLAY_RENDER_SERVICE_H_
#define LIBS_SERVICES_DISPLAY_RENDER_INC_DISPLAY_RENDER_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "LCD_Driver.h"
#include "stdbool.h"
#include "stdint.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**
 * @brief Maximum number of credential digits represented by lock characters.
 */
#define DRS_ENTRY_DIGIT_CAPACITY (6U)

/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief Execution status returned by Display Render Service operations.
 */
typedef enum
{
    DRS_OPERATION_OK,
    DRS_OPERATION_FAIL

}DRS_OpStatus_t;

/**
 * @brief Semantic LCD screens provided by the service.
 */
typedef enum
{
    DRS_SCREEN_PASSWORD_ENTRY,
    DRS_SCREEN_ENTRY_TIMEOUT,
    DRS_SCREEN_ENTRY_INCOMPLETE,
    DRS_SCREEN_ACCESS_GRANTED,
    DRS_SCREEN_ACCESS_DENIED,
    DRS_SCREEN_COUNT

}DRS_Screen_t;

/**
 * @brief Logical display view requested or rendered by the service.
 *
 * @details EnteredDigits is visually relevant only to
 *          DRS_SCREEN_PASSWORD_ENTRY. It contains a count and never credential
 *          digit values.
 */
typedef struct
{
    DRS_Screen_t Screen;
    uint8_t      EnteredDigits;

}DRS_View_t;

/**
 * @brief Runtime state of one Display Render Service instance.
 *
 * @warning Members are private service state and shall not be read or modified
 *          directly by callers.
 */
typedef struct
{
                                      /*< Private data. Do not read or modify!                         */
    LCD_Handle_t* _lcd;               /*< Injected initialized LCD; ownership is not transferred.     */
    DRS_View_t    _requested_view;    /*< Logical view most recently requested by the application.    */
    DRS_View_t    _rendered_view;     /*< Logical view last rendered successfully to the physical LCD. */
    bool          _initialized;       /*< Indicates whether initialization and default render passed. */

}DRS_Handle_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
DRS_OpStatus_t DRS_Init             (DRS_Handle_t* Instance, LCD_Handle_t* LCD);
DRS_OpStatus_t DRS_SetScreen        (DRS_Handle_t* Instance, DRS_Screen_t Screen);
DRS_OpStatus_t DRS_SetEnteredDigits (DRS_Handle_t* Instance, uint8_t EnteredDigits);
DRS_OpStatus_t DRS_Update           (DRS_Handle_t* Instance);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_SERVICES_DISPLAY_RENDER_INC_DISPLAY_RENDER_SERVICE_H_ */
