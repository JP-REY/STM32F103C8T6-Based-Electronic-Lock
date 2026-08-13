/**********************************************************************************************************************************
 * @file    PWM_Platform_Interface.c
 * @brief   Describe this module implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Jul 29, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "PWM_Platform_Interface.h"
#include "stm32f4xx.h"
#include "tim.h"
/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
#define PPWM_MAX_DUTY_PERCENT     (uint16_t)100U
#define PPWM_MIN_DUTY_PERCENT     (uint16_t)0U
#define PPWM_MIN_DUTY_VAL         (uint16_t)0U
#define TIMER_16BIT_MAX_PERIOD    (uint16_t)0xFFFFU
#define TIM_CCER_CC1P_BIT_MASK    (uint32_t)0x00000002
#define TIM_CCER_CC2P_BIT_MASK    (uint32_t)0x00000020
#define TIM_CCER_CC3P_BIT_MASK    (uint32_t)0x00000100
#define TIM_CCER_CC4P_BIT_MASK    (uint32_t)0x00002000
#define TIM_CR1_UDS_BIT_MASK      (uint32_t)0x00000002

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
/* << Helper Functions >> */
/**********************************************************************************************************************************
 * @brief   Checks whether a Platform PWM instance has been initialized.
 *
 * @details Returns the current initialization state of the specified
 *          Platform PWM instance.
 *
 *          This helper function is used internally by the Platform PWM API
 *          to validate that PPWM_Init() has been succesfully called before
 *          executing operations that require an initialized instance.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @return  true   - if the PWM instance has been initialized.
 * @return  false  - if the PWM instance has not been initialized.
 **********************************************************************************************************************************/
static inline bool PPWM_IsInit(const PWM_Handle_t* Instance)
{
    return Instance == NULL ? false : Instance->_initialized;
}

/**********************************************************************************************************************************
 * @brief   Clamps a duty cycle value to the specified maximum value.
 *
 * @details Returns the specified duty cycle value if it does not exceed
 *          the specified maximum value. Otherwise, the maximum value is
 *          returned.
 *
 * @param   Duty      - Duty cycle value to be validated.
 * @param   Max_Value - Maximum allowed duty cycle value.
 *
 * @return  A duty cycle value guaranteed not to exceed Max_Value.
 **********************************************************************************************************************************/
static inline uint16_t PPWM_ClampDutyValue(uint16_t Duty, uint16_t Max_Value)
{
    return Duty > Max_Value ? Max_Value : Duty;
}

/**********************************************************************************************************************************
 * @brief   Maps a duty cycle percentage to a timer compare register (CCRx) value.
 *
 * @details Performs a linear interpolation between two numeric ranges,
 *          converting a duty cycle percentage into the corresponding timer
 *          compare register (CCRx) value.
 *
 *          If the specified duty cycle percentage exceeds Duty_Max_Percent,
 *          it is automatically clamped to the maximum allowed percentage
 *          before performing the conversion.
 *
 * @param   Duty_Percent     - Duty cycle percentage to be converted.
 * @param   Duty_Min_Percent - Lower limit of the input duty cycle percentage range.
 * @param   Duty_Min_Val     - Minimum output compare register value.
 * @param   Duty_Max_Percent - Upper limit of the input duty cycle percentage range.
 * @param   Duty_Max_Val     - Maximum output compare register value.
 *
 * @pre     Duty_Max_Percent shall be greater than Duty_Min_Percent.
 *
 * @return  Timer compare register (CCRx) value obtained through linear
 *          interpolation between the specified input and output ranges.
 **********************************************************************************************************************************/
static inline uint16_t PPWM_MapDutyPercentToValue(uint16_t Duty_Percent, uint16_t Duty_Min_Percent, uint16_t Duty_Min_Val,
                                                  uint16_t Duty_Max_Percent, uint16_t Duty_Max_Val)
{
    Duty_Percent = PPWM_ClampDutyValue(Duty_Percent, PPWM_MAX_DUTY_PERCENT);

    return (Duty_Percent - Duty_Min_Percent) * (Duty_Max_Val - Duty_Min_Val) /
           (Duty_Max_Percent - Duty_Min_Percent) + Duty_Min_Val;
}

/**********************************************************************************************************************************
 * @brief   Maps a compare register value to a duty cycle percentage.
 *
 * @details Performs a linear interpolation between two numeric ranges,
 *          converting a timer compare register value into the corresponding
 *          duty cycle percentage.
 *
 *          If the specified compare register value exceeds Duty_Max_Val, it
 *          is automatically clamped to the maximum allowed value before
 *          performing the conversion.
 *
 * @param   Duty_Val         - Compare register value to be converted.
 * @param   Duty_Min_Val     - Lower limit of the input compare register range.
 * @param   Duty_Min_Percent - Minimum output duty cycle percentage.
 * @param   Duty_Max_Val     - Upper limit of the input compare register range.
 * @param   Duty_Max_Percent - Maximum output duty cycle percentage.
 *
 * @pre     Duty_Max_Val shall be greater than Duty_Min_Val.
 *
 * @return  Duty cycle percentage obtained through linear interpolation
 *          between the specified input and output ranges.
 **********************************************************************************************************************************/
static inline uint16_t PPWM_MapDutyValueToPercent(uint16_t Duty_Val, uint16_t Duty_Min_Val, uint16_t Duty_Min_Percent,
                                                  uint16_t Duty_Max_Val, uint16_t Duty_Max_Percent)
{
    Duty_Val = PPWM_ClampDutyValue(Duty_Val, Duty_Max_Val);

    return (Duty_Val - Duty_Min_Val) * (Duty_Max_Percent - Duty_Min_Percent) /
           (Duty_Max_Val - Duty_Min_Val) + Duty_Min_Percent;
}

/**********************************************************************************************************************************
 * @brief   Returns the address of the timer compare register (CCRx) for the specified PWM channel.
 *
 * @details Resolves the hardware compare register (CCRx) associated with the
 *          specified PWM output channel and returns its memory address.
 *
 *          This helper function abstracts the channel-specific register
 *          mapping, allowing the remaining module implementation to access
 *          the compare register through a common interface.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 * @param   Channel  - PWM output channel.
 *
 * @return  Pointer to the corresponding timer compare register (CCRx).
 * @return  NULL if the instance, platform context or channel is invalid.
 **********************************************************************************************************************************/
static inline volatile uint32_t* PPWM_GetCCRxRegister(PWM_Handle_t* Instance, PWM_Channel_t Channel)
{
    if(Instance == NULL)
    {
        return NULL;
    }

    TIM_HandleTypeDef* ctx = (TIM_HandleTypeDef*)Instance->Ctx;

    if(ctx == NULL)
    {
        return NULL;
    }

    switch(Channel)
    {
        case PWM_CHANNEL_1: return &(ctx->Instance->CCR1);
        case PWM_CHANNEL_2: return &(ctx->Instance->CCR2);
        case PWM_CHANNEL_3: return &(ctx->Instance->CCR3);
        case PWM_CHANNEL_4: return &(ctx->Instance->CCR4);

        default: return NULL;
    }
}

/**********************************************************************************************************************************
 * @brief   Returns the CCxP bit mask corresponding to a PWM channel.
 *
 * @details Maps the specified PWM output channel to its corresponding CCxP
 *          bit mask within the timer capture/compare enable register (CCER).
 *
 *          The returned bit mask can be used to access or modify the output
 *          polarity configuration of the selected PWM channel.
 *
 * @param   Channel - PWM output channel.
 *
 * @return  CCxP bit mask corresponding to the specified PWM channel.
 * @return  0 if the specified channel is invalid.
 **********************************************************************************************************************************/
static inline uint32_t PPWM_GetCCxPBitMask(PWM_Channel_t Channel)
{
    switch(Channel)
    {
        case PWM_CHANNEL_1: return TIM_CCER_CC1P_BIT_MASK;
        case PWM_CHANNEL_2: return TIM_CCER_CC2P_BIT_MASK;
        case PWM_CHANNEL_3: return TIM_CCER_CC3P_BIT_MASK;
        case PWM_CHANNEL_4: return TIM_CCER_CC4P_BIT_MASK;

        default: return 0;
    }
}

/**********************************************************************************************************************************
 * @brief   Returns the address of the timer auto-reload register (ARR).
 *
 * @details Resolves the hardware auto-reload register (ARR) associated with
 *          the timer used by the specified Platform PWM instance and returns
 *          its memory address.
 *
 *          This helper function abstracts direct access to the timer
 *          auto-reload register from the remaining module implementation.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @return  Pointer to the timer auto-reload register (ARR).
 * @return  NULL if the instance or platform context is invalid.
 **********************************************************************************************************************************/
static inline volatile uint32_t* PPWM_GetARRRegister(PWM_Handle_t* Instance)
{
    if(Instance == NULL)
    {
        return NULL;
    }

    TIM_HandleTypeDef* ctx = (TIM_HandleTypeDef*)Instance->Ctx;

    if(ctx == NULL)
    {
        return NULL;
    }

    return &(ctx->Instance->ARR);
}

/**********************************************************************************************************************************
 * @brief   Returns the address of the timer prescaler register (PSC).
 *
 * @details Resolves the hardware prescaler register (PSC) associated with
 *          the timer used by the specified Platform PWM instance and returns
 *          its memory address.
 *
 *          This helper function abstracts direct access to the timer
 *          prescaler register from the remaining module implementation.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @return  Pointer to the timer prescaler register (PSC).
 * @return  NULL if the instance or platform context is invalid.
 **********************************************************************************************************************************/
static inline volatile uint32_t* PPWM_GetPSCRegister(PWM_Handle_t* Instance)
{
    if(Instance == NULL)
    {
        return NULL;
    }

    TIM_HandleTypeDef* ctx = (TIM_HandleTypeDef*)Instance->Ctx;

    if(ctx == NULL)
    {
        return NULL;
    }

    return &(ctx->Instance->PSC);
}

/**********************************************************************************************************************************
 * @brief   Returns the address of the timer capture/compare enable register (CCER).
 *
 * @details Resolves the hardware capture/compare enable register (CCER)
 *          associated with the timer used by the specified Platform PWM
 *          instance and returns its memory address.
 *
 *          This helper function abstracts direct access to the timer
 *          capture/compare enable register from the remaining module
 *          implementation.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @return  Pointer to the timer capture/compare enable register (CCER).
 * @return  NULL if the instance or platform context is invalid.
 **********************************************************************************************************************************/
static inline volatile uint32_t* PPWM_GetCCERRegister(PWM_Handle_t* Instance)
{
    if(Instance == NULL)
    {
        return NULL;
    }

    TIM_HandleTypeDef* ctx = (TIM_HandleTypeDef*)Instance->Ctx;

    if(ctx == NULL)
    {
        return NULL;
    }

    return &(ctx->Instance->CCER);
}

/**********************************************************************************************************************************
 * @brief   Returns the timer input clock frequency.
 *
 * @details Provides the clock frequency driving the timer peripheral used by
 *          this Platform PWM implementation.
 *
 *          In the current Platform PWM V1 implementation, the timer clock is
 *          assumed to be equal to the system clock frequency configured by
 *          the platform.
 *
 * @note    This implementation is platform specific and does not account for
 *          timer clock multipliers or independent peripheral clock trees.
 *
 * @warning Future platform revisions should determine the timer clock from
 *          the actual timer instance and RCC clock configuration.
 *
 * @return  Timer input clock frequency in hertz (Hz).
 **********************************************************************************************************************************/
static inline uint32_t PPWM_GetTimerClockFreq()
{
    return HAL_RCC_GetSysClockFreq();
}

/**********************************************************************************************************************************
 * @brief   Refreshes the PWM duty cycle after a timer period update.
 *
 * @details Recalculates the timer compare register (CCRx) value according to
 *          the current timer auto-reload register (ARR), preserving the
 *          previously configured duty cycle ratio.
 *
 *          Upon successful completion, both the hardware compare register and
 *          the internal Platform PWM duty cycle state are synchronized with
 *          the refreshed duty cycle value.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @note    This helper function is intended for internal use after updating
 *          the timer period (ARR).
 *
 *  @note   The refreshed duty cycle value is computed using integer arithmetic.
 *          As a result, the recalculated compare register value may differ
 *          slightly from the ideal value due to quantization effects.
 *
 * @warning This function assumes that the current compare register value was
 *          configured for the previous timer period.
 *
 * @return  PWM_OPERATION_OK   - Duty cycle successfully refreshed.
 * @return  PWM_OPERATION_FAIL - Invalid instance, platform context or
 *                               timer register access.
 **********************************************************************************************************************************/
static inline PWM_OpStatus_t PPWM_RefreshDuty(PWM_Handle_t* Instance)
{
    if(Instance == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    TIM_HandleTypeDef* ctx = (TIM_HandleTypeDef*)Instance->Ctx;

    if(ctx == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    volatile uint32_t* arr_reg = PPWM_GetARRRegister(Instance);
    volatile uint32_t* ccrx_reg = PPWM_GetCCRxRegister(Instance, Instance->_channel);

    if(arr_reg == NULL || ccrx_reg == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    uint16_t old_max_duty = Instance->_max_duty;

    /* << Updates duty max value based on the new arr register value >> */
    Instance->_max_duty = *arr_reg;

    /* << Rescales the compare register value to preserve the duty cycle ratio >>  */
    uint16_t duty_refreshed = ((*ccrx_reg) * (Instance->_max_duty) + (*ccrx_reg)) / (old_max_duty);

    /* << Applies refreshed duty value >> */
    *ccrx_reg = duty_refreshed;

    Instance->_duty = duty_refreshed;

    return PWM_OPERATION_OK;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Creates and configures a Platform PWM instance.
 *
 * @details Initializes a Platform PWM handle by associating it with a
 *          platform-specific PWM peripheral and the selected output channel.
 *
 *          During creation, the function reads the current hardware
 *          configuration to initialize the internal software state,
 *          including:
 *
 *              - PWM output channel;
 *              - Output polarity;
 *              - Current compare register value;
 *              - Maximum compare register value;
 *              - Configured PWM frequency.
 *
 *          The current hardware configuration is mirrored into the internal
 *          software state.
 *
 *          This function does not initialize or enable the PWM peripheral.
 *          After creation, PPWM_Init() shall be called before any runtime
 *          operation is performed.
 *
 * @param   Instance - Pointer to the Platform PWM instance to be created.
 * @param   Context  - Pointer to the platform-specific PWM peripheral context.
 * @param   Channel  - Hardware PWM output channel associated with the instance.
 *
 * @note    The Context pointer is platform dependent and typically refers to
 *          the native PWM peripheral handle (e.g. TIM_HandleTypeDef on STM32
 *          platforms).
 *
 * @note    This function assumes that the underlying PWM peripheral has
 *          already been configured by the platform initialization code.
 *
 * @return  PWM_OPERATION_OK   - PWM instance successfully created.
 * @return  PWM_OPERATION_FAIL - Invalid parameter or unsupported platform configuration.
 **********************************************************************************************************************************/
PWM_OpStatus_t PPWM_Create(PWM_Handle_t* Instance, void* Context, PWM_Channel_t Channel)
{
    TIM_HandleTypeDef* ctx = (TIM_HandleTypeDef*)Context;

    uint32_t timer_clk_freq = PPWM_GetTimerClockFreq();

    if(Instance == NULL ||  ctx == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    /* << PWM Channel initialization >> */
    Instance->Ctx       = ctx;
    Instance->_channel  = Channel;

    volatile uint32_t* arr_reg  = PPWM_GetARRRegister(Instance);
    volatile uint32_t* psc_reg  = PPWM_GetPSCRegister(Instance);
    volatile uint32_t* ccer_reg = PPWM_GetCCERRegister(Instance);
    volatile uint32_t* ccrx_reg = PPWM_GetCCRxRegister(Instance, Instance->_channel);

    Instance->_duty      = *ccrx_reg;
    Instance->_polarity  = (PWM_Polarity_t)(READ_BIT(*ccer_reg ,PPWM_GetCCxPBitMask(Channel)));

    /* << PWM frequency calculation based on register values pre-configured by Cube MX >> */
    Instance->_frequency = ((timer_clk_freq)/((*psc_reg + 1U) * (*arr_reg + 1U)));

    Instance->_max_duty     = *arr_reg;
    Instance->_initialized  = false;
    Instance->_started      = false;

    return PWM_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Initializes a Platform PWM instance.
 *
 * @details Marks the specified Platform PWM instance as initialized,
 *          allowing subsequent runtime operations provided by this module.
 *
 *          This function performs the software initialization of the PWM
 *          instance only. It does not configure or modify the underlying
 *          hardware peripheral.
 *
 *          If the specified instance has already been initialized, the
 *          function returns immediately without performing any additional
 *          operation.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @warning PPWM_Create() shall be successfully called before this function.
 *
 * @return  PWM_OPERATION_OK   - The instance was successfully initialized or was already initialized.
 * @return  PWM_OPERATION_FAIL - Instance is NULL.
 **********************************************************************************************************************************/
PWM_OpStatus_t PPWM_Init(PWM_Handle_t* Instance)
{
    if(Instance == NULL) return PWM_OPERATION_FAIL;

    if(PPWM_IsInit(Instance)) return PWM_OPERATION_OK;

    Instance->_initialized = true;

    return PWM_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Enables PWM signal generation for the specified output channel.
 *
 * @details Starts PWM waveform generation on the associated hardware channel.
 *
 *          If the PWM instance is already enabled, the function returns
 *          immediately without performing any additional operation.
 *
 *          The internal operation state is updated only if the underlying
 *          platform successfully enables PWM generation.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @warning The PWM instance shall be successfully initialized by
 *          PPWM_Init() before calling this function.
 *
 * @return  PWM_OPERATION_OK   - PWM generation successfully enabled or already enabled.
 * @return  PWM_OPERATION_FAIL - Invalid instance or platform-specific enable operation failed.
 **********************************************************************************************************************************/
PWM_OpStatus_t PPWM_Enable(PWM_Handle_t* Instance)
{
    if(Instance == NULL || !(PPWM_IsInit(Instance)))
    {
        return PWM_OPERATION_FAIL;
    }

    if(Instance->_started) return PWM_OPERATION_OK;


    if(HAL_TIM_PWM_Start(Instance->Ctx, Instance->_channel) != HAL_OK)
    {
        return PWM_OPERATION_FAIL;
    }

    Instance->_started = true;

    return PWM_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Disables PWM signal generation for the specified output channel.
 *
 * @details Stops PWM waveform generation on the associated hardware channel.
 *
 *          If the PWM instance is already disabled, the function returns
 *          immediately without performing any additional operation.
 *
 *          The internal operation state is updated only if the underlying
 *          platform successfully disables PWM generation.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @warning The PWM instance shall be successfully initialized by
 *          PPWM_Init() before calling this function.
 *
 * @return  PWM_OPERATION_OK   - PWM generation successfully disabled or already disabled.
 * @return  PWM_OPERATION_FAIL - Invalid instance or platform-specific disable operation failed.
 **********************************************************************************************************************************/
PWM_OpStatus_t PPWM_Disable(PWM_Handle_t* Instance)
{
    if(Instance == NULL || !(PPWM_IsInit(Instance)))
    {
        return PWM_OPERATION_FAIL;
    }

    if(!(Instance->_started)) return PWM_OPERATION_OK;

    if(HAL_TIM_PWM_Stop(Instance->Ctx, Instance->_channel) != HAL_OK)
    {
        return PWM_OPERATION_FAIL;
    }

    Instance->_started = false;

    return PWM_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Sets the PWM duty cycle using a compare register value.
 *
 * @details Updates the duty cycle by writing the specified compare value to
 *          the corresponding hardware compare register associated with the
 *          Platform PWM instance.
 *
 *          If the specified duty value exceeds the maximum compare register
 *          value supported by the configured timer period, it is
 *          automatically clamped before being applied.
 *
 *          Upon successful completion, the internal duty cycle state is
 *          synchronized with the value written to the hardware.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 * @param   Duty     - Compare register value to be applied.
 *
 * @warning The PWM instance shall be successfully initialized by
 *          PPWM_Init() before calling this function.
 *
 * @return  PWM_OPERATION_OK   - Duty cycle successfully updated.
 * @return  PWM_OPERATION_FAIL - Invalid instance or uninitialized PWM instance.
 **********************************************************************************************************************************/
PWM_OpStatus_t PPWM_SetDutyVal(PWM_Handle_t* Instance, uint16_t Duty)
{
    if(Instance == NULL || !(PPWM_IsInit(Instance)))
    {
        return PWM_OPERATION_FAIL;
    }

    volatile uint32_t* ccrx_reg = PPWM_GetCCRxRegister(Instance, Instance->_channel);

    if(ccrx_reg == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    TIM_HandleTypeDef* ctx = (TIM_HandleTypeDef*)Instance->Ctx;

    if(ctx == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    Duty = PPWM_ClampDutyValue(Duty, PPWM_GetMaxDuty(Instance));

    *ccrx_reg = Duty;

    Instance->_duty = Duty;

    return PWM_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Sets the PWM duty cycle as a percentage.
 *
 * @details Converts the specified duty cycle percentage into the equivalent
 *          compare register value according to the current timer period and
 *          applies it to the corresponding PWM output channel.
 *
 *          If the specified percentage exceeds the supported range, it is
 *          automatically clamped before the conversion is performed.
 *
 *          Upon successful completion, the internal duty cycle state is
 *          synchronized with the value written to the hardware.
 *
 * @param   Instance      - Pointer to the Platform PWM instance.
 * @param   Duty_Percent  - Duty cycle percentage to be applied.
 *
 * @warning The PWM instance shall be successfully initialized by
 *          PPWM_Init() before calling this function.
 *
 * @return  PWM_OPERATION_OK   - Duty cycle successfully updated.
 * @return  PWM_OPERATION_FAIL - Invalid instance or uninitialized PWM instance.
 **********************************************************************************************************************************/
PWM_OpStatus_t PPWM_SetDutyPercent(PWM_Handle_t* Instance, uint16_t Duty_Percent)
{
    if(Instance == NULL || !(PPWM_IsInit(Instance)))
    {
        return PWM_OPERATION_FAIL;
    }

    volatile uint32_t* ccrx_reg = PPWM_GetCCRxRegister(Instance, Instance->_channel);

    if(ccrx_reg == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    TIM_HandleTypeDef* ctx = (TIM_HandleTypeDef*)Instance->Ctx;

    if(ctx == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    uint16_t max_duty = PPWM_GetMaxDuty(Instance);

    uint16_t duty_val = PPWM_MapDutyPercentToValue(Duty_Percent, PPWM_MIN_DUTY_PERCENT, 0U, PPWM_MAX_DUTY_PERCENT, max_duty);

    *ccrx_reg = duty_val;

    Instance->_duty = duty_val;

    return PWM_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Returns the current PWM duty cycle compare value.
 *
 * @details Retrieves the compare register value currently stored by the
 *          Platform PWM instance.
 *
 *          The returned value corresponds to the last duty cycle successfully
 *          applied through the Platform PWM interface.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @note    The PWM instance shall be successfully initialized by
 *          PPWM_Init() before calling this function.
 *
 * @return  Current compare register value.
 *
 * @warning Returns 0 if the specified instance is NULL or has not been initialized.
 **********************************************************************************************************************************/
uint16_t PPWM_GetDutyVal(const PWM_Handle_t* Instance)
{
    if(Instance == NULL || !(PPWM_IsInit(Instance)))
    {
        return 0;
    }

    return Instance->_duty;
}

/**********************************************************************************************************************************
 * @brief   Returns the current PWM duty cycle percent.
 *
 * @details Map the compare register value currently stored by the
 *          Platform PWM instance to percent value.
 *
 *          The returned value corresponds to the last duty cycle percent
 *          successfully applied through the Platform PWM interface.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @note    The PWM instance shall be successfully initialized by
 *          PPWM_Init() before calling this function.
 *
 * @return  Current duty cycle percent value.
 *
 * @warning Returns 0 if the specified instance is NULL or has not been initialized.
 **********************************************************************************************************************************/
uint16_t PPWM_GetDutyPercent(const PWM_Handle_t* Instance)
{
    if(Instance == NULL || !(PPWM_IsInit(Instance)))
    {
        return 0;
    }

    return PPWM_MapDutyValueToPercent(Instance->_duty, PPWM_MIN_DUTY_VAL,
                                      PPWM_MIN_DUTY_PERCENT, Instance->_max_duty,
                                      PPWM_MAX_DUTY_PERCENT);
}

/**********************************************************************************************************************************
 * @brief   Returns the maximum duty cycle value supported by the PWM instance.
 *
 * @details Retrieves the maximum compare register value that can be applied
 *          to the associated PWM output channel.
 *
 *          This value is determined by the configured timer period and
 *          defines the upper limit accepted by PPWM_SetDutyVal().
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @note    The PWM instance shall be successfully initialized by
 *          PPWM_Init() before calling this function.
 *
 * @return  Maximum compare register value supported by the configured PWM
 *          instance.
 *
 * @warning Returns 0 if the specified instance is NULL or has not been initialized.
 **********************************************************************************************************************************/
uint16_t PPWM_GetMaxDuty(const PWM_Handle_t* Instance)
{
    if(Instance == NULL || !(PPWM_IsInit(Instance)))
    {
        return 0;
    }

    TIM_HandleTypeDef* ctx = (TIM_HandleTypeDef*)Instance->Ctx;

    if(ctx == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    return ctx->Instance->ARR;
}

/**********************************************************************************************************************************
 * @brief   Sets the PWM signal frequency.
 *
 * @details Configures the PWM output frequency by updating the timer auto-reload
 *          register (ARR) while preserving the current timer prescaler (PSC).
 *
 *          After the timer period is updated, the compare register (CCRx) is
 *          automatically rescaled to preserve the previously configured duty cycle
 *          ratio. The resulting frequency is then synchronized with the internal
 *          Platform PWM instance.
 *
 * @param   Instance  - Pointer to the Platform PWM instance.
 * @param   Frequency - Desired PWM output frequency in hertz (Hz).
 *
 * @pre     The PWM instance shall be successfully initialized by
 *          PPWM_Init() before calling this function.
 *
 * @note    This implementation preserves the timer prescaler (PSC) and
 *          updates only the auto-reload register (ARR). Therefore, the
 *          actual frequency depends on the previously configured timer
 *          prescaler (PSC).
 *
 * @note    Timer input clock calculation is platform dependent. Platform
 *          PWM V1 assumes the timer clock equals the system clock frequency.
 *
 * @warning On platforms where multiple PWM channels share the same hardware
 *          timer, changing the frequency affects every PWM channel belonging
 *          to that timer.
 *
 * @warning The duty cycle ratio is preserved using integer arithmetic.
 *          Consequently, the refreshed compare register value may differ
 *          slightly from the ideal value due to timer resolution and
 *          quantization effects.
 *
 * @warning Requested frequencies outside the range representable by the
 *          current timer configuration are rejected.
 *
 * @return  PWM_OPERATION_OK   - PWM frequency successfully updated.
 * @return  PWM_OPERATION_FAIL - Invalid parameter or unsupported timer configuration.
 **********************************************************************************************************************************/
PWM_OpStatus_t PPWM_SetFrequency(PWM_Handle_t* Instance, uint32_t Frequency)
{
    uint32_t arr = 0;

    uint32_t timer_clk_freq = PPWM_GetTimerClockFreq();

    if(Instance == NULL || !(PPWM_IsInit(Instance)))
    {
        return PWM_OPERATION_FAIL;
    }

    if(Instance->_frequency == Frequency)
    {
        return PWM_OPERATION_OK;
    }

    if(Frequency > timer_clk_freq || Frequency == 0)
    {
        return PWM_OPERATION_FAIL;
    }

    TIM_HandleTypeDef* ctx = (TIM_HandleTypeDef*)Instance->Ctx;

    if(ctx == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    volatile uint32_t* psc_reg = PPWM_GetPSCRegister(Instance);
    volatile uint32_t* arr_reg = PPWM_GetARRRegister(Instance);

    if(psc_reg == NULL || arr_reg == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    /* << Temporarily disable update events to prevent shadow register updates while ARR is being reconfigured >> */
    SET_BIT((ctx->Instance->CR1), TIM_CR1_UDS_BIT_MASK);

    arr = ((timer_clk_freq)/((*psc_reg + 1U) * Frequency));

    if(arr == 0 || arr > TIMER_16BIT_MAX_PERIOD)
    {
        CLEAR_BIT((ctx->Instance->CR1), TIM_CR1_UDS_BIT_MASK);

        return PWM_OPERATION_FAIL;
    }

    *arr_reg = (uint16_t)arr - 1U;

    CLEAR_BIT((ctx->Instance->CR1), TIM_CR1_UDS_BIT_MASK);

    ctx->Instance->EGR |= TIM_EGR_UG;

    if(PPWM_RefreshDuty(Instance) != PWM_OPERATION_OK)
    {
        return PWM_OPERATION_FAIL;
    }

    Instance->_frequency = Frequency;

    return PWM_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Returns the current PWM output frequency.
 *
 * @details Retrieves the PWM output frequency currently maintained by the
 *          Platform PWM instance.
 *
 *          The returned value corresponds to the last frequency successfully
 *          configured through PPWM_SetFrequency() or initialized during
 *          PPWM_Create().
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @pre     PPWM_Init() shall be successfully called before this function.
 *
 * @return  Current PWM output frequency in hertz (Hz).
 * @return  0 if the specified instance is NULL or has not been initialized.
 **********************************************************************************************************************************/
uint32_t PPWM_GetFrequency(const PWM_Handle_t* Instance)
{
    if(Instance == NULL || !(PPWM_IsInit(Instance)))
    {
        return PWM_OPERATION_FAIL;
    }

    return Instance->_frequency;
}

/**********************************************************************************************************************************
 * @brief   Sets the active polarity of the PWM output signal.
 *
 * @details Configures the active output polarity of the associated PWM
 *          channel.
 *
 *          Depending on the selected polarity, the active portion of the PWM
 *          waveform is represented by either a logic HIGH or logic LOW level.
 *
 *          Upon successful completion, the internal polarity state is
 *          synchronized with the hardware configuration.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 * @param   Polarity - Desired PWM output polarity.
 *
 * @warning The PWM instance shall be successfully initialized by
 *          PPWM_Init() before calling this function.
 *
 * @return  PWM_OPERATION_OK   - PWM polarity successfully updated.
 * @return  PWM_OPERATION_FAIL - Invalid instance or uninitialized PWM instance.
 **********************************************************************************************************************************/
PWM_OpStatus_t PPWM_SetPolarity(PWM_Handle_t* Instance, PWM_Polarity_t Polarity)
{
    if(Instance == NULL || !(PPWM_IsInit(Instance)))
    {
        return PWM_OPERATION_FAIL;
    }

    volatile uint32_t* ccrx_reg = PPWM_GetCCRxRegister(Instance, Instance->_channel);

    if(ccrx_reg == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    TIM_HandleTypeDef* ctx = (TIM_HandleTypeDef*)Instance->Ctx;

    if(ctx == NULL)
    {
        return PWM_OPERATION_FAIL;
    }

    uint32_t mask = PPWM_GetCCxPBitMask(Instance->_channel);

    Polarity == PWM_POLARITY_HIGH ? CLEAR_BIT((ctx->Instance->CCER), mask) :
                                    SET_BIT  ((ctx->Instance->CCER), mask) ;

    Instance->_polarity = Polarity;

    return PWM_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Returns the current PWM output polarity.
 *
 * @details Retrieves the polarity currently configured for the specified
 *          Platform PWM instance.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @note    The PWM instance shall be successfully initialized by
 *          PPWM_Init() before calling this function.
 *
 * @return  Current PWM output polarity.
 *
 * @warning Returns PWM_POLARITY_HIGH if the specified instance is NULL or has not been initialized.
 **********************************************************************************************************************************/
PWM_Polarity_t PPWM_GetPolarity(const PWM_Handle_t* Instance)
{
    if(Instance == NULL || !(PPWM_IsInit(Instance)))
    {
        return PWM_OPERATION_FAIL;
    }

    return Instance->_polarity;
}

/**********************************************************************************************************************************
 * @brief   Returns the current operating state of the PWM instance.
 *
 * @details Indicates whether PWM waveform generation is currently enabled or
 *          disabled for the specified Platform PWM instance.
 *
 *          The returned state reflects the internal software state maintained
 *          by the Platform PWM interface.
 *
 * @param   Instance - Pointer to the Platform PWM instance.
 *
 * @note    The PWM instance shall be successfully initialized by
 *          PPWM_Init() before calling this function.
 *
 * @return  PWM_STATE_ENABLED  - PWM generation is currently enabled.
 * @return  PWM_STATE_DISABLED - PWM generation is currently disabled.
 **********************************************************************************************************************************/
PWM_State_t PPWM_GetState(const PWM_Handle_t* Instance)
{
    if(!(PPWM_IsInit(Instance)))
    {
        return PWM_STATE_DISABLED;
    }

    return Instance->_started ? PWM_STATE_ENABLED : PWM_STATE_DISABLED;
}
















