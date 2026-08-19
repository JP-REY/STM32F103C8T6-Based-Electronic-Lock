/**********************************************************************************************************************************
 * @file    Display_Render_Service.c
 * @brief   Display Render Service implementation.
 *
 * @details Implements semantic 16x2 screens, masked password-entry rendering
 *          through a custom lock character, and coalescing between requested
 *          and successfully rendered views.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 16, 2026
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Display_Render_Service.h"
#include "stddef.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/** @brief Zero-based LCD row used for the first line of a semantic screen. */
#define DRS_FIRST_ROW                 (0U)

/** @brief Zero-based LCD row used for the second line of a semantic screen. */
#define DRS_SECOND_ROW                (1U)

/** @brief Zero-based LCD column at which password lock characters begin. */
#define DRS_FIRST_COLUMN              (0U)

/** @brief CGRAM position reserved for the service lock character. */
#define DRS_LOCK_CUSTOM_CHAR_POSITION (0U)

/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**
 * @brief   Immutable text content associated with one semantic screen.
 *
 * @details Groups the null-terminated strings written to the first and second
 *          LCD rows during a full-screen render. Dynamic password-entry lock
 *          characters are rendered separately after both fixed lines.
 *
 * @note    Instances of this type are stored in the compile-time screen map.
 */
typedef struct
{
    const char* first_line;  /*< Null-terminated text rendered on LCD row zero. */
    const char* second_line; /*< Null-terminated text rendered on LCD row one.  */

}DRS_ScreenContent_t;

/**
 * @brief   Logical display view requested or tracked by the service.
 *
 * @details Combines a semantic screen identifier with the number of accepted
 *          credential digits. The digit count affects visible output only when
 *          the selected screen is DRS_SCREEN_PASSWORD_ENTRY.
 *
 * @note    This type is private because callers modify singleton state only
 *          through the public function-based API.
 */
typedef struct
{
    DRS_Screen_t screen;         /*< Semantic screen associated with the view.       */
    uint8_t      entered_digits; /*< Number of password mask characters in the view. */

}DRS_View_t;

/**
 * @brief   Private runtime state of the Display Render Service singleton.
 *
 * @details Retains the borrowed LCD dependency, the most recently requested
 *          logical view, the last successfully committed render state and the
 *          service lifecycle flag.
 *
 * @note    Callers cannot allocate or access this type. Exactly one instance is
 *          owned statically by this translation unit.
 */
typedef struct
{
    LCD_Handle_t* lcd;             /*< Borrowed initialized LCD; ownership is not transferred.         */
    DRS_View_t    requested_view;  /*< Logical view most recently requested by the application.        */
    DRS_View_t    rendered_view;   /*< State committed after corresponding LCD operations succeed.     */
    bool          initialized;     /*< Indicates whether initialization and the default render passed. */

}DRS_Handle_t;

/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**
 * @brief   5x8 custom-character bitmap representing a closed padlock.
 *
 * @details Each array element defines one character row and uses the five least
 *          significant bits as visible pixels. DRS_Init() programs the bitmap
 *          into DRS_LOCK_CUSTOM_CHAR_POSITION.
 *
 * @note    The eight-byte pattern targets the LCD 5x8 character font.
 */
static const uint8_t DRS_LockCharacterBitmap[8] =
{
    0x0EU,
    0x11U,
    0x11U,
    0x1FU,
    0x1BU,
    0x1BU,
    0x1FU,
    0x00U
};

/**
 * @brief   Fixed text associated with every renderable semantic screen.
 *
 * @details Uses DRS_Screen_t values as designated indexes so each renderable
 *          screen maps directly to the two null-terminated strings written by
 *          DRS_RenderFullScreen().
 *
 * @note    The password-entry second line is completed dynamically with custom
 *          lock characters according to the requested entered-digit count.
 */
static const DRS_ScreenContent_t DRS_ScreenContentMap[DRS_SCREEN_COUNT] =
{
    [DRS_SCREEN_IDLE] =
    {
        .first_line  = "",
        .second_line = ""
    },
    
    [DRS_SCREEN_PASSWORD_ENTRY] =
    {
        .first_line  = "Insert Password:",
        .second_line = ""
    },

    [DRS_SCREEN_ENTRY_TIMEOUT] =
    {
        .first_line  = "Entry Timeout",
        .second_line = ""
    },

    [DRS_SCREEN_ENTRY_INCOMPLETE] =
    {
        .first_line  = "Entry Incomplete",
        .second_line = ""
    },

    [DRS_SCREEN_ACCESS_GRANTED] =
    {
        .first_line  = "Access Granted",
        .second_line = "Welcome!"
    },

    [DRS_SCREEN_ACCESS_DENIED] =
    {
        .first_line  = "Access Denied!",
        .second_line = ""
    },

    [DRS_SCREEN_LOCKOUT] =
    {
        .first_line  = "Lockout!",
        .second_line = ""
    }
};

/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/**
 * @brief   Singleton runtime instance of the Display Render Service.
 *
 * @details Owns all mutable service state and retains the LCD dependency
 *          injected through DRS_Init(). Static zero initialization leaves the
 *          service unbound and uninitialized before its first initialization.
 *
 * @note    The instance is private and shall be accessed only by functions in
 *          this translation unit.
 */
static DRS_Handle_t DRS_Runtime_Instance;

/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
static bool           DRS_IsScreenValid       (DRS_Screen_t Screen);
static bool           DRS_ViewNeedsRender     (void);
static DRS_OpStatus_t DRS_RenderLockCharacters(uint8_t Start, uint8_t End);
static DRS_OpStatus_t DRS_ClearLockCharacters (uint8_t Start, uint8_t End);
static DRS_OpStatus_t DRS_RenderEntryDelta    (void);
static DRS_OpStatus_t DRS_RenderFullScreen    (void);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Reports whether a screen identifier is renderable.
 *
 * @details Converts the enumeration value to an unsigned integer and compares
 *          it with DRS_SCREEN_COUNT. This rejects the sentinel, values above it
 *          and negative enumeration values after unsigned conversion.
 *
 * @param   Screen - Semantic screen identifier to validate.
 *
 * @note    This helper does not modify singleton state or access the LCD.
 *
 * @return  true when Screen indexes DRS_ScreenContentMap; otherwise false.
 */
static bool DRS_IsScreenValid(DRS_Screen_t Screen)
{
    return ((uint32_t)Screen < (uint32_t)DRS_SCREEN_COUNT);
}

/**
 * @brief   Reports whether the physical display may differ from the request.
 *
 * @details A changed semantic screen always requires rendering. When the
 *          password-entry screen remains selected, a changed entered-digit
 *          count also requires rendering. Digit-count differences are ignored
 *          for every static feedback screen.
 *
 * @note    This helper reads singleton state without modifying it.
 *
 * @return  true when a full-screen or incremental render is required;
 *          otherwise false.
 */
static bool DRS_ViewNeedsRender(void)
{
    if(DRS_Runtime_Instance.requested_view.screen != DRS_Runtime_Instance.rendered_view.screen)
    {
        return true;
    }

    return (DRS_Runtime_Instance.requested_view.screen == DRS_SCREEN_PASSWORD_ENTRY &&
            DRS_Runtime_Instance.requested_view.entered_digits != DRS_Runtime_Instance.rendered_view.entered_digits);
}

/**
 * @brief   Writes lock characters over a half-open column range.
 *
 * @details Positions the LCD cursor at row DRS_SECOND_ROW and column Start,
 *          then writes the custom lock character once for every position in
 *          the half-open range [Start, End).
 *
 * @param   Start - First zero-based column to render, inclusive.
 * @param   End   - Final zero-based column boundary, exclusive.
 *
 * @note    An empty range succeeds without accessing the LCD. A reversed range
 *          or an End value above DRS_ENTRY_DIGIT_CAPACITY is rejected.
 *
 * @return  DRS_OPERATION_OK   - When the complete range was rendered or empty;
 *          DRS_OPERATION_FAIL - When the range is invalid or an LCD operation
 *                               fails.
 */
static DRS_OpStatus_t DRS_RenderLockCharacters(uint8_t Start, uint8_t End)
{
    if(Start > End || End > DRS_ENTRY_DIGIT_CAPACITY)
    {
        return DRS_OPERATION_FAIL;
    }

    if(Start == End)
    {
        return DRS_OPERATION_OK;
    }

    if(LCD_SetCursor(DRS_Runtime_Instance.lcd, DRS_SECOND_ROW, Start) != LCD_OPERATION_OK)
    {
        return DRS_OPERATION_FAIL;
    }

    for(uint8_t position = Start; position < End; position++)
    {
        if(LCD_WriteCustomChar(DRS_Runtime_Instance.lcd, DRS_LOCK_CUSTOM_CHAR_POSITION) != LCD_OPERATION_OK)
        {
            return DRS_OPERATION_FAIL;
        }
    }

    return DRS_OPERATION_OK;
}

/**
 * @brief   Clears lock characters over a half-open column range.
 *
 * @details Positions the LCD cursor at row DRS_SECOND_ROW and column Start,
 *          then overwrites every position in the half-open range [Start, End)
 *          with an ASCII space.
 *
 * @param   Start - First zero-based column to clear, inclusive.
 * @param   End   - Final zero-based column boundary, exclusive.
 *
 * @note    An empty range succeeds without accessing the LCD. A reversed range
 *          or an End value above DRS_ENTRY_DIGIT_CAPACITY is rejected.
 *
 * @return  DRS_OPERATION_OK   - When the complete range was cleared or empty;
 *          DRS_OPERATION_FAIL - When the range is invalid or an LCD operation
 *                               fails.
 */
static DRS_OpStatus_t DRS_ClearLockCharacters(uint8_t Start, uint8_t End)
{
    if(Start > End || End > DRS_ENTRY_DIGIT_CAPACITY)
    {
        return DRS_OPERATION_FAIL;
    }

    if(Start == End)
    {
        return DRS_OPERATION_OK;
    }

    if(LCD_SetCursor(DRS_Runtime_Instance.lcd, DRS_SECOND_ROW, Start) != LCD_OPERATION_OK)
    {
        return DRS_OPERATION_FAIL;
    }

    for(uint8_t position = Start; position < End; position++)
    {
        if(LCD_WriteChar(DRS_Runtime_Instance.lcd, (uint8_t)' ') != LCD_OPERATION_OK)
        {
            return DRS_OPERATION_FAIL;
        }
    }

    return DRS_OPERATION_OK;
}

/**
 * @brief   Applies the changed password-mask positions to the LCD.
 *
 * @details Compares the requested and rendered entered-digit counts. An
 *          increase writes only new lock characters, while a decrease clears
 *          only positions that are no longer requested.
 *
 * @note    This helper does not commit rendered_view.entered_digits. The caller
 *          performs that commit only after this function succeeds.
 *
 * @return  DRS_OPERATION_OK   - When the entry delta was applied or no digit
 *                               position changed;
 *          DRS_OPERATION_FAIL - When a required LCD operation fails.
 */
static DRS_OpStatus_t DRS_RenderEntryDelta(void)
{
    const uint8_t requested_digits = DRS_Runtime_Instance.requested_view.entered_digits;
    const uint8_t rendered_digits  = DRS_Runtime_Instance.rendered_view.entered_digits;

    if(requested_digits > rendered_digits)
    {
        return DRS_RenderLockCharacters(rendered_digits, requested_digits);
    }

    if(requested_digits < rendered_digits)
    {
        return DRS_ClearLockCharacters(requested_digits, rendered_digits);
    }

    return DRS_OPERATION_OK;
}

/**
 * @brief   Clears and renders the complete requested semantic screen.
 *
 * @details Resolves the requested screen through DRS_ScreenContentMap, clears
 *          the LCD, prints both fixed lines and, for password entry, writes the
 *          complete requested lock-character sequence from the first column.
 *
 * @note    This helper assumes the requested screen was validated before being
 *          stored. It does not commit either member of rendered_view.
 *
 * @return  DRS_OPERATION_OK   - When every requested screen element was
 *                               rendered successfully;
 *          DRS_OPERATION_FAIL - When mapped text is invalid or a required LCD
 *                               operation fails.
 */
static DRS_OpStatus_t DRS_RenderFullScreen(void)
{
    const DRS_ScreenContent_t* content =
        &DRS_ScreenContentMap[DRS_Runtime_Instance.requested_view.screen];

    if(content->first_line == NULL || content->second_line == NULL)
    {
        return DRS_OPERATION_FAIL;
    }

    if(LCD_Clear(DRS_Runtime_Instance.lcd) != LCD_OPERATION_OK)
    {
        return DRS_OPERATION_FAIL;
    }

    if(LCD_PrintLine(DRS_Runtime_Instance.lcd, DRS_FIRST_ROW, content->first_line) != LCD_OPERATION_OK)
    {
        return DRS_OPERATION_FAIL;
    }

    if(LCD_PrintLine(DRS_Runtime_Instance.lcd, DRS_SECOND_ROW, content->second_line) != LCD_OPERATION_OK)
    {
        return DRS_OPERATION_FAIL;
    }

    if(DRS_Runtime_Instance.requested_view.screen == DRS_SCREEN_PASSWORD_ENTRY)
    {
        if(DRS_RenderLockCharacters(DRS_FIRST_COLUMN,
                                    DRS_Runtime_Instance.requested_view.entered_digits) != DRS_OPERATION_OK)
        {
            return DRS_OPERATION_FAIL;
        }
    }

    return DRS_OPERATION_OK;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes the Display Render Service singleton.
 *
 * @details Stores the borrowed LCD reference, resets the requested view to an
 *          empty password-entry screen, invalidates the rendered screen and
 *          marks the service uninitialized. It then programs the lock bitmap,
 *          enables the lifecycle flag and calls DRS_Update() to render the
 *          default view immediately.
 *
 * @param   Lcd - Pointer to a caller-owned, initialized LCD component.
 *
 * @note    Reinitialization replaces the retained LCD reference and resets all
 *          requested and rendered singleton state. Ownership of Lcd remains
 *          with the composition root.
 *
 * @return  DRS_OPERATION_OK   - When the custom character and default screen
 *                               were rendered successfully;
 *          DRS_OPERATION_FAIL - When Lcd is invalid, is not initialized or a
 *                               required LCD operation fails. The retained LCD
 *                               reference is cleared on failure.
 */
DRS_OpStatus_t DRS_Init(LCD_Handle_t* Lcd)
{
    DRS_Runtime_Instance.lcd                            = Lcd;
    DRS_Runtime_Instance.requested_view.screen          = DRS_SCREEN_IDLE;
    DRS_Runtime_Instance.requested_view.entered_digits  = 0U;
    DRS_Runtime_Instance.rendered_view.screen           = DRS_SCREEN_COUNT;
    DRS_Runtime_Instance.rendered_view.entered_digits   = 0U;
    DRS_Runtime_Instance.initialized                    = false;

    if(LCD_CreateChar(DRS_Runtime_Instance.lcd, DRS_LOCK_CUSTOM_CHAR_POSITION,
                      DRS_LockCharacterBitmap) != LCD_OPERATION_OK)
    {
        DRS_Runtime_Instance.lcd = NULL;

        return DRS_OPERATION_FAIL;
    }

    DRS_Runtime_Instance.initialized = true;

    if(DRS_Update() != DRS_OPERATION_OK)
    {
        DRS_Runtime_Instance.initialized = false;
        DRS_Runtime_Instance.lcd         = NULL;

        return DRS_OPERATION_FAIL;
    }

    return DRS_OPERATION_OK;
}

/**
 * @brief   Stores the semantic screen requested by the application.
 *
 * @details Validates Screen and, when valid, replaces only the screen member of
 *          the singleton requested view. No LCD transaction is performed.
 *
 * @param   Screen - Semantic screen to store in the requested view.
 *
 * @note    This function validates only the screen identifier. It does not
 *          require the singleton to be initialized; normal lifecycle usage
 *          initializes the service before submitting requests.
 *
 * @return  DRS_OPERATION_OK   - When the requested screen was stored;
 *          DRS_OPERATION_FAIL - When Screen is the sentinel or outside the
 *                               renderable range.
 */
DRS_OpStatus_t DRS_SetScreen(DRS_Screen_t Screen)
{
    if(!DRS_IsScreenValid(Screen))
    {
        return DRS_OPERATION_FAIL;
    }

    DRS_Runtime_Instance.requested_view.screen = Screen;

    return DRS_OPERATION_OK;
}

/**
 * @brief   Stores the requested password-entry progress count.
 *
 * @details Replaces only the entered-digit member of the singleton requested
 *          view. The count represents credential length and no raw digit value
 *          is received, retained or rendered.
 *
 * @param   EnteredDigits - Number of mask characters requested in the inclusive
 *                          range from 0U through DRS_ENTRY_DIGIT_CAPACITY.
 *
 * @note    The request changes memory only. Its visible effect is deferred to
 *          DRS_Update() and applies only to the password-entry screen.
 *
 * @return  DRS_OPERATION_OK   - When the count was stored;
 *          DRS_OPERATION_FAIL - When the service is not initialized or the
 *                               count exceeds DRS_ENTRY_DIGIT_CAPACITY.
 */
DRS_OpStatus_t DRS_SetEnteredDigits(uint8_t EnteredDigits)
{
    if(!DRS_Runtime_Instance.initialized ||
       EnteredDigits > DRS_ENTRY_DIGIT_CAPACITY)
    {
        return DRS_OPERATION_FAIL;
    }

    DRS_Runtime_Instance.requested_view.entered_digits = EnteredDigits;

    return DRS_OPERATION_OK;
}

/**
 * @brief   Synchronizes the physical LCD with the requested logical view.
 *
 * @details Returns immediately when no visible difference exists. A changed
 *          screen triggers a full render and commits the rendered screen after
 *          success. Otherwise the function applies the password-entry delta
 *          and commits the rendered entered-digit count after success.
 *
 * @note    A failed LCD operation leaves the corresponding requested state
 *          pending. A later call may repeat physical writes that completed
 *          before the failure.
 *
 * @return  DRS_OPERATION_OK   - When no render was required or all required LCD
 *                               operations succeeded;
 *          DRS_OPERATION_FAIL - When no LCD is retained or a required LCD
 *                               operation fails.
 */
DRS_OpStatus_t DRS_Update(void)
{
    if(DRS_Runtime_Instance.lcd == NULL)
    {
        return DRS_OPERATION_FAIL;
    }

    if(!DRS_ViewNeedsRender())
    {
        return DRS_OPERATION_OK;
    }

    if(DRS_Runtime_Instance.requested_view.screen != DRS_Runtime_Instance.rendered_view.screen)
    {
        if(DRS_RenderFullScreen() != DRS_OPERATION_OK)
        {
            return DRS_OPERATION_FAIL;
        }

        DRS_Runtime_Instance.rendered_view.screen = DRS_Runtime_Instance.requested_view.screen;

        return DRS_OPERATION_OK;
    }

    if(DRS_RenderEntryDelta() != DRS_OPERATION_OK)
    {
        return DRS_OPERATION_FAIL;
    }

    DRS_Runtime_Instance.rendered_view.entered_digits = DRS_Runtime_Instance.requested_view.entered_digits;

    return DRS_OPERATION_OK;
}
