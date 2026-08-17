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
#define DRS_FIRST_ROW                 (0U)
#define DRS_SECOND_ROW                (1U)
#define DRS_FIRST_COLUMN              (0U)
#define DRS_LOCK_CUSTOM_CHAR_POSITION (0U)

/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**
 * @brief Immutable text content associated with one semantic screen.
 */
typedef struct
{
    const char* FirstLine;
    const char* SecondLine;

}DRS_ScreenContent_t;

/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**
 * @brief 5x8 custom-character bitmap representing a closed padlock.
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
 * @brief Fixed 16x2 text associated with every semantic screen.
 *
 * @note  The password-entry second line is completed dynamically with custom
 *        lock characters according to DRS_View_t::EnteredDigits.
 */
static const DRS_ScreenContent_t DRS_ScreenContentMap[DRS_SCREEN_COUNT] =
{
    [DRS_SCREEN_PASSWORD_ENTRY] =
    {
        .FirstLine  = "Insert Password:",
        .SecondLine = ""
    },

    [DRS_SCREEN_ENTRY_TIMEOUT] =
    {
        .FirstLine  = "Entry Timeout",
        .SecondLine = ""
    },

    [DRS_SCREEN_ENTRY_INCOMPLETE] =
    {
        .FirstLine  = "Entry Incomplete",
        .SecondLine = ""
    },

    [DRS_SCREEN_ACCESS_GRANTED] =
    {
        .FirstLine  = "Access Granted",
        .SecondLine = "Welcome!"
    },

    [DRS_SCREEN_ACCESS_DENIED] =
    {
        .FirstLine  = "Access Denied!",
        .SecondLine = ""
    }
};

/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
static bool           DRS_IsScreenValid       (DRS_Screen_t Screen);
static bool           DRS_ViewNeedsRender     (const DRS_Handle_t* Instance);
static DRS_OpStatus_t DRS_RenderLockCharacters(DRS_Handle_t* Instance, uint8_t Start, uint8_t End);
static DRS_OpStatus_t DRS_ClearLockCharacters (DRS_Handle_t* Instance, uint8_t Start, uint8_t End);
static DRS_OpStatus_t DRS_RenderEntryDelta    (DRS_Handle_t* Instance);
static DRS_OpStatus_t DRS_RenderFullScreen    (DRS_Handle_t* Instance);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief Reports whether a screen identifier indexes a playable screen.
 */
static bool DRS_IsScreenValid(DRS_Screen_t Screen)
{
    return ((uint32_t)Screen < (uint32_t)DRS_SCREEN_COUNT);
}

/**
 * @brief Reports whether the physical LCD differs from the requested view.
 */
static bool DRS_ViewNeedsRender(const DRS_Handle_t* Instance)
{
    if(Instance->_requested_view.Screen != Instance->_rendered_view.Screen)
    {
        return true;
    }

    return (Instance->_requested_view.Screen == DRS_SCREEN_PASSWORD_ENTRY &&
            Instance->_requested_view.EnteredDigits != Instance->_rendered_view.EnteredDigits);
}

/**
 * @brief Writes the lock custom character over the half-open range [Start, End).
 */
static DRS_OpStatus_t DRS_RenderLockCharacters(DRS_Handle_t* Instance, uint8_t Start, uint8_t End)
{
    if(Start > End || End > DRS_ENTRY_DIGIT_CAPACITY)
    {
        return DRS_OPERATION_FAIL;
    }

    if(Start == End)
    {
        return DRS_OPERATION_OK;
    }

    if(LCD_SetCursor(Instance->_lcd, DRS_SECOND_ROW, Start) != LCD_OPERATION_OK)
    {
        return DRS_OPERATION_FAIL;
    }

    for(uint8_t position = Start; position < End; position++)
    {
        if(LCD_WriteCustomChar(Instance->_lcd, DRS_LOCK_CUSTOM_CHAR_POSITION) != LCD_OPERATION_OK)
        {
            return DRS_OPERATION_FAIL;
        }
    }

    return DRS_OPERATION_OK;
}

/**
 * @brief Overwrites lock characters with spaces over [Start, End).
 */
static DRS_OpStatus_t DRS_ClearLockCharacters(DRS_Handle_t* Instance, uint8_t Start, uint8_t End)
{
    if(Start > End || End > DRS_ENTRY_DIGIT_CAPACITY)
    {
        return DRS_OPERATION_FAIL;
    }

    if(Start == End)
    {
        return DRS_OPERATION_OK;
    }

    if(LCD_SetCursor(Instance->_lcd, DRS_SECOND_ROW, Start) != LCD_OPERATION_OK)
    {
        return DRS_OPERATION_FAIL;
    }

    for(uint8_t position = Start; position < End; position++)
    {
        if(LCD_WriteChar(Instance->_lcd, (uint8_t)' ') != LCD_OPERATION_OK)
        {
            return DRS_OPERATION_FAIL;
        }
    }

    return DRS_OPERATION_OK;
}

/**
 * @brief Applies only the changed password-mask positions.
 */
static DRS_OpStatus_t DRS_RenderEntryDelta(DRS_Handle_t* Instance)
{
    const uint8_t requested_digits = Instance->_requested_view.EnteredDigits;
    const uint8_t rendered_digits  = Instance->_rendered_view.EnteredDigits;

    if(requested_digits > rendered_digits)
    {
        return DRS_RenderLockCharacters(Instance, rendered_digits, requested_digits);
    }

    if(requested_digits < rendered_digits)
    {
        return DRS_ClearLockCharacters(Instance, requested_digits, rendered_digits);
    }

    return DRS_OPERATION_OK;
}

/**
 * @brief Clears and renders both lines of the requested semantic screen.
 */
static DRS_OpStatus_t DRS_RenderFullScreen(DRS_Handle_t* Instance)
{
    const DRS_ScreenContent_t* content =
        &DRS_ScreenContentMap[Instance->_requested_view.Screen];

    if(content->FirstLine == NULL || content->SecondLine == NULL)
    {
        return DRS_OPERATION_FAIL;
    }

    if(LCD_Clear(Instance->_lcd) != LCD_OPERATION_OK)
    {
        return DRS_OPERATION_FAIL;
    }

    if(LCD_PrintLine(Instance->_lcd, DRS_FIRST_ROW, content->FirstLine) != LCD_OPERATION_OK)
    {
        return DRS_OPERATION_FAIL;
    }

    if(LCD_PrintLine(Instance->_lcd, DRS_SECOND_ROW, content->SecondLine) != LCD_OPERATION_OK)
    {
        return DRS_OPERATION_FAIL;
    }

    if(Instance->_requested_view.Screen == DRS_SCREEN_PASSWORD_ENTRY)
    {
        if(DRS_RenderLockCharacters(Instance,
                                    DRS_FIRST_COLUMN,
                                    Instance->_requested_view.EnteredDigits) != DRS_OPERATION_OK)
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
 * @brief   Initializes one Display Render Service instance.
 *
 * @details Stores the caller-owned LCD reference, creates the lock character in
 *          CGRAM position zero and immediately renders the empty default password
 *          entry screen.
 *
 * @param   Instance - Pointer to caller-owned Display Render Service storage.
 * @param   LCD      - Pointer to a previously initialized LCD component.
 *
 * @return  DRS_OPERATION_OK when initialization and default rendering succeed;
 *          otherwise DRS_OPERATION_FAIL.
 */
DRS_OpStatus_t DRS_Init(DRS_Handle_t* Instance, LCD_Handle_t* LCD)
{
    if(Instance == NULL || LCD == NULL)
    {
        return DRS_OPERATION_FAIL;
    }

    Instance->_lcd                          = LCD;
    Instance->_requested_view.Screen        = DRS_SCREEN_PASSWORD_ENTRY;
    Instance->_requested_view.EnteredDigits = 0U;
    Instance->_rendered_view.Screen         = DRS_SCREEN_COUNT;
    Instance->_rendered_view.EnteredDigits  = 0U;
    Instance->_initialized                  = false;

    if(LCD_CreateChar(Instance->_lcd,
                      DRS_LOCK_CUSTOM_CHAR_POSITION,
                      DRS_LockCharacterBitmap) != LCD_OPERATION_OK)
    {
        Instance->_lcd = NULL;

        return DRS_OPERATION_FAIL;
    }

    Instance->_initialized = true;

    if(DRS_Update(Instance) != DRS_OPERATION_OK)
    {
        Instance->_initialized = false;
        Instance->_lcd         = NULL;

        return DRS_OPERATION_FAIL;
    }

    return DRS_OPERATION_OK;
}

/**
 * @brief   Selects the semantic screen requested by the application.
 *
 * @details This operation changes only logical service state. DRS_Update()
 *          performs the corresponding LCD transactions.
 *
 * @param   Instance - Pointer to an initialized Display Render Service instance.
 * @param   Screen   - Semantic screen to request.
 *
 * @return  DRS_OPERATION_OK when the request is stored; otherwise
 *          DRS_OPERATION_FAIL.
 */
DRS_OpStatus_t DRS_SetScreen(DRS_Handle_t* Instance, DRS_Screen_t Screen)
{
    if(Instance == NULL || !Instance->_initialized || !DRS_IsScreenValid(Screen))
    {
        return DRS_OPERATION_FAIL;
    }

    Instance->_requested_view.Screen = Screen;

    return DRS_OPERATION_OK;
}

/**
 * @brief   Updates the requested number of masked credential digits.
 *
 * @details The service stores only the count. It never receives, displays, or
 *          retains raw credential digit values. A later DRS_Update() writes or
 *          removes only the lock characters required to match this count.
 *
 * @param   Instance     - Pointer to an initialized Display Render Service instance.
 * @param   EnteredDigits - Number of accepted digits, from 0 through
 *                          DRS_ENTRY_DIGIT_CAPACITY.
 *
 * @return  DRS_OPERATION_OK when the count is accepted; otherwise
 *          DRS_OPERATION_FAIL.
 */
DRS_OpStatus_t DRS_SetEnteredDigits(DRS_Handle_t* Instance, uint8_t EnteredDigits)
{
    if(Instance == NULL || !Instance->_initialized ||
       EnteredDigits > DRS_ENTRY_DIGIT_CAPACITY)
    {
        return DRS_OPERATION_FAIL;
    }

    Instance->_requested_view.EnteredDigits = EnteredDigits;

    return DRS_OPERATION_OK;
}

/**
 * @brief   Makes the physical LCD match the most recently requested view.
 *
 * @details Unchanged views produce no LCD transactions. Screen changes perform
 *          one complete bounded render. Entry-length changes while the password
 *          screen remains active update only the changed positions on line two.
 *
 *          The rendered view is committed only after every required LCD operation
 *          succeeds. A failed partial render therefore remains pending and can be
 *          retried by a subsequent DRS_Update() call.
 *
 * @param   Instance - Pointer to an initialized Display Render Service instance.
 *
 * @return  DRS_OPERATION_OK when the display is already current or rendering
 *          succeeds; otherwise DRS_OPERATION_FAIL.
 */
DRS_OpStatus_t DRS_Update(DRS_Handle_t* Instance)
{
    if(Instance == NULL || !Instance->_initialized || Instance->_lcd == NULL)
    {
        return DRS_OPERATION_FAIL;
    }

    if(!DRS_ViewNeedsRender(Instance))
    {
        return DRS_OPERATION_OK;
    }

    if(Instance->_requested_view.Screen != Instance->_rendered_view.Screen)
    {
        if(DRS_RenderFullScreen(Instance) != DRS_OPERATION_OK)
        {
            return DRS_OPERATION_FAIL;
        }

        Instance->_rendered_view = Instance->_requested_view;

        return DRS_OPERATION_OK;
    }

    if(DRS_RenderEntryDelta(Instance) != DRS_OPERATION_OK)
    {
        return DRS_OPERATION_FAIL;
    }

    Instance->_rendered_view.EnteredDigits = Instance->_requested_view.EnteredDigits;

    return DRS_OPERATION_OK;
}
