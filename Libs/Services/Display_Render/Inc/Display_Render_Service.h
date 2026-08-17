/**********************************************************************************************************************************
 * @file    Display_Render_Service.h
 * @brief   Public interface of the LCD Display Render Service.
 *
 * @details The Display Render Service is a singleton presentation module that
 *          translates semantic application screens into fixed 16x2 LCD views.
 *          The application selects a screen and, for password entry, supplies
 *          only the number of accepted digits. Raw credential digits are never
 *          received or retained by this module.
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
 * @note    All mutable runtime state is private to the implementation. The
 *          function-based API shall be accessed by only one serialized
 *          execution context at a time.
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
 * @brief   Maximum number of credential digits represented by lock characters.
 *
 * @details Defines the inclusive upper bound accepted by
 *          DRS_SetEnteredDigits(). The service renders one custom lock character
 *          for each accepted digit count position.
 */
#define DRS_ENTRY_DIGIT_CAPACITY (6U)

/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   Execution status returned by Display Render Service operations.
 *
 * @details Reports whether the requested state change or synchronous LCD
 *          rendering operation completed successfully.
 */
typedef enum
{
    DRS_OPERATION_OK,   /*< The requested operation completed successfully. */
    DRS_OPERATION_FAIL  /*< The requested operation could not be completed. */

}DRS_OpStatus_t;

/**
 * @brief   Semantic LCD screens provided by the service.
 *
 * @details Identifies the fixed presentation views that the application may
 *          request without supplying arbitrary display text.
 *
 * @note    DRS_SCREEN_COUNT is a boundary and invalid-view sentinel. It is not
 *          a renderable screen and shall not be passed to DRS_SetScreen().
 */
typedef enum
{
    DRS_SCREEN_PASSWORD_ENTRY,   /*< Password prompt with dynamic lock-character progress.  */

    DRS_SCREEN_ENTRY_TIMEOUT,    /*< Feedback indicating that credential entry timed out.   */

    DRS_SCREEN_ENTRY_INCOMPLETE, /*< Feedback indicating that the credential is incomplete. */

    DRS_SCREEN_ACCESS_GRANTED,   /*< Feedback indicating successful authentication.         */

    DRS_SCREEN_ACCESS_DENIED,    /*< Feedback indicating rejected authentication.           */

    DRS_SCREEN_COUNT             /*< Number of screens and invalid rendered-screen marker.  */

}DRS_Screen_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
DRS_OpStatus_t DRS_Init             (LCD_Handle_t* Lcd);
DRS_OpStatus_t DRS_SetScreen        (DRS_Screen_t Screen);
DRS_OpStatus_t DRS_SetEnteredDigits (uint8_t EnteredDigits);
DRS_OpStatus_t DRS_Update           (void);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_SERVICES_DISPLAY_RENDER_INC_DISPLAY_RENDER_SERVICE_H_ */
