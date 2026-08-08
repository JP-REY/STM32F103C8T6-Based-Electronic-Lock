/**********************************************************************************************************************************
 * @file    Buzzer_Driver.c
 * @brief   Buzzer_Driver.h module implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Ago 08, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Buzzer_Driver.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief  	Default PWM duty cycle percentage used by the buzzer.
 **********************************************************************************************************************************/
#define BUZZER_DEFAULT_DUTY_CYCLE_PERCENT 50U

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
/**********************************************************************************************************************************
 * @brief   Checks whether the buzzer instance has been successfully initialized.
 *
 * @details This function validates the buzzer handle and returns the current
 *          initialization state of the instance.
 *
 * @param   Device - Pointer to the buzzer handle instance.
 *
 * @return  true  - if the handle is valid and initialized.
 * @return  false - if the handle is NULL or has not been initialized.
 **********************************************************************************************************************************/
static inline bool Buzzer_IsInit(Buzzer_HandleTypeDef* Device)
{
    return Device == NULL ? false : Device->_initialized;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Initializes a buzzer driver instance.
 *
 * @details The Buzzer Driver does not create or allocate a PWM instance.
 *          Instead, it receives a pointer to an externally created
 *          PWM_HandleTypeDef through the Context parameter.
 *
 *          The supplied PWM instance must already contain a valid platform
 *          context associated with the target timer and PWM channel.
 *
 *          During initialization, the driver:
 *          - stores the supplied PWM handle as its internal context;
 *          - initializes the PWM instance through the PWM Platform Interface;
 *          - configures the default buzzer duty cycle;
 *          - marks the buzzer instance as initialized.
 *
 *          The Buzzer Driver does not take ownership of the supplied PWM
 *          instance. The caller is responsible for maintaining the lifetime
 *          and validity of the PWM handle while the buzzer is in use.
 *
 * @param   Device  - Pointer to the buzzer handle instance.
 * @param   Context - Pointer to a previously created PWM_HandleTypeDef.
 *
 * @warning The PWM handle must have a valid platform context associated with
 *          a timer and PWM channel before calling this function.
 *
 * @note    The PWM handle lifetime must be greater than or equal to the
 *          lifetime of the Buzzer Driver instance.
 *
 * @return  BUZZER_OPERATION_OK   - if initialization succeeds.
 * @return  BUZZER_OPERATION_FAIL - if any parameter or PWM configuration
 **********************************************************************************************************************************/
Buzzer_OpStatusTypeDef Buzzer_Init(Buzzer_HandleTypeDef* Device, PWM_HandleTypeDef* Context)
{
    if(Device == NULL || Context == NULL)
    {
        return BUZZER_OPERATION_FAIL;
    }

    if(Buzzer_IsInit(Device))
    {
        return BUZZER_OPERATION_OK;
    }

    Device->_context = Context;

    if(Device->_context->Ctx == NULL)

        return BUZZER_OPERATION_FAIL;

    if(PPWM_Init(Device->_context) != PWM_OPERATION_OK)

        return BUZZER_OPERATION_FAIL;

    if(PPWM_SetDutyPercent(Device->_context, BUZZER_DEFAULT_DUTY_CYCLE_PERCENT) != PWM_OPERATION_OK)

        return BUZZER_OPERATION_FAIL;

    Device->_initialized = true;

    return BUZZER_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Sets the buzzer operating frequency.
 *
 * @details The requested frequency is forwarded to the PWM Platform
 *          Interface associated with the buzzer instance.
 *
 *          The Buzzer Driver does not directly access timer registers or
 *          perform MCU-specific PWM calculations.
 *
 * @param   Device    - Pointer to the initialized buzzer handle instance.
 * @param   Frequency - Desired buzzer frequency in hertz.
 *
 * @return  BUZZER_OPERATION_OK   - if the frequency is successfully configured.
 * @return  BUZZER_OPERATION_FAIL - if the handle is invalid, the buzzer is not
 *                                  initialized, or the underlying PWM operation fails.
 **********************************************************************************************************************************/
Buzzer_OpStatusTypeDef Buzzer_SetFrequency(Buzzer_HandleTypeDef* Device, uint32_t Frequency)
{
    if(Device == NULL || !Buzzer_IsInit(Device))
    {
        return BUZZER_OPERATION_FAIL;
    }

    if(PPWM_SetFrequency(Device->_context, Frequency) != PWM_OPERATION_OK)

        return BUZZER_OPERATION_FAIL;

    return BUZZER_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Enables the buzzer output.
 *
 * @details Enables the PWM channel associated with the buzzer instance.
 *          The previously configured frequency and duty cycle are preserved.
 *
 * @param   Device - Pointer to the initialized buzzer handle instance.
 *
 * @return  BUZZER_OPERATION_OK   - if the buzzer output is successfully enabled.
 * @return  BUZZER_OPERATION_FAIL - if the handle is invalid, the buzzer is not
 *                                  initialized, or the underlying PWM operation fails.
 **********************************************************************************************************************************/
Buzzer_OpStatusTypeDef Buzzer_On(Buzzer_HandleTypeDef* Device)
{
    if(Device == NULL || !Buzzer_IsInit(Device))
    {
        return BUZZER_OPERATION_FAIL;
    }

    if(PPWM_Enable(Device->_context) != PWM_OPERATION_OK)

        return BUZZER_OPERATION_FAIL;

    return BUZZER_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Disables the buzzer output.
 *
 * @details Disables the PWM channel associated with the buzzer instance.
 *          The configured frequency and duty cycle are preserved and can be
 *          reused when the buzzer is enabled again.
 *
 * @param   Device - Pointer to the initialized buzzer handle instance.
 *
 * @return  BUZZER_OPERATION_OK   - if the buzzer output is successfully enabled.
 * @return  BUZZER_OPERATION_FAIL - if the handle is invalid, the buzzer is not
 *                                  initialized, or the underlying PWM operation fails.
 **********************************************************************************************************************************/
Buzzer_OpStatusTypeDef Buzzer_Off(Buzzer_HandleTypeDef* Device)
{
    if(Device == NULL || !Buzzer_IsInit(Device))
    {
        return BUZZER_OPERATION_FAIL;
    }

    if(PPWM_Disable(Device->_context) != PWM_OPERATION_OK)

        return BUZZER_OPERATION_FAIL;

    return BUZZER_OPERATION_OK;
}
