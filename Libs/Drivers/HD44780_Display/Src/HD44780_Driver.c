/**********************************************************************************************************************************
 * @file    HD44780_Driver.c
 * @brief   HD44780_Driver.h module implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Jul 15, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "HD44780_Driver.h"
#include "Time_Platform_Interface.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
#define HD44780_SET_DDRAM_ADDRESS_MASK(address)\
        ((1 << 7) | (address & ((1 << 7) - 1)))

#define HD44780_SET_CGRAM_ADDRESS_MASK(address)\
        ((1 << 6) | (address & ((1 << 6) - 1)))

#define HD44780_EXTRACT_HIGHER_NIBBLE(byte,nibble)\
        (nibble |= (byte & 0xF0U) >> 4U)

#define HD44780_EXTRACT_LOWER_NIBBLE(byte,nibble)\
        (nibble |= (byte & 0x0FU))

/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   HD44780 instruction opcodes.
 *
 * @details Defines the base command values used internally by the driver to
 *          build HD44780 instructions. Additional configuration bits are
 *          combined with these opcodes according to the command being issued.
 *
 * @note    These opcodes match the instruction set defined by the HD44780
 *          controller datasheet.
 **********************************************************************************************************************************/
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
    HD44780_CMD_CLEAR_DISPLAY   = 0x01U,

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
    HD44780_CMD_RETURN_HOME     = 0x02U,

   /**
    * @brief Sets Entry Mode configuration for HD44780.
    *
    * @note  I/D: Increments or decrements the DDRAM address by 1 when a character code is written into or read from DDRAM.
    *
    * @note  The cursor or blinking moves to the right when incremented by 1 and to the left when decremented by 1.
    *
    * @note  S: Shifts the entire display either to the right (I/D = 0) or to the left (I/D = 1) when S is 1. The display don't shift if S is 0.
    *
    * @note  Execution time = 37us
    */
    HD44780_CMD_ENTRYMODESET    = 0x04U,

   /**
    * @brief Sets Display Control configuration for HD44780
    *
    * @note  D: The display is on when D is 1 and off when D is 0.
    *
    * @note  C: The cursor is displayed when C is 1 and not displayed when C is 0.
    *
    * @note  B: The character indicated by the cursor blinks when B is 1
    *
    * @note  Execution time = 37us
    */
    HD44780_CMD_DISPLAYCONTROL  = 0x08U,

   /**
    * @brief Sets Cursor Shift configuration for HD44780
    *
    * @note  S/C: Cursor or display shift shifts position to the right or left (R/L) without writing or reading display data.
    *
    * @note  The address counter (AC) contents will not change if the only action performed is a display shift.
    *
    * @note  Execution time = 37us
    */
    HD44780_CMD_CURSORSHIFT     = 0x10U,

   /**
    * @brief Sets HD44780 Data Interface Mode, Number of Lines and Character Font Size.
    *
    * @note  DL: Sets the interface data length. 8-bit length (DL = 1) or 4-bit length (DL = 0).
    *
    * @note  N: Sets the number of display lines.
    *
    * @note  F: Sets the character font.
    *
    * @note  Execution time = 37us
    */
    HD44780_CMD_FUNCTIONSET     = 0x20U,

   /**
    * @brief Sets the CGRAM address binary AAAAAA into the address counter.
    *
    * @note  Data is then written to or read from the MPU for CGRAM.
    *
    * @note  Execution time = 37us
    */
    HD44780_CMD_SETCGRAMADDR    = 0x40U,

   /**
    * @brief Set DDRAM address sets the DDRAM address binary AAAAAAA into the address counter.
    *
    * @note  Data is then written to or read from the MPU for DDRAM.
    *
    * @note  Execution time = 37us
    */
    HD44780_CMD_SETDDRAMADDR    = 0x80U

}HD44780_CmdTypeDef;

/**********************************************************************************************************************************
 * @brief   Bit positions for the HD44780 Function Set instruction.
 *
 * @details Defines the bit positions used to build the Function Set command,
 *          allowing the driver to configure the interface data length, display
 *          line count and character font.
 *
 * @note    These values represent bit positions, not bit masks.
 **********************************************************************************************************************************/
typedef enum
{
    HD44780_DL_INTERFACE_MODE = 0x04U,
    HD44780_N_LINE_NUMBER     = 0x03U,
    HD44780_F_CHAR_FONT       = 0x02U,

}HD44780_FunctionSetBitsTypeDef;

/**********************************************************************************************************************************
 * @brief   Bit positions for the HD44780 Display Control instruction.
 *
 * @details Defines the bit positions used to control the display state,
 *          cursor visibility and cursor blinking.
 *
 * @note    These values represent bit positions, not bit masks.
 **********************************************************************************************************************************/
typedef enum
{
    HD44780_D_DISPLAY_ON_OFF  = 0x02U,
    HD44780_C_CURSOR_ON_OFF   = 0x01U,
    HD44780_B_BLINKING_ON_OFF = 0x00U,

}HD44780_DisplayControlBitsTypeDef;

/**********************************************************************************************************************************
 * @brief   Bit positions for the HD44780 Cursor or Display Shift instruction.
 *
 * @details Defines the bit positions used to select whether the instruction
 *          shifts the cursor or the display, and the shift direction.
 *
 * @note    These values represent bit positions, not bit masks.
 **********************************************************************************************************************************/
typedef enum
{
    HD44780_SC_DISPLAY_SHIFT = 0x03U,
    HD44780_RL_SHIFT_RIGHT   = 0x02U,

}HD44780_CursorShiftBitsTypeDef;

/**********************************************************************************************************************************
 * @brief   Bit positions for the HD44780 Entry Mode Set instruction.
 *
 * @details Defines the bit positions used to configure the cursor movement
 *          direction after each data transfer and the optional automatic
 *          display shift.
 *
 * @note    These values represent bit positions, not bit masks.
 **********************************************************************************************************************************/
typedef enum
{
    HD44780_ID_INC_DEC_CURSOR = 0x01U,
    HD44780_S_ENABLE_SHIFT    = 0x00U,

}HD44780_EntryModeSetBitsTypeDef;

/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/* << Flags to store current Display Control command state >> */
static volatile uint8_t g_display_on_off_flag = 0x00;
static volatile uint8_t g_cursor_on_off_flag  = 0x00;
static volatile uint8_t g_blink_on_off_flag   = 0x00;

/* << Flags to store current Entry Mode Set command state >> */
static volatile uint8_t g_cursor_inc_dec_flag = 0x00;
static volatile uint8_t g_cursor_shift_flag = 0x00;

/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
static HD44780_OpStatusTypeDef HD44780_SendCommand(HD44780_HandleTypeDef* Device, HD44780_CmdTypeDef Command);
static HD44780_OpStatusTypeDef HD44780_SendData(HD44780_HandleTypeDef* Device, uint8_t Data);

/* << Helper functions >> */
/**********************************************************************************************************************************
 * @brief   Builds the Display Control instruction configuration bits.
 *
 * @return  Packed Display Control configuration bits.
 **********************************************************************************************************************************/
static inline uint8_t HD44780_GetDisplayControlFlags(void);

/**********************************************************************************************************************************
 * @brief   Builds the Entry Mode Set instruction configuration bits.
 *
 * @return  Packed Entry Mode Set configuration bits.
 **********************************************************************************************************************************/
static inline uint8_t HD44780_GetEntryModeFlags(void);

/**********************************************************************************************************************************
 * @brief   Waits for the default HD44780 instruction execution time.
 *
 * @return  None.
 **********************************************************************************************************************************/
static inline void HD44780_WaitInstructionDefault(void);

/**********************************************************************************************************************************
 * @brief   Waits for the execution of an HD44780 instruction.
 *
 * @param   Command - HD44780 instruction whose execution time shall be waited.
 *
 * @return  None.
 **********************************************************************************************************************************/
static void HD44780_WaitInstruction(HD44780_CmdTypeDef Command);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Builds the Display Control instruction configuration bits.
 *
 * @details Packs the current display state flags into the bit field required
 *          by the HD44780 Display Control instruction. The returned value is
 *          intended to be combined with the HD44780_CMD_DISPLAYCONTROL opcode.
 *
 * @param   void
 *
 * @return  Packed Display Control configuration bits.
 **********************************************************************************************************************************/
static inline uint8_t HD44780_GetDisplayControlFlags(void)
{
    return  g_cursor_inc_dec_flag       |
           (g_cursor_shift_flag << 1)   |
           (g_display_on_off_flag << 2) ;
}

/**********************************************************************************************************************************
 * @brief   Builds the Entry Mode Set instruction configuration bits.
 *
 * @details Packs the current entry mode configuration into the bit field
 *          required by the HD44780 Entry Mode Set instruction. The returned
 *          value is intended to be combined with the
 *          HD44780_CMD_ENTRYMODESET opcode.
 *
 * @param   void
 *
 * @return  Packed Entry Mode Set configuration bits.
 **********************************************************************************************************************************/
static inline uint8_t HD44780_GetEntryModeFlags(void)
{
    return g_cursor_shift_flag         |
          (g_cursor_inc_dec_flag << 1) ;
}

/**********************************************************************************************************************************
 * @brief   Waits for the default HD44780 instruction execution time.
 *
 * @details Delays the execution for the standard instruction execution time
 *          defined by the HD44780 controller. This delay is suitable for all
 *          instructions except those requiring extended execution times, such
 *          as Clear Display and Return Home.
 *
 * @note    This function is intended for write-only interfaces where the Busy
 *          Flag cannot be read from the controller.
 *
 * @return  None.
 **********************************************************************************************************************************/
static inline void HD44780_WaitInstructionDefault(void)
{
    Platform_DelayUs(45);
}

/**********************************************************************************************************************************
 * @brief   Waits for the execution of an HD44780 instruction.
 *
 * @details Delays the execution for the minimum time required by the specified
 *          HD44780 instruction. Instructions with longer execution times, such
 *          as Clear Display and Return Home, receive dedicated delays, while
 *          all other instructions use the standard execution time.
 *
 * @param   Command - HD44780 instruction whose execution time shall be waited.
 *
 * @note    This function is intended for write-only interfaces where the Busy
 *          Flag cannot be read from the controller.
 *
 * @return  None.
 **********************************************************************************************************************************/
static void HD44780_WaitInstruction(HD44780_CmdTypeDef Command)
{
    switch(Command)
    {
        case HD44780_CMD_CLEAR_DISPLAY:
        {
            Platform_DelayUs(2000);
            break;
        }

        case HD44780_CMD_RETURN_HOME:
        {
            Platform_DelayUs(1600);
            break;
        }

        default:
        break;
    }
}

/**********************************************************************************************************************************
 * @brief   Sends a command to the HD44780 controller.
 *
 * @details Splits the command byte into its higher and lower nibbles and
 *          transmits them sequentially through the configured bus interface.
 *          The higher nibble is transmitted first, followed by the lower
 *          nibble, as required by the HD44780 4-bit interface protocol.
 *
 * @param   Device  - Pointer to the HD44780 device instance.
 * @param   Command - HD44780 instruction to be transmitted.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
static HD44780_OpStatusTypeDef HD44780_SendCommand(HD44780_HandleTypeDef* Device, HD44780_CmdTypeDef Command)
{
    uint8_t high_nibble = 0x00;
    uint8_t low_nibble = 0x00;

    HD44780_EXTRACT_LOWER_NIBBLE(Command, low_nibble);
    HD44780_EXTRACT_HIGHER_NIBBLE(Command, high_nibble);

    if(Device->_bus.TransferNibble(Device->_bus.Context, high_nibble, HD44780_BUS_COMMAND) != HD44780_BUS_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    else
    {
        if(Device->_bus.TransferNibble(Device->_bus.Context, low_nibble, HD44780_BUS_COMMAND) != HD44780_BUS_OPERATION_OK)
        {
            return HD44780_OPERATION_FAIL;
        }
    }

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Sends a data byte to the HD44780 controller.
 *
 * @details Splits the data byte into its higher and lower nibbles and
 *          transmits them sequentially through the configured bus interface.
 *          The higher nibble is transmitted first, followed by the lower
 *          nibble, as required by the HD44780 4-bit interface protocol.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 * @param   Data   - Character or data byte to be written to the display.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
static HD44780_OpStatusTypeDef HD44780_SendData(HD44780_HandleTypeDef* Device, uint8_t Data)
{
    uint8_t high_nibble = 0x00;
    uint8_t low_nibble = 0x00;

    HD44780_EXTRACT_LOWER_NIBBLE(Data, low_nibble);
    HD44780_EXTRACT_HIGHER_NIBBLE(Data, high_nibble);

    if(Device->_bus.TransferNibble(Device->_bus.Context, high_nibble, HD44780_BUS_DATA) != HD44780_BUS_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    else
    {
        if(Device->_bus.TransferNibble(Device->_bus.Context, low_nibble, HD44780_BUS_DATA) != HD44780_BUS_OPERATION_OK)
        {
            return HD44780_OPERATION_FAIL;
        }
    }

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Initializes the HD44780 LCD controller.
 *
 * @details Performs the complete initialization sequence required by the
 *          HD44780 controller, including power-up timing, interface
 *          initialization, Function Set configuration and default display
 *          settings.
 *
 *          Upon successful completion, the display is configured with the
 *          interface mode, number of display lines and character font
 *          specified in the device handle. The display is cleared, enabled,
 *          and configured to increment the cursor after each data write.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    This driver currently supports only the 4-bit interface mode.
 *
 * @note    The initialization sequence follows the procedure recommended in
 *          the HD44780 datasheet for 4-bit operation.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_Init(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    Platform_DelayMs(50);

    Device->_bus.TransferNibble(Device->_bus.Context, 0x03, HD44780_BUS_COMMAND);

    Platform_DelayUs(4500);

    Device->_bus.TransferNibble(Device->_bus.Context, 0x03, HD44780_BUS_COMMAND);

    Platform_DelayUs(150);

    Device->_bus.TransferNibble(Device->_bus.Context, 0x03, HD44780_BUS_COMMAND);

    /* << Sets data inteface mode of HD44780 >> */
    switch(Device->_interface_mode)
    {
        /* << Not supported in this driver version! >> */
        case HD44780_8BIT_MODE: return HD44780_OPERATION_FAIL;

        default:
        case HD44780_4BIT_MODE:

            if(Device->_bus.TransferNibble(Device->_bus.Context, 0x02, HD44780_BUS_COMMAND) != HD44780_BUS_OPERATION_OK)
            {
                return HD44780_OPERATION_FAIL;
            }

        break;
    }

    switch(Device->_rows)
    {
        case HD44780_1LINE:

            if(Device->_font_dot_size == HD44780_5X8_FONT)
            {
                command  =  HD44780_CMD_FUNCTIONSET;
                command &= ~(1U << HD44780_N_LINE_NUMBER);
                command &= ~(1U << HD44780_F_CHAR_FONT);
            }

            else
            {
                command  =  HD44780_CMD_FUNCTIONSET;
                command &= ~(1U << HD44780_N_LINE_NUMBER);
                command |=  (1U << HD44780_F_CHAR_FONT);
            }

            if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
            {
                return HD44780_OPERATION_FAIL;
            }

        break;

        default:
        case HD44780_2LINE:

            if(Device->_font_dot_size == HD44780_5X8_FONT)
            {
                command  =  HD44780_CMD_FUNCTIONSET;
                command |=  (1U << HD44780_N_LINE_NUMBER);
                command &= ~(1U << HD44780_F_CHAR_FONT);
            }

            else
            {
                command  =  HD44780_CMD_FUNCTIONSET;
                command |=  (1U << HD44780_N_LINE_NUMBER);
                command |=  (1U << HD44780_F_CHAR_FONT);
            }

            if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
            {
                return HD44780_OPERATION_FAIL;
            }

        break;
    }

    /* << Clear display before initialization >> */
    HD44780_Clear(Device);
    /* << Display Control bits disabled >> */
    HD44780_SendCommand(Device, HD44780_CMD_DISPLAYCONTROL);
    /* << Sets increment cursor in Entry Mode Set command (shift disable) >> */
    HD44780_IncrementCursor(Device);
    /* << Sets display on/off flag into Display Control command >> */
    HD44780_DisplayOn(Device);

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Clears the display and returns the cursor to the home position.
 *
 * @details Writes a space character to every DDRAM location, clears the
 *          visible display contents and resets the DDRAM address counter to
 *          zero. If the display has been shifted, it is restored to its
 *          original position.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    This instruction requires a longer execution time than most
 *          HD44780 commands. The function waits for the instruction to
 *          complete before returning.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_Clear(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    command = HD44780_CMD_CLEAR_DISPLAY;

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    HD44780_WaitInstruction(HD44780_CMD_CLEAR_DISPLAY);

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Returns the cursor to the home position.
 *
 * @details Resets the DDRAM address counter to zero, moving the cursor to the
 *          home position. If the display has been shifted, it is restored to
 *          its original position. The contents of DDRAM remain unchanged.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    This instruction requires a longer execution time than most
 *          HD44780 commands. The function waits for the instruction to
 *          complete before returning.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_Home(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    command = HD44780_CMD_RETURN_HOME;

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    HD44780_WaitInstruction(HD44780_CMD_RETURN_HOME);

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Turns the display on.
 *
 * @details Enables the display by setting the Display ON/OFF (D) bit of the
 *          Display Control instruction. The current cursor visibility and
 *          blinking configuration are preserved.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    This function does not modify the display contents stored in DDRAM.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_DisplayOn(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = HD44780_GetDisplayControlFlags();

    command  = HD44780_CMD_DISPLAYCONTROL;
    command |= display_seted_flags;
    command |= (1 << HD44780_D_DISPLAY_ON_OFF);

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    /* << Set global flag for display on/off >> */
    g_display_on_off_flag = 0x01;

    HD44780_WaitInstructionDefault();

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Turns the display off.
 *
 * @details Disables the display by clearing the Display ON/OFF (D) bit of the
 *          Display Control instruction. The display contents stored in DDRAM
 *          are preserved, as are the current cursor visibility and blinking
 *          configuration.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    Turning the display off does not clear DDRAM. The previously
 *          written contents become visible again when the display is turned
 *          back on.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_DisplayOff(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = HD44780_GetDisplayControlFlags();

    command  = HD44780_CMD_DISPLAYCONTROL;
    command |= display_seted_flags;
    command &= ~(1 << HD44780_D_DISPLAY_ON_OFF);

    command |= ((HD44780_CMD_DISPLAYCONTROL | display_seted_flags) & ~(1 << HD44780_D_DISPLAY_ON_OFF));

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    /* << Reset global flag for display on/off >> */
    g_display_on_off_flag = 0x00;

    HD44780_WaitInstructionDefault();

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
* @brief   Enables the cursor.
 *
 * @details Sets the Cursor ON/OFF (C) bit of the Display Control instruction,
 *          making the cursor visible. The current display state and blinking
 *          configuration are preserved.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    This function only affects cursor visibility. The cursor position
 *          and the display contents remain unchanged.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_CursorOn(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = HD44780_GetDisplayControlFlags();

    command  = HD44780_CMD_DISPLAYCONTROL;
    command |= display_seted_flags;
    command |= (1 << HD44780_C_CURSOR_ON_OFF);

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    /* << Set global flag for cursor on/off >> */
    g_cursor_on_off_flag = 0x01;

    HD44780_WaitInstructionDefault();

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Disables the cursor.
 *
 * @details Clears the Cursor ON/OFF (C) bit of the Display Control instruction,
 *          making the cursor invisible. The current display state and blinking
 *          configuration are preserved.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    This function only affects cursor visibility. The cursor position
 *          and the display contents remain unchanged.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_CursorOff(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = HD44780_GetDisplayControlFlags();

    command  = HD44780_CMD_DISPLAYCONTROL;
    command |= display_seted_flags;
    command &= ~(1 << HD44780_C_CURSOR_ON_OFF);

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    /* << Reset global flag for cursor on/off >> */
    g_cursor_on_off_flag = 0x00;

    HD44780_WaitInstructionDefault();

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Enables cursor blinking.
 *
 * @details Sets the Blinking ON/OFF (B) bit of the Display Control instruction,
 *          enabling cursor blinking. The current display state and cursor
 *          visibility are preserved.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    Cursor blinking is only visible when the cursor is enabled.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_BlinkOn(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = HD44780_GetDisplayControlFlags();

    command  = HD44780_CMD_DISPLAYCONTROL;
    command |= display_seted_flags;
    command |= (1 << HD44780_B_BLINKING_ON_OFF);

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    /* << Set global flag for blink on/off >> */
    g_blink_on_off_flag = 0x01;

    HD44780_WaitInstructionDefault();

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Disables cursor blinking.
 *
 * @details Clears the Blinking ON/OFF (B) bit of the Display Control
 *          instruction, disabling cursor blinking. The current display state
 *          and cursor visibility are preserved.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    Disabling blinking does not affect the cursor position or
 *          visibility.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_BlinkOff(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = HD44780_GetDisplayControlFlags();

    command  = HD44780_CMD_DISPLAYCONTROL;
    command |= display_seted_flags;
    command &= ~(1 << HD44780_B_BLINKING_ON_OFF);

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    /* << Reset global flag for blink on/off >> */
    g_blink_on_off_flag = 0x00;

    HD44780_WaitInstructionDefault();

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Configures the cursor to increment after each data write.
 *
 * @details Sets the Increment/Decrement (I/D) bit of the Entry Mode Set
 *          instruction, causing the DDRAM address to increment after each
 *          character is written to or read from the display. The current
 *          automatic display shift configuration is preserved.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    With automatic display shift disabled, the visible cursor moves to
 *          the right after each character operation.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_IncrementCursor(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = HD44780_GetEntryModeFlags();

    command  = HD44780_CMD_ENTRYMODESET;
    command |= display_seted_flags;
    command |= (1 << HD44780_ID_INC_DEC_CURSOR);

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    HD44780_WaitInstructionDefault();

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Configures the cursor to decrement after each data write.
 *
 * @details Clears the Increment/Decrement (I/D) bit of the Entry Mode Set
 *          instruction, causing the DDRAM address to decrement after each
 *          character is written to or read from the display. The current
 *          automatic display shift configuration is preserved.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    With automatic display shift disabled, the visible cursor moves to
 *          the left after each character operation.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_DecrementCursor(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = HD44780_GetEntryModeFlags();

    command  = HD44780_CMD_ENTRYMODESET;
    command |= display_seted_flags;
    command &= ~(1 << HD44780_ID_INC_DEC_CURSOR);

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    HD44780_WaitInstructionDefault();

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Enables automatic display shifting.
 *
 * @details Sets the Shift (S) bit of the Entry Mode Set instruction, causing
 *          the display to shift automatically after each character operation.
 *          The shift direction is determined by the current Increment/
 *          Decrement (I/D) configuration.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    Enabling automatic display shift does not change the current
 *          cursor movement direction.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_EnableShift(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = HD44780_GetEntryModeFlags();

    command  = HD44780_CMD_ENTRYMODESET;
    command |= display_seted_flags;
    command |= (1 << HD44780_S_ENABLE_SHIFT);

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    HD44780_WaitInstructionDefault();

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Disables automatic display shifting.
 *
 * @details Clears the Shift (S) bit of the Entry Mode Set instruction,
 *          preventing the display from shifting automatically after character
 *          operations. The current cursor movement direction is preserved.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 *
 * @note    Disabling automatic display shift does not affect the current
 *          cursor position.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_DisableShift(HD44780_HandleTypeDef* Device)
{
    uint8_t command = 0x00;

    uint8_t display_seted_flags = HD44780_GetEntryModeFlags();

    command  = HD44780_CMD_ENTRYMODESET;
    command |= display_seted_flags;
    command &= ~(1 << HD44780_S_ENABLE_SHIFT);

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    HD44780_WaitInstructionDefault();

    return HD44780_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Sets the cursor position.
 *
 * @details Positions the cursor by updating the HD44780 DDRAM address counter.
 *          The specified row and column are translated into the corresponding
 *          DDRAM address and transmitted using the Set DDRAM Address
 *          instruction.
 *
 * @param   Device - Pointer to the HD44780 device instance.
 * @param   row    - Zero-based display row.
 * @param   col    - Zero-based display column.
 *
 * @note    Row and column values outside the configured display dimensions are
 *          clamped to the nearest valid position.
 *
 * @return  HD44780_OPERATION_OK   - Indicates that the command was successfully transmitted.
 *          HD44780_OPERATION_FAIL - Indicates that the command has failed while attempting transmission.
 **********************************************************************************************************************************/
HD44780_OpStatusTypeDef HD44780_SetCursor(HD44780_HandleTypeDef* Device, uint8_t Row, uint8_t Col)
{
    uint8_t line0_base_addr = 0x00;
    uint8_t line1_base_addr = 0x40;
    uint8_t address_counter = 0x00;

    uint8_t command = 0x00;

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

    command = HD44780_CMD_SETDDRAMADDR;

    command |= HD44780_SET_DDRAM_ADDRESS_MASK(address_counter);

    if(HD44780_SendCommand(Device, command) != HD44780_OPERATION_OK)
    {
        return HD44780_OPERATION_FAIL;
    }

    HD44780_WaitInstructionDefault();

    return HD44780_OPERATION_OK;
}
















