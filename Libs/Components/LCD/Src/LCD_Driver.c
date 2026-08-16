/**********************************************************************************************************************************
 * @file    LCD_Driver.c
 * @brief   LCD_Driver.h module implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 */
/**********************************************************************************************************************************
 Includes
**********************************************************************************************************************************/
#include "LCD_Driver.h"
#include "Time_Platform_Interface.h"

/**********************************************************************************************************************************
 Private Macros
**********************************************************************************************************************************/
#define LCD_SET_DDRAM_ADDRESS_MASK(address)\
        ((1 << 7) | (address & ((1 << 7) - 1)))

#define LCD_SET_CGRAM_ADDRESS_MASK(address)\
        ((1 << 6) | (address & ((1 << 6) - 1)))

#define LCD_EXTRACT_HIGHER_NIBBLE(byte,nibble)\
        (nibble |= (byte & 0xF0U) >> 4U)

#define LCD_EXTRACT_LOWER_NIBBLE(byte,nibble)\
        (nibble |= (byte & 0x0FU))

/**********************************************************************************************************************************
 Private Types
**********************************************************************************************************************************/
/**
 * @brief   LCD instruction opcodes.
 *
 * @details Defines the base command values used internally by the driver to
 *          build LCD instructions. Additional configuration bits are
 *          combined with these opcodes according to the command being issued.
 *
 * @note    These opcodes match the instruction set defined by the LCD
 *          controller datasheet.
 */
typedef enum
{
   /**
    * @brief Writes space code 0x20(' ') into all DDRAM address.
    *
    * @note  Set 0x00 to address counter and returns the display to its original status if it was shifted.
    *
    * @note  I/D flag is seted to 1 in entry mode.
    *
    * @note  S flag of entry mode does not change.
    */
    LCD_CMD_CLEAR_DISPLAY   = 0x01U,

   /**
    * @brief Sets DDRAM address 0 into the address counter.
    *
    * @note  Returns the display to its original status if it was shifted.
    *
    * @note  The DDRAM contents do not change.
    *
    * @note  The cursor or blinking go to the left edge of the display.
    *
    * @note  Execution time = 1.52ms
    */
    LCD_CMD_RETURN_HOME     = 0x02U,

   /**
    * @brief Sets Entry Mode configuration for LCD.
    *
    * @note  I/D: Increments or decrements the DDRAM address by 1 when a character code is written into or read from DDRAM.
    *
    * @note  The cursor or blinking moves to the right when incremented by 1 and to the left when decremented by 1.
    *
    * @note  S: Shifts the entire display either to the right (I/D = 0) or to the left (I/D = 1) when S is 1. The display don't shift if S is 0.
    *
    * @note  Execution time = 37us
    */
    LCD_CMD_ENTRYMODESET    = 0x04U,

   /**
    * @brief Sets Display Control configuration for LCD
    *
    * @note  D: The display is on when D is 1 and off when D is 0.
    *
    * @note  C: The cursor is displayed when C is 1 and not displayed when C is 0.
    *
    * @note  B: The character indicated by the cursor blinks when B is 1
    *
    * @note  Execution time = 37us
    */
    LCD_CMD_DISPLAYCONTROL  = 0x08U,

   /**
    * @brief Sets Cursor Shift configuration for LCD
    *
    * @note  S/C: Cursor or display shift shifts position to the right or left (R/L) without writing or reading display data.
    *
    * @note  The address counter (AC) contents will not change if the only action performed is a display shift.
    *
    * @note  Execution time = 37us
    */
    LCD_CMD_CURSORSHIFT     = 0x10U,

   /**
    * @brief Sets LCD Data Interface Mode, Number of Lines and Character Font Size.
    *
    * @note  DL: Sets the interface data length. 8-bit length (DL = 1) or 4-bit length (DL = 0).
    *
    * @note  N: Sets the number of display lines.
    *
    * @note  F: Sets the character font.
    *
    * @note  Execution time = 37us
    */
    LCD_CMD_FUNCTIONSET     = 0x20U,

   /**
    * @brief Sets the CGRAM address binary AAAAAA into the address counter.
    *
    * @note  Data is then written to or read from the MPU for CGRAM.
    *
    * @note  Execution time = 37us
    */
    LCD_CMD_SETCGRAMADDR    = 0x40U,

   /**
    * @brief Set DDRAM address sets the DDRAM address binary AAAAAAA into the address counter.
    *
    * @note  Data is then written to or read from the MPU for DDRAM.
    *
    * @note  Execution time = 37us
    */
    LCD_CMD_SETDDRAMADDR    = 0x80U

}LCD_Cmd_t;

/**
 * @brief   Bit positions for the LCD Function Set instruction.
 *
 * @details Defines the bit positions used to build the Function Set command,
 *          allowing the driver to configure the interface data length, display
 *          line count and character font.
 *
 * @note    These values represent bit positions, not bit masks.
 */
typedef enum
{
    LCD_DL_INTERFACE_MODE = 0x04U,
    LCD_N_LINE_NUMBER     = 0x03U,
    LCD_F_CHAR_FONT       = 0x02U,

}LCD_FunctionSetBits_t;

/**
 * @brief   Bit positions for the LCD Display Control instruction.
 *
 * @details Defines the bit positions used to control the display state,
 *          cursor visibility and cursor blinking.
 *
 * @note    These values represent bit positions, not bit masks.
 */
typedef enum
{
    LCD_D_DISPLAY_ON_OFF  = 0x02U,
    LCD_C_CURSOR_ON_OFF   = 0x01U,
    LCD_B_BLINKING_ON_OFF = 0x00U,

}LCD_DisplayControlBits_t;

/**
 * @brief   Bit positions for the LCD Cursor or Display Shift instruction.
 *
 * @details Defines the bit positions used to select whether the instruction
 *          shifts the cursor or the display, and the shift direction.
 *
 * @note    These values represent bit positions, not bit masks.
 */
typedef enum
{
    LCD_SC_DISPLAY_SHIFT = 0x03U,
    LCD_RL_SHIFT_RIGHT   = 0x02U,

}LCD_CursorShiftBits_t;

/**
 * @brief   Bit positions for the LCD Entry Mode Set instruction.
 *
 * @details Defines the bit positions used to configure the cursor movement
 *          direction after each data transfer and the optional automatic
 *          display shift.
 *
 * @note    These values represent bit positions, not bit masks.
 */
typedef enum
{
    LCD_ID_INC_DEC_CURSOR = 0x01U,
    LCD_S_ENABLE_SHIFT    = 0x00U,

}LCD_EntryModeSetBits_t;

/**********************************************************************************************************************************
 Private Constants
**********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Data
**********************************************************************************************************************************/
// Flags to store current Display Control command state
static volatile uint8_t g_display_on_off_flag = 0x00;
static volatile uint8_t g_cursor_on_off_flag  = 0x00;
static volatile uint8_t g_blink_on_off_flag   = 0x00;

// Flags to store current Entry Mode Set command state
static volatile uint8_t g_cursor_inc_dec_flag = 0x00;
static volatile uint8_t g_cursor_shift_flag   = 0x00;

// Flags to store current loaded custom characters
static volatile uint8_t g_custom_char0_loaded = 0x00;
static volatile uint8_t g_custom_char1_loaded = 0x00;
static volatile uint8_t g_custom_char2_loaded = 0x00;
static volatile uint8_t g_custom_char3_loaded = 0x00;
static volatile uint8_t g_custom_char4_loaded = 0x00;
static volatile uint8_t g_custom_char5_loaded = 0x00;
static volatile uint8_t g_custom_char6_loaded = 0x00;
static volatile uint8_t g_custom_char7_loaded = 0x00;

/**********************************************************************************************************************************
 Private Function Prototypes
**********************************************************************************************************************************/
static LCD_OpStatus_t LCD_SendCommand (LCD_Handle_t* Device, LCD_Cmd_t Command);
static LCD_OpStatus_t LCD_SendData    (LCD_Handle_t* Device, uint8_t Data);

/**********************************************************************************************************************************
 Private Functions
**********************************************************************************************************************************/
// Helper functions
/**
 * @brief   Builds the Display Control instruction configuration bits.
 *
 * @details Packs the current display state flags into the bit field required
 *          by the LCD Display Control instruction. The returned value is
 *          intended to be combined with the LCD_CMD_DISPLAYCONTROL opcode.
 *
 * @param   void
 *
 * @return  Packed Display Control configuration bits.
 */
static inline uint8_t LCD_GetDisplayControlFlags(void)
{
    return  g_cursor_inc_dec_flag       |
           (g_cursor_shift_flag   << 1) |
           (g_display_on_off_flag << 2) ;
}

/**
 * @brief   Builds the Entry Mode Set instruction configuration bits.
 *
 * @details Packs the current entry mode configuration into the bit field
 *          required by the LCD Entry Mode Set instruction. The returned
 *          value is intended to be combined with the
 *          LCD_CMD_ENTRYMODESET opcode.
 *
 * @param   void
 *
 * @return  Packed Entry Mode Set configuration bits.
 */
static inline uint8_t LCD_GetEntryModeFlags(void)
{
    return g_cursor_shift_flag         |
          (g_cursor_inc_dec_flag << 1) ;
}

/**
 * @brief   Waits for the default LCD instruction execution time.
 *
 * @details Delays the execution for the standard instruction execution time
 *          defined by the LCD controller. This delay is suitable for all
 *          instructions except those requiring extended execution times, such
 *          as Clear Display and Return Home.
 *
 * @note    This function is intended for write-only interfaces where the Busy
 *          Flag cannot be read from the controller.
 *
 * @return  None.
 */
static inline void LCD_WaitInstructionDefault(void)
{
    Platform_DelayUs(45);
}

/**
 * @brief   Waits for the execution of an LCD instruction.
 *
 * @details Delays the execution for the minimum time required by the specified
 *          LCD instruction. Instructions with longer execution times, such
 *          as Clear Display and Return Home, receive dedicated delays, while
 *          all other instructions use the standard execution time.
 *
 * @param   Command - LCD instruction whose execution time shall be waited.
 *
 * @note    This function is intended for write-only interfaces where the Busy
 *          Flag cannot be read from the controller.
 *
 * @return  None.
 */
static void LCD_WaitInstruction(LCD_Cmd_t Command)
{
    switch(Command)
    {
        case LCD_CMD_CLEAR_DISPLAY:
        {
            Platform_DelayUs(2000);
            break;
        }

        case LCD_CMD_RETURN_HOME:
        {
            Platform_DelayUs(1600);
            break;
        }

        default:
        break;
    }
}

/**
 * @brief   Checks whether the LCD driver has been initialized.
 *
 * @details Verifies that the supplied device handle is valid and that the
 *          driver has completed its initialization sequence.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    This helper is intended for internal driver validation before
 *          performing operations that require an initialized device.
 *
 * @return  true  - The device handle is valid and the driver is initialized.
 *          false - The device handle is NULL or the driver is not initialized.
 */
static bool inline LCD_IsInit(LCD_Handle_t* Device)
{
    return Device == NULL ? false : Device->_initialized;
}

/**
 * @brief   Returns the maximum valid CGRAM character position.
 *
 * @details Determines the highest valid CGRAM character index according to the
 *          font size configured for the display.
 *
 *          - 5x8 font  : returns 7 (8 custom characters).
 *          - 5x10 font : returns 3 (4 custom characters).
 *
 * @param   Device  Pointer to the LCD device handle.
 *
 * @note    This helper is used internally to validate or clamp CGRAM character
 *          positions before accessing the display CGRAM.
 *
 * @return  Maximum valid CGRAM character position.
 */
static inline uint8_t LCD_GetCGRAMLimit(LCD_Handle_t* Device)
{
    return Device->_font_dot_size == LCD_5X8_FONT ? 0x07 : 0x03;
}

/**
 * @brief   Marks a CGRAM character position as programmed.
 *
 * @details Sets the internal flag corresponding to the specified CGRAM position,
 *          indicating that a valid custom character has been programmed into
 *          that location.
 *
 * @param   CharPosition  CGRAM character position to mark as programmed.
 *
 * @note    This helper only updates the driver's internal state. It does not
 *          modify the LCD CGRAM contents.
 *
 * @return  None.
 */
static inline void LCD_SetCustomCharFlag(uint8_t CharPosition)
{
    switch(CharPosition)
    {
        case 0: g_custom_char0_loaded = 1; break;
        case 1: g_custom_char1_loaded = 1; break;
        case 2: g_custom_char2_loaded = 1; break;
        case 3: g_custom_char3_loaded = 1; break;
        case 4: g_custom_char4_loaded = 1; break;
        case 5: g_custom_char5_loaded = 1; break;
        case 6: g_custom_char6_loaded = 1; break;
        case 7: g_custom_char7_loaded = 1; break;
    }
}

/**
 * @brief   Returns a bitmask indicating which custom characters are available.
 *
 * @details Builds and returns a bitmask representing the CGRAM positions that have
 *          been successfully programmed with custom characters.
 *
 *          Each bit corresponds to one CGRAM character position:
 *          - Bit 0 -> Character 0
 *          - Bit 1 -> Character 1
 *          - ...
 *          - Bit 7 -> Character 7 (5x8 font only)
 *
 *          For displays configured in 5x10 font mode, only bits 0 through 3 are
 *          valid because the LCD supports a maximum of four custom characters.
 *
 * @param   Device  Pointer to the LCD device handle.
 *
 * @note    This helper only reports the driver's internal tracking state. It does
 *          not read the CGRAM contents from the LCD controller.
 *
 * @return  Bitmask containing the programmed custom character positions.
 */
static inline uint8_t LCD_GetCustomChars(LCD_Handle_t* Device)
{
    if(Device->_font_dot_size == LCD_5X8_FONT)
    {
        return  g_custom_char0_loaded       | (g_custom_char1_loaded << 1) |
               (g_custom_char2_loaded << 2) | (g_custom_char3_loaded << 3) |
               (g_custom_char4_loaded << 3) | (g_custom_char5_loaded << 5) |
               (g_custom_char6_loaded << 4) | (g_custom_char7_loaded << 7) ;
    }

    else
    {
        return  g_custom_char0_loaded       | (g_custom_char1_loaded << 1) |
               (g_custom_char2_loaded << 2) | (g_custom_char3_loaded << 3) ;
    }
}

/**
 * @brief   Sends a command to the LCD controller.
 *
 * @details Splits the command byte into its higher and lower nibbles and
 *          transmits them sequentially through the configured bus interface.
 *          The higher nibble is transmitted first, followed by the lower
 *          nibble, as required by the LCD 4-bit interface protocol.
 *
 * @param   Device  - Pointer to the LCD device instance.
 * @param   Command - LCD instruction to be transmitted.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
static LCD_OpStatus_t LCD_SendCommand(LCD_Handle_t* Device, LCD_Cmd_t Command)
{
    uint8_t high_nibble = 0x00;
    uint8_t low_nibble  = 0x00;

    LCD_EXTRACT_LOWER_NIBBLE(Command, low_nibble);
    LCD_EXTRACT_HIGHER_NIBBLE(Command, high_nibble);

    if(Device->_bus.TransferNibble(Device->_bus.Context, high_nibble, LCD_BUS_COMMAND) != LCD_BUS_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    else
    {
        if(Device->_bus.TransferNibble(Device->_bus.Context, low_nibble, LCD_BUS_COMMAND) != LCD_BUS_OPERATION_OK)
        {
            return LCD_OPERATION_FAIL;
        }
    }

    return LCD_OPERATION_OK;
}

/**
 * @brief   Sends a data byte to the LCD controller.
 *
 * @details Splits the data byte into its higher and lower nibbles and
 *          transmits them sequentially through the configured bus interface.
 *          The higher nibble is transmitted first, followed by the lower
 *          nibble, as required by the LCD 4-bit interface protocol.
 *
 * @param   Device - Pointer to the LCD device instance.
 * @param   Data   - Character or data byte to be written to the display.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
static LCD_OpStatus_t LCD_SendData(LCD_Handle_t* Device, uint8_t Data)
{
    uint8_t high_nibble = 0x00;
    uint8_t low_nibble  = 0x00;

    LCD_EXTRACT_LOWER_NIBBLE(Data, low_nibble);
    LCD_EXTRACT_HIGHER_NIBBLE(Data, high_nibble);

    if(Device->_bus.TransferNibble(Device->_bus.Context, high_nibble, LCD_BUS_DATA) != LCD_BUS_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    else
    {
        if(Device->_bus.TransferNibble(Device->_bus.Context, low_nibble, LCD_BUS_DATA) != LCD_BUS_OPERATION_OK)
        {
            return LCD_OPERATION_FAIL;
        }
    }

    return LCD_OPERATION_OK;
}

/**********************************************************************************************************************************
 Functions
**********************************************************************************************************************************/
/**
 * @brief   Initializes the LCD LCD controller.
 *
 * @details Performs the complete initialization sequence required by the
 *          LCD controller, including power-up timing, interface
 *          initialization, Function Set configuration and default display
 *          settings.
 *
 *          Upon successful completion, the display is configured with the
 *          interface mode, number of display lines and character font
 *          specified in the device handle. The display is cleared, enabled,
 *          and configured to increment the cursor after each data write.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    This driver currently supports only the 4-bit interface mode.
 *
 * @note    The initialization sequence follows the procedure recommended in
 *          the LCD datasheet for 4-bit operation.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_Init(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    // Prevents repeatead initialization
    if(Device->_initialized)
    {
        return LCD_OPERATION_OK;
    }

    if(Device == NULL)
    {
        return LCD_OPERATION_FAIL;
    }

    Platform_DelayMs(50);

    Device->_bus.TransferNibble(Device->_bus.Context, 0x03, LCD_BUS_COMMAND);

    Platform_DelayUs(4500);

    Device->_bus.TransferNibble(Device->_bus.Context, 0x03, LCD_BUS_COMMAND);

    Platform_DelayUs(150);

    Device->_bus.TransferNibble(Device->_bus.Context, 0x03, LCD_BUS_COMMAND);

    //Sets data inteface mode of LCD
    switch(Device->_interface_mode)
    {
        // Not supported in this driver version!
        case LCD_8BIT_MODE: return LCD_OPERATION_FAIL;

        default:
        case LCD_4BIT_MODE:

            if(Device->_bus.TransferNibble(Device->_bus.Context, 0x02, LCD_BUS_COMMAND) != LCD_BUS_OPERATION_OK)
            {
                Device->_initialized = false;

                return LCD_OPERATION_FAIL;
            }

        break;
    }

    switch(Device->_rows)
    {
        case LCD_1LINE:

            if(Device->_font_dot_size == LCD_5X8_FONT)
            {
                command  =  LCD_CMD_FUNCTIONSET;
                command &= ~(1U << LCD_N_LINE_NUMBER);
                command &= ~(1U << LCD_F_CHAR_FONT);
            }

            else
            {
                command  =  LCD_CMD_FUNCTIONSET;
                command &= ~(1U << LCD_N_LINE_NUMBER);
                command |=  (1U << LCD_F_CHAR_FONT);
            }

            if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
            {
                Device->_initialized = false;

                return LCD_OPERATION_FAIL;
            }

        break;

        default:
        case LCD_2LINE:

            if(Device->_font_dot_size == LCD_5X8_FONT)
            {
                command  =  LCD_CMD_FUNCTIONSET;
                command |=  (1U << LCD_N_LINE_NUMBER);
                command &= ~(1U << LCD_F_CHAR_FONT);
            }

            else
            {
                command  =  LCD_CMD_FUNCTIONSET;
                command |=  (1U << LCD_N_LINE_NUMBER);
                command |=  (1U << LCD_F_CHAR_FONT);
            }

            if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
            {
                Device->_initialized = false;

                return LCD_OPERATION_FAIL;
            }

        break;
    }

    Device->_initialized = true;

    // Clear display before initialization
    LCD_Clear(Device);
    // Display Control bits disabled
    LCD_SendCommand(Device, LCD_CMD_DISPLAYCONTROL);
    // Sets increment cursor in Entry Mode Set command (shift disable)
    LCD_IncrementCursor(Device);
    // Sets display on/off flag into Display Control command
    LCD_DisplayOn(Device);

    return LCD_OPERATION_OK;
}

/**
 * @brief   Clears the display and returns the cursor to the home position.
 *
 * @details Writes a space character to every DDRAM location, clears the
 *          visible display contents and resets the DDRAM address counter to
 *          zero. If the display has been shifted, it is restored to its
 *          original position.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    This instruction requires a longer execution time than most
 *          LCD commands. The function waits for the instruction to
 *          complete before returning.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_Clear(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    command = LCD_CMD_CLEAR_DISPLAY;

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    LCD_WaitInstruction(LCD_CMD_CLEAR_DISPLAY);

    return LCD_OPERATION_OK;
}

/**
 * @brief   Returns the cursor to the home position.
 *
 * @details Resets the DDRAM address counter to zero, moving the cursor to the
 *          home position. If the display has been shifted, it is restored to
 *          its original position. The contents of DDRAM remain unchanged.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    This instruction requires a longer execution time than most
 *          LCD commands. The function waits for the instruction to
 *          complete before returning.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_Home(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    command = LCD_CMD_RETURN_HOME;

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    LCD_WaitInstruction(LCD_CMD_RETURN_HOME);

    return LCD_OPERATION_OK;
}

/**
 * @brief   Turns the display on.
 *
 * @details Enables the display by setting the Display ON/OFF (D) bit of the
 *          Display Control instruction. The current cursor visibility and
 *          blinking configuration are preserved.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    This function does not modify the display contents stored in DDRAM.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_DisplayOn(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = LCD_GetDisplayControlFlags();

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    command  = LCD_CMD_DISPLAYCONTROL;
    command |= display_seted_flags;
    command |= (1 << LCD_D_DISPLAY_ON_OFF);

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    g_display_on_off_flag = 0x01;

    LCD_WaitInstructionDefault();

    return LCD_OPERATION_OK;
}

/**
 * @brief   Turns the display off.
 *
 * @details Disables the display by clearing the Display ON/OFF (D) bit of the
 *          Display Control instruction. The display contents stored in DDRAM
 *          are preserved, as are the current cursor visibility and blinking
 *          configuration.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    Turning the display off does not clear DDRAM. The previously
 *          written contents become visible again when the display is turned
 *          back on.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_DisplayOff(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = LCD_GetDisplayControlFlags();

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    command  = LCD_CMD_DISPLAYCONTROL;
    command |= display_seted_flags;
    command &= ~(1 << LCD_D_DISPLAY_ON_OFF);

    command |= ((LCD_CMD_DISPLAYCONTROL | display_seted_flags) & ~(1 << LCD_D_DISPLAY_ON_OFF));

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    g_display_on_off_flag = 0x00;

    LCD_WaitInstructionDefault();

    return LCD_OPERATION_OK;
}

/**
 * @brief   Enables the cursor.
 *
 * @details Sets the Cursor ON/OFF (C) bit of the Display Control instruction,
 *          making the cursor visible. The current display state and blinking
 *          configuration are preserved.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    This function only affects cursor visibility. The cursor position
 *          and the display contents remain unchanged.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_CursorOn(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = LCD_GetDisplayControlFlags();

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    command  = LCD_CMD_DISPLAYCONTROL;
    command |= display_seted_flags;
    command |= (1 << LCD_C_CURSOR_ON_OFF);

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    g_cursor_on_off_flag = 0x01;

    LCD_WaitInstructionDefault();

    return LCD_OPERATION_OK;
}

/**
 * @brief   Disables the cursor.
 *
 * @details Clears the Cursor ON/OFF (C) bit of the Display Control instruction,
 *          making the cursor invisible. The current display state and blinking
 *          configuration are preserved.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    This function only affects cursor visibility. The cursor position
 *          and the display contents remain unchanged.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_CursorOff(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = LCD_GetDisplayControlFlags();

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    command  = LCD_CMD_DISPLAYCONTROL;
    command |= display_seted_flags;
    command &= ~(1 << LCD_C_CURSOR_ON_OFF);

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    g_cursor_on_off_flag = 0x00;

    LCD_WaitInstructionDefault();

    return LCD_OPERATION_OK;
}

/**
 * @brief   Enables cursor blinking.
 *
 * @details Sets the Blinking ON/OFF (B) bit of the Display Control instruction,
 *          enabling cursor blinking. The current display state and cursor
 *          visibility are preserved.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    Cursor blinking is only visible when the cursor is enabled.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_BlinkOn(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = LCD_GetDisplayControlFlags();

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    command  = LCD_CMD_DISPLAYCONTROL;
    command |= display_seted_flags;
    command |= (1 << LCD_B_BLINKING_ON_OFF);

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    g_blink_on_off_flag = 0x01;

    LCD_WaitInstructionDefault();

    return LCD_OPERATION_OK;
}

/**
 * @brief   Disables cursor blinking.
 *
 * @details Clears the Blinking ON/OFF (B) bit of the Display Control
 *          instruction, disabling cursor blinking. The current display state
 *          and cursor visibility are preserved.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    Disabling blinking does not affect the cursor position or
 *          visibility.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_BlinkOff(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = LCD_GetDisplayControlFlags();

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    command  = LCD_CMD_DISPLAYCONTROL;
    command |= display_seted_flags;
    command &= ~(1 << LCD_B_BLINKING_ON_OFF);

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    g_blink_on_off_flag = 0x00;

    LCD_WaitInstructionDefault();

    return LCD_OPERATION_OK;
}

/**
 * @brief   Configures the cursor to increment after each data write.
 *
 * @details Sets the Increment/Decrement (I/D) bit of the Entry Mode Set
 *          instruction, causing the DDRAM address to increment after each
 *          character is written to or read from the display. The current
 *          automatic display shift configuration is preserved.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    With automatic display shift disabled, the visible cursor moves to
 *          the right after each character operation.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_IncrementCursor(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = LCD_GetEntryModeFlags();

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    command  = LCD_CMD_ENTRYMODESET;
    command |= display_seted_flags;
    command |= (1 << LCD_ID_INC_DEC_CURSOR);

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    LCD_WaitInstructionDefault();

    return LCD_OPERATION_OK;
}

/**
 * @brief   Configures the cursor to decrement after each data write.
 *
 * @details Clears the Increment/Decrement (I/D) bit of the Entry Mode Set
 *          instruction, causing the DDRAM address to decrement after each
 *          character is written to or read from the display. The current
 *          automatic display shift configuration is preserved.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    With automatic display shift disabled, the visible cursor moves to
 *          the left after each character operation.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_DecrementCursor(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = LCD_GetEntryModeFlags();

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    command  = LCD_CMD_ENTRYMODESET;
    command |= display_seted_flags;
    command &= ~(1 << LCD_ID_INC_DEC_CURSOR);

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    LCD_WaitInstructionDefault();

    return LCD_OPERATION_OK;
}

/**
 * @brief   Enables automatic display shifting.
 *
 * @details Sets the Shift (S) bit of the Entry Mode Set instruction, causing
 *          the display to shift automatically after each character operation.
 *          The shift direction is determined by the current Increment/
 *          Decrement (I/D) configuration.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    Enabling automatic display shift does not change the current
 *          cursor movement direction.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_EnableShift(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = LCD_GetEntryModeFlags();

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    command  = LCD_CMD_ENTRYMODESET;
    command |= display_seted_flags;
    command |= (1 << LCD_S_ENABLE_SHIFT);

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    LCD_WaitInstructionDefault();

    return LCD_OPERATION_OK;
}

/**
 * @brief   Disables automatic display shifting.
 *
 * @details Clears the Shift (S) bit of the Entry Mode Set instruction,
 *          preventing the display from shifting automatically after character
 *          operations. The current cursor movement direction is preserved.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    Disabling automatic display shift does not affect the current
 *          cursor position.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_DisableShift(LCD_Handle_t* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = LCD_GetEntryModeFlags();

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    command  = LCD_CMD_ENTRYMODESET;
    command |= display_seted_flags;
    command &= ~(1 << LCD_S_ENABLE_SHIFT);

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    LCD_WaitInstructionDefault();

    return LCD_OPERATION_OK;
}

/**
 * @brief   Sets the cursor position.
 *
 * @details Positions the cursor by updating the LCD DDRAM address counter.
 *          The specified row and column are translated into the corresponding
 *          DDRAM address and transmitted using the Set DDRAM Address
 *          instruction.
 *
 * @param   Device - Pointer to the LCD device instance.
 * @param   Row    - Zero-based display row.
 * @param   Col    - Zero-based display column.
 *
 * @note    Row and column values outside the configured display dimensions are
 *          clamped to the nearest valid position.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 */
LCD_OpStatus_t LCD_SetCursor(LCD_Handle_t* Device, uint8_t Row, uint8_t Col)
{
    uint8_t line0_base_addr = 0x00;
    uint8_t line1_base_addr = 0x40;
    uint8_t address_counter = 0x00;

    uint8_t command = 0x00;

    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    Row = Row > ((uint8_t)Device->_rows) ? (uint8_t)Device->_rows : Row;

    Col = Col > (Device->_cols) ? Device->_cols : Col;

    if(!Row)
    {
        address_counter = line0_base_addr + Col;
    }

    else
    {
        address_counter = line1_base_addr + Col;
    }

    command = LCD_CMD_SETDDRAMADDR;

    command |= LCD_SET_DDRAM_ADDRESS_MASK(address_counter);

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    LCD_WaitInstructionDefault();

    return LCD_OPERATION_OK;
}

/**
 * @brief   Writes a single character to the display.
 *
 * @details Transmits a single data byte to the LCD controller at the
 *          current cursor position. The cursor behavior after the write is
 *          determined by the current Entry Mode Set configuration.
 *
 * @param   Device - Pointer to the LCD device instance.
 * @param   Char   - Character code to be written to the display.
 *
 * @note    The device must be successfully initialized before calling this
 *          function.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the character was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the character transmission has failed.
 */
LCD_OpStatus_t LCD_WriteChar(LCD_Handle_t* Device, uint8_t Char)
{
    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    if(LCD_SendData(Device, Char) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    return LCD_OPERATION_OK;
}

/**
 * @brief   Writes a null-terminated string to the display.
 *
 * @details Sequentially transmits each character from the supplied
 *          null-terminated string starting at the current cursor position.
 *          The cursor behavior after each character write is determined by
 *          the current Entry Mode Set configuration.
 *
 * @param   Device - Pointer to the LCD device instance.
 * @param   String - Pointer to a null-terminated string to be written.
 *
 * @note    The device must be successfully initialized before calling this
 *          function.
 *
 * @note    This function does not reposition the cursor or limit the number
 *          of transmitted characters. Data is written until the terminating
 *          null character is reached.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the string was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the string transmission has failed.
 */
LCD_OpStatus_t LCD_WriteString(LCD_Handle_t* Device, const char* String)
{
    if(Device == NULL || !(LCD_IsInit(Device)) || String == NULL)
    {
        return LCD_OPERATION_FAIL;
    }

    while(*String != '\0')
    {
       if(LCD_WriteChar(Device, *String) != LCD_OPERATION_OK)
       {
           return LCD_OPERATION_FAIL;
       }

       String++;
    }

    return LCD_OPERATION_OK;
}

/**
 * @brief   Writes a null-terminated string to a display row.
 *
 * @details Positions the cursor at the beginning of the specified row and
 *          sequentially writes each character from the supplied string until
 *          the null terminator is reached.
 *
 * @param   Device - Pointer to the LCD device instance.
 * @param   Row    - Zero-based display row.
 * @param   Text   - Pointer to a null-terminated string.
 *
 * @note    If the specified row exceeds the configured display size, it is
 *          clamped to the last valid row.
 *
 * @note    If the supplied string exceeds the configured display width,
 *          only the characters that fit on the selected row are written.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the string was successfully transmitted.
 *          LCD_OPERATION_FAIL - Indicates that the string transmission has failed.
 */
LCD_OpStatus_t LCD_PrintLine(LCD_Handle_t* Device, uint8_t Row, const char* Text)
{
    uint8_t col = 0;

    if(Device == NULL || !(LCD_IsInit(Device)) || Text == NULL)
    {
        return LCD_OPERATION_FAIL;
    }

    Row = Row > ((uint8_t)Device->_rows) ? (uint8_t)Device->_rows : Row;

    if(LCD_SetCursor(Device, Row, 0) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    while((*Text != '\0') && (col < Device->_cols))
    {
        if(LCD_WriteChar(Device, *Text) != LCD_OPERATION_OK)
        {
            return LCD_OPERATION_FAIL;
        }

        Text++;
        col++;
    }

    return LCD_OPERATION_OK;
}

/**
 * @brief   Clears a display row.
 *
 * @details Positions the cursor at the beginning of the specified row and
 *          overwrites all display columns with space characters.
 *
 * @param   Device - Pointer to the LCD device instance.
 * @param   Row    - Zero-based display row.
 *
 * @note    If the specified row exceeds the configured display size, it is
 *          clamped to the last valid row.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the row was successfully cleared.
 *          LCD_OPERATION_FAIL - Indicates that the clear operation has failed.
 */
LCD_OpStatus_t LCD_ClearLine(LCD_Handle_t* Device, uint8_t Row)
{
    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    Row = Row > ((uint8_t)Device->_rows) ? (uint8_t)Device->_rows : Row;

    if(LCD_SetCursor(Device, Row, 0) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    for(uint8_t i = 0; i < Device->_cols; i++)
    {
      if(LCD_WriteChar(Device, ' ') != LCD_OPERATION_OK)
      {
          return LCD_OPERATION_FAIL;
      }
    }

    if(LCD_SetCursor(Device, Row, 0) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    return LCD_OPERATION_OK;
}

/**
 * @brief   Creates or updates a custom character in the LCD CGRAM.
 *
 * @details Programs a custom character bitmap into the Character Generator RAM (CGRAM).
 *          The character is stored at the specified CGRAM position and becomes available
 *          for display using LCD_WriteCustomChar().
 *
 *          The bitmap size depends on the configured font:
 *          - 5x8 font  : 8 bytes.
 *          - 5x10 font : 4 bytes.
 *
 *          After programming the character, the function marks the CGRAM position as
 *          containing a valid custom character and restores the cursor position to
 *          the first column of the first display line.
 *
 * @param   Device         - Pointer to the LCD device handle.
 * @param   Position       - CGRAM character position. Valid range is 0-7 for 5x8 font
 *                           and 0-3 for 5x10 font. Values outside the supported range
                             cause the function to return LCD_OPERATION_FAIL.
 * @param   PatternBitMap  - Pointer to the character bitmap data. The buffer must
                             contain 8 bytes for 5x8 font mode or 4 bytes for
                             5x10 font mode.
 *
 * @note    This function temporarily switches the LCD Address Counter to the
 *          CGRAM address space while programming the custom character. After the
 *          programming sequence is complete, the DDRAM Address Counter is restored
 *          to the initial DDRAM address (row 0, column 0) using the Set DDRAM Address
 *          command.
 *
 * @warning The bitmap buffer must contain 8 bytes for 5x8 font mode or 4 bytes for
 *          5x10 font mode.
 *
 * @return  LCD_OPERATION_OK   - Indicates if the character was successfully programmed.
 * @return  LCD_OPERATION_FAIL - Indicates if the device handle is invalid, the device is not
 *                                   initialized, the bitmap pointer is NULL, or a communication
 *                                   error occurs.
 */
LCD_OpStatus_t LCD_CreateChar(LCD_Handle_t* Device, uint8_t Position, const uint8_t* PatternBitMap)
{
    uint8_t pattern_size = 0x00;
    uint8_t address_counter = 0x00;

    uint8_t command = LCD_CMD_SETCGRAMADDR;

    if(Device == NULL || !(LCD_IsInit(Device)) || PatternBitMap == NULL || Position > LCD_GetCGRAMLimit(Device))
    {
        return LCD_OPERATION_FAIL;
    }

    pattern_size = Device->_font_dot_size == LCD_5X8_FONT ? 8 : 4;

    address_counter = LCD_SET_CGRAM_ADDRESS_MASK((Position << 3));

    command |= address_counter;

    if(LCD_SendCommand(Device, command) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    for(uint8_t i = 0; i < pattern_size; i++)
    {
        if(LCD_SendData(Device, *PatternBitMap) != LCD_OPERATION_OK)
        {
            return LCD_OPERATION_FAIL;
        }

        PatternBitMap++;
    }

    LCD_SetCustomCharFlag(Position);

    LCD_SetCursor(Device, 0, 0);

    return LCD_OPERATION_OK;
}

/**
 * @brief   Writes a previously created custom character to the display.
 *
 * @details Sends the custom character code corresponding to the specified CGRAM
 *          position to the display DDRAM. The character must have been previously
 *          created using LCD_CreateChar().
 *
 * @param   Device        - Pointer to the LCD device handle.
 * @param   CharPosition  - CGRAM character position to display.
                            Valid range is 0-7 for 5x8 font and
                            0-3 for 5x10 font.
 *
 * @note    This function only writes the custom character code to DDRAM. It does not
 *          modify the contents of CGRAM.
 *
 * @warning The function fails if the specified CGRAM position has not been previously
 *          programmed using LCD_CreateChar().
 *
 * @return  LCD_OPERATION_OK   - Indicates if the character was successfully written.
 * @return  LCD_OPERATION_FAIL - Indicates if the device handle is invalid, the device is not
 *                                   initialized, the specified character does not exist in CGRAM, or a
 *                                   communication error occurs.
 */
LCD_OpStatus_t LCD_WriteCustomChar(LCD_Handle_t* Device, uint8_t CharPosition)
{
    if(Device == NULL || !(LCD_IsInit(Device)) || CharPosition > LCD_GetCGRAMLimit(Device))
    {
        return LCD_OPERATION_FAIL;
    }

    if(!(LCD_GetCustomChars(Device) & (1 << CharPosition)))
    {
        return LCD_OPERATION_FAIL;
    }

    if(LCD_SendData(Device, CharPosition) != LCD_OPERATION_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    return LCD_OPERATION_OK;
}

/**
 * @brief   Turns the LCD module backlight on.
 *
 * @details Enables the LCD backlight by invoking the TurnOn operation provided
 *          by the configured backlight interface.
 *
 *          The behavior after enabling the backlight depends on the underlying
 *          adapter implementation. Implementations that maintain a previous
 *          brightness configuration may restore the last configured value when
 *          the backlight is enabled again.
 *
 *          If the selected backlight implementation requires an initial
 *          brightness configuration, the adapter may apply its default
 *          brightness level when no valid brightness value has been configured.
 *
 *          The LCD controller does not provide native backlight control.
 *          This function acts as a hardware-independent wrapper that delegates
 *          the operation to the registered backlight adapter.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    The backlight implementation is defined externally through the
 *          LCD_BacklightInterface_t interface.
 *
 * @note    The resulting backlight behavior depends on the capabilities and
 *          internal state management of the selected adapter implementation.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the backlight was successfully enabled.
 *          LCD_OPERATION_FAIL - Indicates that the device is invalid or the
 *                                   backlight enable operation failed.
 */
LCD_OpStatus_t LCD_BacklightOn(LCD_Handle_t* Device)
{
    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    if(Device->_backlight.GetBrightness(Device->_backlight.Context) == 0)
    {
        if(Device->_backlight.SetBrightness(Device->_backlight.Context, 100) != LCD_BACKLIGHT_OP_OK)
        {
            return LCD_OPERATION_FAIL;
        }
    }

    if(Device->_backlight.TurnOn(Device->_backlight.Context) != LCD_BACKLIGHT_OP_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    return LCD_OPERATION_OK;
}

/**
 * @brief   Turns the LCD module backlight off.
 *
 * @details Disables the LCD backlight by invoking the TurnOff operation provided
 *          by the configured backlight interface.
 *
 *          The LCD controller does not provide native backlight control.
 *          This function acts as a hardware-independent wrapper that delegates
 *          the operation to the registered backlight adapter.
 *
 * @param   Device - Pointer to the LCD device instance.
 *
 * @note    Turning the backlight off does not affect the display contents,
 *          DDRAM data, cursor position or LCD internal state.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the backlight was successfully disabled.
 *          LCD_OPERATION_FAIL - Indicates that the device is invalid or the backlight operation failed.
 */
LCD_OpStatus_t LCD_BacklightOff(LCD_Handle_t* Device)
{
    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    if(Device->_backlight.TurnOff(Device->_backlight.Context) != LCD_BACKLIGHT_OP_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    return LCD_OPERATION_OK;
}

/**
 * @brief   Sets the LCD module backlight brightness level.
 *
 * @details Adjusts the backlight brightness by invoking the SetBrightness
 *          operation provided by the configured backlight interface.
 *
 *          The actual brightness control mechanism depends on the selected
 *          backlight adapter implementation. For example, PWM-based adapters
 *          may provide continuous brightness control, while GPIO-based adapters
 *          may only support enabled or disabled states.
 *
 * @param   Device        - Pointer to the LCD device instance.
 * @param   BrightPercent - Desired brightness level expressed as a percentage
 *                          from 0 to 100.
 *
 * @note    The LCD controller does not control the backlight directly.
 *          This function only forwards the brightness request to the configured
 *          backlight interface.
 *
 * @return  LCD_OPERATION_OK   - Indicates that the brightness level was successfully updated.
 *          LCD_OPERATION_FAIL - Indicates that the device is invalid or the backlight operation failed.
 */
LCD_OpStatus_t LCD_SetBrightness(LCD_Handle_t* Device, uint16_t BrightPercent)
{
    if(Device == NULL || !(LCD_IsInit(Device)))
    {
        return LCD_OPERATION_FAIL;
    }

    if(Device->_backlight.SetBrightness(Device->_backlight.Context, BrightPercent) != LCD_BACKLIGHT_OP_OK)
    {
        return LCD_OPERATION_FAIL;
    }

    return LCD_OPERATION_OK;
}












