/**********************************************************************************************************************************
 * @file    PWM_Platform_Interface.h
 * @brief   Platform abstraction interface for Pulse Width Modulation (PWM).
 *
 * @details This module defines a generic Platform Abstraction Layer (PAL) for PWM peripherals.
 *          It provides a hardware-independent interface used to configure and control PWM output
 *          channels while hiding all microcontroller-specific implementation details.
 *
 *          The module follows the Platform Interface design adopted throughout this project:
 *              - The application interacts only with this API.
 *              - The implementation is platform specific.
 *              - Platform-dependent objects are hidden through an opaque Context pointer.
 *
 * @note    All public functions are prefixed with PPWM (Platform PWM).
 *
 * @note    Platform PWM V1 limitations:
 *              - Timer clock is assumed to be equal to the system clock.
 *              - Prescaler (PSC) is preserved during frequency updates.
 *              - Frequency adjustment is performed only by modifying ARR.
 *              - Only 16-bit general-purpose timers are currently supported.
 *              - Multiple PWM channels sharing the same timer also share the configured frequency.
 *
 * @warning On platforms where multiple PWM channels share the same timer
 *          (e.g. STM32 TIM2-TIM5), the PWM frequency is a timer-wide property.
 *          Consequently, changing the frequency of one channel affects every
 *          channel belonging to the same hardware timer.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Jul 29, 2026
 **********************************************************************************************************************************/

#ifndef INC_PWM_PLATFORM_INTERFACE_H_
#define INC_PWM_PLATFORM_INTERFACE_H_

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
/**********************************************************************************************************************************
 * @brief   Defines the operation status returned by the Platform PWM interface.
 *
 * @details This enumeration represents the execution result of a Platform PWM
 *          operation. It is returned by all functions capable of succeeding
 *          or failing due to invalid parameters or platform-specific errors.
 *
 * @note    Every Platform PWM function returning this type shall return
 *          PWM_OPERATION_OK when the requested operation completes
 *          successfully.
 **********************************************************************************************************************************/
typedef enum
{
    PWM_OPERATION_OK = 0U,
    PWM_OPERATION_FAIL

} PWM_OpStatusTypeDef;

/**********************************************************************************************************************************
 * @brief   Identifies a PWM output channel.
 *
 * @details This enumeration specifies which hardware Output Compare (OC)
 *          channel is associated with a Platform PWM instance.
 *
 *          The actual hardware mapping is platform dependent.
 *
 *          Example (STM32):
 *
 *              PWM_CHANNEL_1 -> TIMx_CCR1
 *              PWM_CHANNEL_2 -> TIMx_CCR2
 *              PWM_CHANNEL_3 -> TIMx_CCR3
 *              PWM_CHANNEL_4 -> TIMx_CCR4
 *
 * @note    Not every platform is required to support all four channels.
 **********************************************************************************************************************************/
typedef enum
{
    PWM_CHANNEL_1 = (uint32_t) 0x00000000U,
    PWM_CHANNEL_2 = (uint32_t) 0x00000004U,
    PWM_CHANNEL_3 = (uint32_t) 0x00000008U,
    PWM_CHANNEL_4 = (uint32_t) 0x0000000CU

} PWM_ChannelTypeDef;

/**********************************************************************************************************************************
 * @brief   Defines the active output polarity of a PWM signal.
 *
 * @details The selected polarity determines whether the active portion of the
 *          PWM waveform is represented by a logic HIGH or logic LOW level.
 *
 *          The actual hardware implementation is platform dependent.
 *
 * @note    On STM32 devices this configuration typically maps to the CCxP bit
 *          of the TIMx_CCER register.
 **********************************************************************************************************************************/
typedef enum
{
    PWM_POLARITY_HIGH = 0U,
    PWM_POLARITY_LOW

} PWM_PolarityTypeDef;

/**********************************************************************************************************************************
 * @brief   Defines the current operation state of a PWM signal.
 *
 * @details The selected state determines whether the PWM channel is enabled
 *          (running) or disable (idle)
 *
 *          The actual hardware implementation is platform dependent.
 *
 * @note    On STM32 devices this configuration typically maps to the CCxP bit
 *          of the TIMx_CCER register.
 **********************************************************************************************************************************/
typedef enum
{
    PWM_STATE_ENABLED,
    PWM_STATE_DISABLED

}PWM_StateTypeDef;

/**********************************************************************************************************************************
 * @brief   Platform PWM instance.
 *
 * @details This structure represents a single logical PWM output channel.
 *
 *          Besides storing the logical configuration of the PWM instance, the
 *          structure also contains an opaque platform-specific context used by
 *          the implementation to access the underlying hardware peripheral.
 *
 *          Application code shall never access or modify any member directly
 *          after initialization. Runtime configuration shall always be
 *          performed through the Platform PWM API.
 *
 * @note    On platforms where multiple channels share the same timer
 *          peripheral (e.g. STM32 TIM2-TIM5), changing the PWM frequency
 *          affects every channel associated with that timer.
 **********************************************************************************************************************************/
typedef struct
{
    /**************************************************************************************************
     * @brief   Platform-specific context.
     *
     * @details Opaque pointer reserved for the Platform PWM implementation.
     *          This member typically stores the native peripheral handle
     *          required by the underlying platform.
     **************************************************************************************************/
    void* Ctx;

    /* << Private data. Do not read or modify!               >> */
    /* << PWM output channel                                 >> */ PWM_ChannelTypeDef  _channel;
    /* << PWM output polarity                                >> */ PWM_PolarityTypeDef _polarity;
    /* << Current compare register value (TIMx_CCRx)         >> */ uint16_t            _duty;
    /* << Max duty value according to TIMx_ARR register      >> */ uint16_t            _max_duty;
    /* << Configured PWM frequency                           >> */ uint32_t            _frequency;
    /* << Indicates if the PWM instance has been initialized >> */ bool                _initialized;
    /* << Indicates if PWM generation is currently enabled.  >> */ bool                _started;

}PWM_HandleTypeDef;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
PWM_OpStatusTypeDef PPWM_Create         (PWM_HandleTypeDef* Instance, void* Context, PWM_ChannelTypeDef Channel);
PWM_OpStatusTypeDef PPWM_Init           (PWM_HandleTypeDef* Instance);
PWM_OpStatusTypeDef PPWM_Enable         (PWM_HandleTypeDef* Instance);
PWM_OpStatusTypeDef PPWM_Disable        (PWM_HandleTypeDef* Instance);
PWM_OpStatusTypeDef PPWM_SetDutyVal     (PWM_HandleTypeDef* Instance, uint16_t Duty);
PWM_OpStatusTypeDef PPWM_SetDutyPercent (PWM_HandleTypeDef* Instance, uint16_t Duty_Percent);
uint16_t            PPWM_GetDutyVal     (const PWM_HandleTypeDef* Instance);
uint16_t            PPWM_GetDutyPercent (const PWM_HandleTypeDef* Instance);
uint16_t            PPWM_GetMaxDuty     (const PWM_HandleTypeDef* Instance);
PWM_OpStatusTypeDef PPWM_SetFrequency   (PWM_HandleTypeDef* Instance, uint32_t Frequency);
uint32_t            PPWM_GetFrequency   (const PWM_HandleTypeDef* Instance);
PWM_OpStatusTypeDef PPWM_SetPolarity    (PWM_HandleTypeDef* Instance, PWM_PolarityTypeDef Polarity);
PWM_PolarityTypeDef PPWM_GetPolarity    (const PWM_HandleTypeDef* Instance);
PWM_StateTypeDef    PPWM_GetState       (const PWM_HandleTypeDef* Instance);


#ifdef __cplusplus
}
#endif

#endif /* INC_PWM_PLATFORM_INTERFACE_H_ */
