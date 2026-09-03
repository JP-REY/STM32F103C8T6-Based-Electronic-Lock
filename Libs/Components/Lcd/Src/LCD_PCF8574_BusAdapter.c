/**********************************************************************************************************************************
 * @file    LCD_PCF8574_BusAdapter.c
 * @brief   LCD_PCF8574_BusAdapter.h module implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "LCD_PCF8574_BusAdapter.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**
 * @brief   Returns the bit mask corresponding to the specified bit position.
 */
#define BIT_MASK(bit)                     (1U << (bit))

/**
 * @brief   LCD to PCF8574 pin mapping.
 */
#define LCD_RS_PIN_2_PCF8574_BIT      0U
#define LCD_RW_PIN_2_PCF8574_BIT      1U
#define LCD_EN_PIN_2_PCF8574_BIT      2U
#define LCD_D4_PIN_2_PCF8574_BIT      3U
#define LCD_D5_PIN_2_PCF8574_BIT      4U
#define LCD_D6_PIN_2_PCF8574_BIT      5U
#define LCD_D7_PIN_2_PCF8574_BIT      6U

/**
 * @brief   Data line mapping parameters.
 */
#define LCD_PCF8574_DATA_SHIFT        LCD_D4_PIN_2_PCF8574_BIT

#define LCD_PCF8574_DATA_MASK        ( \
    BIT_MASK(LCD_D4_PIN_2_PCF8574_BIT) | \
    BIT_MASK(LCD_D5_PIN_2_PCF8574_BIT) | \
    BIT_MASK(LCD_D6_PIN_2_PCF8574_BIT) | \
    BIT_MASK(LCD_D7_PIN_2_PCF8574_BIT))

/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
static LCD_BusOpStatus_t TransferNibble (void* Context, uint8_t Nibble, LCD_RegisterSelect_t Rs);
static LCD_BusOpStatus_t PulseEnable    (void* Context);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Transfers a 4-bit value to the LCD through the PCF8574.
 *
 * @details Builds the corresponding PCF8574 output state for the requested
 *          nibble and target register (instruction or data), updates the
 *          expander outputs and generates the Enable pulse required to latch
 *          the data into the display controller.
 *
 * @param   Context - Pointer to the associated PCF8574 driver instance.
 * @param   Nibble  - 4-bit value to be transferred.
 * @param   Rs      - Selects the target LCD register.
 *
 * @note    This function implements the LCD bus interface callback used by
 *          the LCD driver.
 *
 * @return  LCD_BUS_OPERATION_OK   - Indicates if the transfer completes successfully.
 * @return  LCD_BUS_OPERATION_FAIL - Indicates if the transfer failed.
 */
static LCD_BusOpStatus_t TransferNibble(void* Context, uint8_t Nibble, LCD_RegisterSelect_t Rs)
{
    PCF8574_Handle_t* ctx = Context;
    uint8_t write_port = 0x00;

    if(ctx == NULL)
    {
        return LCD_BUS_OPERATION_FAIL;
    }

    if (Rs == LCD_BUS_DATA)
    {
        write_port |= BIT_MASK(LCD_RS_PIN_2_PCF8574_BIT);
    }

    write_port |= ((Nibble << LCD_PCF8574_DATA_SHIFT) &
                  LCD_PCF8574_DATA_MASK);

    if(PCF8574_WritePort(ctx, write_port) == PCF8574_OPERATION_OK)
    {
        return PulseEnable(ctx);
    }
    else
    {
        return LCD_BUS_OPERATION_FAIL;
    }
}

/**
 * @brief   Generates the LCD Enable pulse.
 *
 * @details Drives the Enable signal through the PCF8574 to latch the
 *          previously configured data and control signals into the display.
 *
 * @param   Context - Pointer to the associated PCF8574 driver instance.
 *
 * @note    This function assumes that the data lines and the Register Select
 *          signal have already been configured.
 *
 * @return  LCD_BUS_OPERATION_OK   - Indicates if the pulse is successfully generated;
 * @return  LCD_BUS_OPERATION_FAIL - Indicates if the pulse generation has failed;
 */
static LCD_BusOpStatus_t PulseEnable(void* Context)
{
    PCF8574_Handle_t* ctx = Context;

    if(ctx == NULL)
    {
        return LCD_BUS_OPERATION_FAIL;
    }

    PCF8574_ClearBit(ctx, LCD_EN_PIN_2_PCF8574_BIT);

    if(PCF8574_WriteBit(ctx, LCD_EN_PIN_2_PCF8574_BIT) == PCF8574_OPERATION_OK)
    {
        PCF8574_ClearBit(ctx, LCD_EN_PIN_2_PCF8574_BIT);

        return LCD_BUS_OPERATION_OK;
    }
    else
    {
        return LCD_BUS_OPERATION_FAIL;
    }
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes the PCF8574 implementation of the LCD bus interface.
 *
 * @details Configures the supplied bus interface instance with the adapter
 *          context and registers the callback responsible for transferring
 *          nibbles to the display through the PCF8574.
 *
 * @param   Bus              - Pointer to the bus interface instance to initialize.
 * @param   PCF8574_Instance - Pointer to the associated PCF8574 driver instance.
 *
 * @note    The caller is responsible for ensuring that the PCF8574 driver has
 *          been properly initialized before this function is called.
 */
LCD_BusOpStatus_t LCD_PCF8574_BusAdapterInit(LCD_BusInterface_t* Bus, PCF8574_Handle_t* PCF8574_Instance)
{
    if ((Bus == NULL) || (PCF8574_Instance == NULL))
    {
        return LCD_BUS_OPERATION_FAIL;
    }

    Bus->Context = PCF8574_Instance;

    Bus->TransferNibble = TransferNibble;

    return LCD_BUS_OPERATION_OK;
}





















