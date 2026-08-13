/**********************************************************************************************************************************
 * @file    Led_Driver.h
 * @brief   Public interface for the LED driver.
 *
 * @details Provides an abstraction for controlling an LED through the GPIO
 *          platform interface, including direct ON/OFF control and
 *          non-blocking visual effects such as blink, pulse and flash.
 *
 *          The driver is designed to be updated periodically through
 *          LED_Update(), allowing LED effects to execute without blocking
 *          the application.
 *
 *          This implementation is intended to fulfill the LED control and
 *          visual indication requirements defined for the current project.
 *          Its interface and internal architecture may be extended in the
 *          future to support additional LED behaviors and application
 *          requirements without changing the underlying GPIO abstraction.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/ 

#ifndef LIBS_COMPONENTS_LED_INC_LED_DRIVER_H_
#define LIBS_COMPONENTS_LED_INC_LED_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
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
 * @brief    LED operation status.
 *
 * @details  Defines the possible return statuses for LED driver operations.
 */
typedef enum
{
    LED_OPERATION_OK,
    LED_OPERATION_FAIL

}LED_OpStatus_t;

/**
 * @brief   LED active electrical level.
 *
 * @details Defines the GPIO electrical level that activates the LED.
 *
 *          LED_ACTIVE_LOW indicates that the LED is turned on when the GPIO
 *          output is driven LOW.
 *
 *          LED_ACTIVE_HIGH indicates that the LED is turned on when the GPIO
 *          output is driven HIGH.
 */
typedef enum
{
    LED_ACTIVE_LOW  = 0U,
    LED_ACTIVE_HIGH = 1U

}LED_ActiveLevel_t;

/**
 * @brief    LED effect type.
 *
 * @details  Defines the effects used by LED instance.
 */
typedef enum
{
    LED_EFFECT_STATIC,
    LED_EFFECT_BLINK,
    LED_EFFECT_PULSE,
    LED_EFFECT_FLASH,

}LED_Effect_t;

/**
 * @brief    LED GPIO state.
 *
 * @details  Represents the logical output state of the LED instance.
 */
typedef enum
{
    LED_STATE_ON,
    LED_STATE_OFF

}LED_State_t;

/**
 * @brief    LED effect context.
 *
 * @details  Stores the internal effect state required to manage temporary
 *           LED effects and restore the LED to its previous operating state
 *           after the triggered effect has completed.
 *
 *           The current effect identifies the effect currently being executed,
 *           while the return effect identifies the effect that must be restored
 *           after a temporary effect such as pulse or flash completes.
 *
 *           The return LED state stores the logical LED state that must be
 *           restored after a temporary effect has completed.
 *
 * @warning  This structure is private driver state and should not be accessed
 *           or modified directly by the application.
 */
typedef struct
{
                                    /*< Private data. Do not read or modify!                                 */
    LED_State_t  _return_led_state; /*< LED state to restore after the flash effect complete.                */
    LED_Effect_t _current_effect;   /*< Currently active LED effect.                                         */
    LED_Effect_t _return_effect;    /*< LED effect to restore after the triggered effect has been completed. */

}LED_EffectContext_t;

/**
 * @brief    LED driver handle.
 *
 * @details  Stores the hardware configuration, current LED state, active
 *           electrical level, effect context and timing information required
 *           by the LED driver.
 *
 *           The active electrical level defines whether the LED is controlled
 *           as active-high or active-low. This allows the driver to provide
 *           the same logical ON/OFF interface regardless of the electrical
 *           polarity of the connected LED.
 *
 * @note     The handle must be initialized through LED_Init() before being
 *           passed to any other LED driver function.
 *
 * @warning  All struct members are internal driver state and should not be
 *           accessed or modified directly by the application.
*/
typedef struct
{
                                                  /*< Private data. Do not read or modify!                                   */
    void*               _gpio;                    /*< Pointer to LED gpio handle.                                            */
    LED_State_t         _current_state;           /*< Current LED logical state.                                             */
    LED_ActiveLevel_t   _active_level;            /*< Electrical level which LED is active (on).                             */
    LED_EffectContext_t _effect_context;          /*< LED effect context                                                     */
    uint16_t            _effect_repeat;           /*< Number of complete flash cycles configured for the current effect.     */
    uint16_t            _effect_counter;          /*< Number of remaining LED state transitions in the current flash effect. */
    uint32_t            _last_update_time_ms;     /*< Timestamp of the last LED effect update, in milliseconds.              */
    uint32_t            _blink_time_interval_ms;  /*< Blink effect interval, in milliseconds.                                */
    uint32_t            _effect_time_interval_ms; /*< Flash effect interval between consecutive state transitions            */
    bool                _effect_is_active;        /*< Indicates whether the flash effect is currently active.                */
    bool                _initialized;             /*< Indicates whether the LED driver has been successfully initialized.    */

}LED_Handle_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
LED_OpStatus_t LED_Init          (LED_Handle_t* Device, void* Gpio, LED_ActiveLevel_t ActiveLevel);
LED_OpStatus_t LED_On            (LED_Handle_t* Device);
LED_OpStatus_t LED_Off           (LED_Handle_t* Device);
LED_OpStatus_t LED_BlinkOn       (LED_Handle_t* Device, uint32_t BlinkTimeMs);
LED_OpStatus_t LED_BlinkOff      (LED_Handle_t* Device);
LED_OpStatus_t LED_TriggerEffect (LED_Handle_t* Device, LED_Effect_t Effect, uint32_t Interval, uint16_t Repeats);
LED_OpStatus_t LED_Update        (LED_Handle_t* Device);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_LED_INC_LED_DRIVER_H_ */
