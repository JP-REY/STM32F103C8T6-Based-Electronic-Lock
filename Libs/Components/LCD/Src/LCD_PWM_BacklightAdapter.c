/**********************************************************************************************************************************
 * @file    LCD_PWM_BacklightAdapter.c
 * @brief   LCD_PWM_BacklightAdapter.c module implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "LCD_PWM_BacklightAdapter.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
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
static LCD_BacklightOpStatus_t TurnOn        (void* Context);
static LCD_BacklightOpStatus_t TurnOff       (void* Context);
static LCD_BacklightOpStatus_t SetBrightness (void* Context, uint16_t Level);
static uint16_t                GetBrightness (void* Context);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Enables the LCD backlight using PWM control.
 *
 * @details Enables the PWM peripheral associated with the LCD backlight by
 *          invoking the platform PWM enable operation.
 *
 *          The current PWM duty cycle configuration is preserved. Therefore,
 *          the resulting backlight intensity depends on the duty cycle value
 *          previously configured in the PWM context.
 *
 * @param   Context - Pointer to the PWM_Handle_t instance associated with
 *                    the LCD backlight control.
 *
 * @note    If the PWM context was initialized with a duty cycle of 0%, the
 *          backlight will be enabled electrically but no visible illumination
 *          will be produced until a non-zero brightness level is configured.
 *
 * @return  LCD_BACKLIGHT_OP_OK   - Indicates that the backlight was successfully enabled.
 *          LCD_BACKLIGHT_OP_FAIL - Indicates that the PWM context is invalid or the enable operation failed.
 */
static LCD_BacklightOpStatus_t TurnOn(void* Context)
{
    PWM_Handle_t* ctx = Context;

    if(ctx == NULL)
    {
        return LCD_BACKLIGHT_OP_FAIL;
    }

    if(PPWM_Enable(ctx) != PWM_OPERATION_OK)
    {
        return LCD_BACKLIGHT_OP_FAIL;
    }

    return LCD_BACKLIGHT_OP_OK;
}

/**
 * @brief   Disables the LCD backlight using PWM control.
 *
 * @details Disables the PWM peripheral associated with the LCD backlight.
 *          The current PWM duty cycle configuration is preserved and can be
 *          restored when the backlight is enabled again.
 *
 * @param   Context - Pointer to the PWM_Handle_t instance associated with
 *                    the LCD backlight control.
 *
 * @note    Turning the backlight off does not modify the configured brightness
 *          level stored in the PWM context.
 *
 * @return  LCD_BACKLIGHT_OP_OK   - Indicates that the backlight was successfully disabled.
 *          LCD_BACKLIGHT_OP_FAIL - Indicates that the PWM context is invalid or the disable operation failed.
 */
static LCD_BacklightOpStatus_t TurnOff(void* Context)
{
    PWM_Handle_t* ctx = Context;

    if(ctx == NULL)
    {
        return LCD_BACKLIGHT_OP_FAIL;
    }

    if(PPWM_Disable(ctx) != PWM_OPERATION_OK)
    {
        return LCD_BACKLIGHT_OP_FAIL;
    }

    return LCD_BACKLIGHT_OP_OK;
}

/**
 * @brief   Sets the LCD backlight brightness using PWM duty cycle control.
 *
 * @details Updates the PWM duty cycle associated with the LCD backlight
 *          according to the requested brightness percentage.
 *
 *          The brightness level is converted internally by the PWM platform
 *          layer into the corresponding timer compare value.
 *
 * @param   Context - Pointer to the PWM_Handle_t instance associated with
 *                    the LCD backlight control.
 * @param   Level   - Desired brightness level expressed as a percentage from
 *                    0 to 100.
 *
 * @note    This function only updates the PWM duty cycle. It does not enable
 *          the PWM output automatically.
 *
 * @return  LCD_BACKLIGHT_OP_OK   - Indicates that the brightness was successfully updated.
 *          LCD_BACKLIGHT_OP_FAIL - Indicates that the PWM context is invalid or the duty cycle update failed.
 */
static LCD_BacklightOpStatus_t SetBrightness(void* Context, uint16_t Level)
{
    PWM_Handle_t* ctx = Context;

    if(ctx == NULL)
    {
        return LCD_BACKLIGHT_OP_FAIL;
    }

    if(PPWM_SetDutyPercent(ctx, Level) != PWM_OPERATION_OK)
    {
        return LCD_BACKLIGHT_OP_FAIL;
    }

    return LCD_BACKLIGHT_OP_OK;
}

/**
 * @brief   Gets the current LCD backlight brightness level.
 *
 * @details Returns the brightness percentage currently configured in the PWM
 *          context by reading the stored PWM duty cycle value.
 *
 *          The returned value represents the configured duty cycle percentage
 *          and does not represent the actual optical brightness emitted by the
 *          LCD backlight.
 *
 * @param   Context - Pointer to the PWM_Handle_t instance associated with
 *                    the LCD backlight control.
 *
 * @note    If the supplied context is invalid, this function returns 0.
 *
 * @return  Current backlight brightness level expressed as a percentage from 0 to 100.
 */
static uint16_t GetBrightness(void* Context)
{
    PWM_Handle_t* ctx = Context;

    if(ctx == NULL)
    {
        return 0;
    }

    return PPWM_GetDutyPercent(ctx);
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes the LCD PWM backlight adapter interface.
 *
 * @details Configures an LCD_BacklightInterface_t instance to use
 *          PWM-based backlight control operations.
 *
 *          This function binds the generic backlight interface callbacks to
 *          the PWM implementation and stores the PWM context pointer that will
 *          be forwarded to each operation.
 *
 * @param   Backlight - Pointer to the backlight interface instance to initialize.
 * @param   Context   - Pointer to the PWM_Handle_t instance used to control
 *                      the LCD backlight.
 *
 * @note    The supplied PWM context must remain valid throughout the lifetime
 *          of the backlight interface usage.
 *
 * @return  LCD_BACKLIGHT_OP_OK   - Indicates that the adapter was successfully initialized.
 *          LCD_BACKLIGHT_OP_FAIL - Indicates that one or more parameters are invalid.
 */
LCD_BacklightOpStatus_t LCD_PWM_BacklightAdapterInit(LCD_BacklightInterface_t* Backlight, PWM_Handle_t* Context)
{
    if ((Backlight == NULL) || (Context == NULL))
    {
        return LCD_BACKLIGHT_OP_FAIL;
    }

    Backlight->Context       = Context;
    Backlight->TurnOn        = TurnOn;
    Backlight->TurnOff       = TurnOff;
    Backlight->SetBrightness = SetBrightness;
    Backlight->GetBrightness = GetBrightness;

    return LCD_BACKLIGHT_OP_OK;
}
