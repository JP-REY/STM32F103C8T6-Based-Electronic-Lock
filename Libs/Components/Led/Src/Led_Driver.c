/**********************************************************************************************************************************
 * @file    Led_Driver.c
 * @brief   Led_Driver.h module implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Led_Driver.h"
#include "GPIO_Platform_Interface.h"
#include "Time_Platform_Interface.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
#define LED_PULSE_EFFECT_REPEATS 1U

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
/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief    Checks whether the LED driver has been initialized.
 *
 * @details  Performs a NULL-pointer check and verifies the initialization
 *           state stored in the LED driver handle.
 *
 * @param    Device - Pointer to the LED driver handle.
 *
 * @return   true  - Whether Device is valid and initialized.
 * @return   false - Whether Device is NULL or has not been initialized.
 */
static inline bool LED_IsInit(LED_Handle_t* Device)
{
    return Device == NULL ? false : Device->_initialized;
}

/**
 * @brief    Sets the currently active LED effect.
 *
 * @details  Updates the current effect stored in the LED driver handle and
 *           resets the effect timing reference using the platform millisecond
 *           time base.
 *
 *           The function only accepts effects defined by LED_Effect_t.
 *           It is intended for internal use by the LED driver.
 *
 *           LED_EFFECT_STATIC - selects static LED operation with no
 *                               timed transitions.
 *           LED_EFFECT_BLINK  - enables continuous LED blinking.
 *           LED_EFFECT_FLASH  - enables the finite flash sequence.
 *           LED_EFFECT_PULSE  - enables the finite pulse effect.
 *
 * @param    Device - Pointer to the LED driver handle.
 * @param    Effect - Effect to be activated.
 *
 * @return   LED_OPERATION_OK   - Whether the effect was successfully selected.
 * @return   LED_OPERATION_FAIL - Whether Device is NULL or not initialized.
 */
static inline LED_OpStatus_t LED_SetEffect(LED_Handle_t* Device, LED_Effect_t Effect)
{
    if(Device == NULL || !LED_IsInit(Device))
    {
        return LED_OPERATION_FAIL;
    }
    
    uint32_t now = Platform_GetMillis();

    LED_EffectContext_t* fx_ctx = &Device->_effect_context;

    switch(Effect)
    {
        case LED_EFFECT_STATIC: fx_ctx->_current_effect = LED_EFFECT_STATIC; break;

        case LED_EFFECT_BLINK:  fx_ctx->_current_effect = LED_EFFECT_BLINK;  break;

        case LED_EFFECT_PULSE:  fx_ctx->_current_effect = LED_EFFECT_PULSE;  break;

        case LED_EFFECT_FLASH:  fx_ctx->_current_effect = LED_EFFECT_FLASH;  break;

        default: return LED_OPERATION_FAIL;
    }

    Device->_last_update_time_ms  = now;

    return LED_OPERATION_OK;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief    Initializes an LED driver instance.
 *
 * @details  Associates the LED driver with a previously configured GPIO
 *           interface and initializes all internal driver state required
 *           for direct LED control and timed effects.
 *
 *           The ActiveLevel parameter defines the electrical GPIO level
 *           required to turn the LED on. This allows the driver to support
 *           both active-high and active-low LED configurations.
 *
 *           The initial logical LED state is determined by reading the
 *           current GPIO level. The driver does not force the GPIO into
 *           an ON or OFF state during initialization.
 *
 *           The effect context is initialized to LED_EFFECT_STATIC.
 *           No timed effect is active after initialization.
 *
 * @param    Device      - Pointer to the LED driver handle.
 * @param    Gpio        - Pointer to the GPIO platform handle associated
 *                         with the LED.
 * @param    ActiveLevel - Electrical GPIO level that activates the LED.
 *
 * @note     The GPIO platform interface must be configured for output
 *           operation before the LED driver is initialized.
 * 
 * @return   LED_OPERATION_OK   - Whether the LED driver and GPIO were successfully initialized.
 * @return   LED_OPERATION_FAIL - Whether Device is NULL or GPIO initialization fails.
 */
LED_OpStatus_t LED_Init(LED_Handle_t* Device, void* Gpio, LED_ActiveLevel_t ActiveLevel)
{
    if(Device == NULL || Gpio == NULL ||
      (ActiveLevel != LED_ACTIVE_HIGH &&
       ActiveLevel!= LED_ACTIVE_LOW))
    {
        return LED_OPERATION_FAIL;
    }

    LED_EffectContext_t* fx_ctx = &Device->_effect_context;

    Device->_gpio                    = Gpio;
    Device->_current_state           = LED_STATE_OFF;
    Device->_active_level            = ActiveLevel;
    Device->_last_update_time_ms     = 0;
    Device->_blink_time_interval_ms  = 0;
    Device->_effect_time_interval_ms = 0;
    Device->_effect_counter          = 0;
    Device->_effect_is_active        = false;
    fx_ctx->_return_led_state        = LED_STATE_OFF;
    fx_ctx->_current_effect          = LED_EFFECT_STATIC;
    fx_ctx->_return_effect           = LED_EFFECT_STATIC;
    Device->_initialized             = true;

    LED_Off(Device);

    return LED_OPERATION_OK;
}

/**
 * @brief    Turns the LED on.
 *
 * @details  Drives the LED GPIO to its configured active electrical level
 *           and updates the logical LED state stored by the driver.
 *
 *           The active GPIO level is determined by the ActiveLevel parameter
 *           supplied during LED_Init().
 *
 *           The current effect is restored from the effect context after
 *           the LED state is changed.
 *
 * @param    Device - Pointer to the LED driver handle.
 *
 * @note     Calling LED_On() does not necessarily disable a previously
 *           configured timed effect. LED_Update() may subsequently modify
 *           the LED state if an effect remains active.
 * 
 * @return   LED_OPERATION_OK   - Whether the GPIO was successfully set.
 * @return   LED_OPERATION_FAIL - Whether Device is NULL, not initialized, 
 *                                or the GPIO operation fails.
 */
LED_OpStatus_t LED_On(LED_Handle_t* Device)
{
    if(Device == NULL || !LED_IsInit(Device))
    {
        return LED_OPERATION_FAIL;
    }

    LED_EffectContext_t* fx_ctx = &Device->_effect_context;

    if(Device->_active_level == LED_ACTIVE_LOW)
    {
        if(PGPIO_Reset((GPIO_Handle_t*)Device->_gpio) != GPIO_OPERATION_OK)

            return LED_OPERATION_FAIL;
    }

    else
    {
        if(PGPIO_Set((GPIO_Handle_t*)Device->_gpio) != GPIO_OPERATION_OK)

            return LED_OPERATION_FAIL;
    }

    Device->_current_state  = LED_STATE_ON;
    fx_ctx->_current_effect = fx_ctx->_return_effect;

    return LED_OPERATION_OK;
}

/**
 * @brief    Turns the LED off.
 *
 * @details  Drives the LED GPIO to its configured inactive electrical level
 *           and updates the logical LED state stored by the driver.
 *
 *           The inactive GPIO level is determined from the ActiveLevel
 *           configuration supplied during LED_Init().
 *
 *           The current effect is stored in the effect context so it can
 *           be restored when required by the effect-control mechanism.
 *
 * @param    Device - Pointer to the LED driver handle.
 *
 * @note     Calling LED_Off() does not necessarily disable a previously
 *           configured timed effect. LED_Update() may subsequently modify
 *           the LED state if an effect remains active.
 * 
 * @return   LED_OPERATION_OK   - Whether the GPIO was successfully set.
 * @return   LED_OPERATION_FAIL - Whether Device is NULL, not initialized, 
 *                                or the GPIO operation fails.
 */
LED_OpStatus_t LED_Off(LED_Handle_t* Device)
{
    if(Device == NULL || !LED_IsInit(Device))
    {
        return LED_OPERATION_FAIL;
    }

    LED_EffectContext_t* fx_ctx = &Device->_effect_context;

    if(Device->_active_level == LED_ACTIVE_LOW)
    {
        if(PGPIO_Set((GPIO_Handle_t*)Device->_gpio) != GPIO_OPERATION_OK)

            return LED_OPERATION_FAIL;
    }

    else
    {
        if(PGPIO_Reset((GPIO_Handle_t*)Device->_gpio) != GPIO_OPERATION_OK)

            return LED_OPERATION_FAIL;
    }

    Device->_current_state  = LED_STATE_OFF;
    fx_ctx->_return_effect  = fx_ctx->_current_effect;

    return LED_OPERATION_OK;
}

/**
 * @brief    Enables continuous LED blinking.
 *
 * @details  Configures the LED driver to execute the blink effect using
 *           the specified interval between consecutive LED state transitions.
 *
 *           The function is non-blocking. The actual LED transitions are
 *           performed by LED_Update().
 *
 * @param    Device      - Pointer to the LED driver handle.
 * @param    BlinkTimeMs - Time interval between LED state transitions,
 *                         expressed in milliseconds.
 *
 * @note     LED_Update() must be called periodically while the blink effect
 *           is active.
 *
 * @return   LED_OPERATION_OK   - Whether the blink effect was configured.
 * @return   LED_OPERATION_FAIL - Whether Device is NULL or not initialized.
 */
LED_OpStatus_t LED_BlinkOn(LED_Handle_t* Device, uint32_t BlinkTimeMs)
{
    if(Device == NULL || !LED_IsInit(Device))
    {
        return LED_OPERATION_FAIL;
    }

    uint32_t now = Platform_GetMillis();

    LED_EffectContext_t* fx_ctx = &Device->_effect_context;

    Device->_blink_time_interval_ms  = BlinkTimeMs;
    Device->_last_update_time_ms   = now;

    fx_ctx->_current_effect = LED_EFFECT_BLINK;

    return LED_OPERATION_OK;
}

/**
 * @brief    Disables the continuous LED blink effect.
 *
 * @details  Clears the configured blink interval and changes the current
 *           effect to LED_EFFECT_STATIC.
 *
 *           The LED output state itself is not modified by this function.
 *
 * @param    Device - Pointer to the LED driver handle.
 *
 * @return   LED_OPERATION_OK   - Whether the blink effect was disabled.
 * @return   LED_OPERATION_FAIL - Whether Device is NULL or not initialized.
 */
LED_OpStatus_t LED_BlinkOff(LED_Handle_t* Device)
{
    if(Device == NULL || !LED_IsInit(Device))
    {
        return LED_OPERATION_FAIL;
    }

    LED_EffectContext_t* fx_ctx = &Device->_effect_context;

    Device->_blink_time_interval_ms  = 0;
    Device->_last_update_time_ms   = 0;
    
    fx_ctx->_current_effect = LED_EFFECT_STATIC;

    return LED_OPERATION_OK;
}

/**
 * @brief    Triggers a finite LED effect.
 *
 * @details  Configures a finite pulse or flash sequence using the specified
 *           interval and number of repetitions.
 *
 *           Each repetition consists of two LED state transitions. The
 *           effect therefore requires twice the configured repetition count
 *           as its internal transition counter.
 *
 *           LED_EFFECT_PULSE is always executed as a single repetition,
 *           regardless of the Repeats parameter.
 *
 *           Before the effect starts, the current LED state and current
 *           effect are stored in the effect context. These values are used
 *           to restore the previous LED behavior when the triggered effect
 *           finishes.
 *
 * @param    Device   - Pointer to the LED driver handle.
 * @param    Effect   - Finite LED effect to trigger.
 * @param    Interval - Time interval between consecutive LED state
 *                      transitions, expressed in milliseconds.
 * @param    Repeats  - Number of complete effect cycles to execute.
 *
 * @note     The effect is non-blocking. LED_Update() must be called
 *           periodically while the effect is active.
 *
 * @return   LED_OPERATION_OK   - Whether the GPIO was successfully set.
 * @return   LED_OPERATION_FAIL - Whether Device is NULL, not initialized, 
 *                                or the GPIO operation fails.
 */
LED_OpStatus_t LED_TriggerEffect(LED_Handle_t* Device, LED_Effect_t Effect, uint32_t Interval, uint16_t Repeats)
{
    if(Device == NULL || !LED_IsInit(Device))
    {
        return LED_OPERATION_FAIL;
    }

    LED_EffectContext_t* fx_ctx = &Device->_effect_context;

    Device->_effect_time_interval_ms   = Interval;
    Device->_effect_repeat    = Repeats;
    Device->_effect_repeat    = Effect == LED_EFFECT_PULSE         ?
                                          LED_PULSE_EFFECT_REPEATS :
                                          Repeats                  ;

    Device->_effect_counter   = (Device->_effect_repeat) * 2;

    fx_ctx->_return_effect    = fx_ctx->_current_effect;
    fx_ctx->_return_led_state = Device->_current_state;

    if(LED_SetEffect(Device, Effect) != LED_OPERATION_OK)

        return LED_OPERATION_FAIL;

    return LED_OPERATION_OK;
}

/**
 * @brief    Updates the currently active LED effect.
 *
 * @details  Executes the state transitions required by the currently active
 *           LED effect without blocking the application.
 *
 *           LED_EFFECT_BLINK toggles the LED whenever the configured blink
 *           interval has elapsed.
 *
 *           LED_EFFECT_PULSE executes a finite sequence of LED state
 *           transitions and then restores the previously active effect.
 *
 *           LED_EFFECT_FLASH executes a finite sequence of LED state
 *           transitions and then restores both the previously active effect
 *           and the LED state stored when the effect was triggered.
 *
 *           LED_EFFECT_STATIC do not perform timed state transitions.
 *
 *           The function uses the platform millisecond time base and does
 *           not introduce blocking delays.
 *
 * @param    Device - Pointer to the LED driver handle.
 *
 * @note     LED_Update() must be called periodically by the application
 *           while a timed LED effect is active.
 *
 * @note     The function is intended for execution from the application's
 *           main loop or from a periodic task.
 *
 * @return   LED_OPERATION_OK   - Whether the effect update was processed
 *                                successfully.
 * @return   LED_OPERATION_FAIL - Whether Device is NULL, not initialized, 
 *                                or an underlying GPIO operation fails.
 */
LED_OpStatus_t LED_Update(LED_Handle_t* Device)
{
    if(Device == NULL || !LED_IsInit(Device))
    {
        return LED_OPERATION_FAIL;
    }
    
    uint32_t now = Platform_GetMillis();
    
    LED_EffectContext_t* fx_ctx = &Device->_effect_context;

    switch(fx_ctx->_current_effect)
    {
        case LED_EFFECT_BLINK:
        {
            if(now - Device->_last_update_time_ms >= Device->_blink_time_interval_ms)
            {
                Device->_last_update_time_ms = now;

                if(PGPIO_Toggle((GPIO_Handle_t*)Device->_gpio) != GPIO_OPERATION_OK)

                    return LED_OPERATION_FAIL;

                Device->_current_state  = PGPIO_GetLevel((GPIO_Handle_t*)Device->_gpio) == GPIO_LEVEL_HIGH ?
                                                                                           LED_STATE_ON    :
                                                                                           LED_STATE_OFF   ;
            }
        }

        break;

        case LED_EFFECT_PULSE:
        {
            if(Device->_effect_counter > 0)
            {
                if(now - Device->_last_update_time_ms >= Device->_effect_time_interval_ms)
                {
                    Device->_last_update_time_ms = now;

                    Device->_effect_counter--;

                    if(PGPIO_Toggle((GPIO_Handle_t*)Device->_gpio) != GPIO_OPERATION_OK)

                        return LED_OPERATION_FAIL;


                }
            }

            else
            {
                LED_SetEffect(Device, fx_ctx->_return_effect);
            }
        }

        break;

        case LED_EFFECT_FLASH:
        {
            if(!Device->_effect_is_active)
            {
                Device->_effect_is_active = true;
            }

            if(Device->_effect_counter > 0)
            {
                if(now - Device->_last_update_time_ms >= Device->_effect_time_interval_ms)
                {
                    Device->_last_update_time_ms = now;

                    Device->_effect_counter--;

                    if(PGPIO_Toggle((GPIO_Handle_t*)Device->_gpio) != GPIO_OPERATION_OK)

                        return LED_OPERATION_FAIL;
                }
            }

            else
            {
                LED_SetEffect(Device, fx_ctx->_return_effect);

                Device->_effect_is_active = false;

                if (fx_ctx->_return_led_state == LED_STATE_ON)
                {
                    LED_On(Device);
                }
                else
                {
                    LED_Off(Device);
                }
            }
        }

        break;

        default:
            break;
    }

    return LED_OPERATION_OK;
}
