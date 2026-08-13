/**********************************************************************************************************************************
 * @file    GPIO_Platform_Interface.c
 * @brief   GPIO_Platform_Inteface.h module implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "GPIO_Platform_Interface.h"
#include "stm32f4xx.h"

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
/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Checks whether a Platform GPIO instance has been initialized.
 *
 * @details This helper function validates the initialization state of a GPIO
 *          instance. If the supplied pointer is NULL, the function returns
 *          false.
 *
 * @param   Instance - Pointer to the Platform GPIO instance.
 *
 * @return  true   - if the GPIO instance has been initialized.
 * @return  false  - if the instance is NULL or not initialized.
 */
static inline bool PGPIO_IsInit(GPIO_Handle_t* Instance)
{
    return Instance == NULL ? false : Instance->_initialized;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Creates and configures a Platform GPIO instance.
 *
 * @details Initializes a Platform GPIO handle by associating it with the
 *          specified hardware GPIO port and pin.
 *
 *          This function only prepares the software representation of the GPIO.
 *          No hardware configuration is performed until PGPIO_Init() is called.
 *
 * @param   Instance  - Pointer to the Platform GPIO instance.
 * @param   GPIO_Port - Opaque pointer identifying the hardware GPIO port.
 * @param   GPIO_Pin  - Platform-specific GPIO pin number.
 *
 * @note    On STM32 platforms, the pin number is internally converted to the
 *          corresponding GPIO_PIN_x bit mask.
 *
 * @return  GPIO_OPERATION_OK   - Whether the instance was successfully created.
 * @return  GPIO_OPERATION_FAIL - Whether any parameter is invalid.
 */
GPIO_OpStatus_t PGPIO_Init(GPIO_Handle_t* Instance, void* GPIO_Port, uint16_t GPIO_Pin)
{
    if(Instance == NULL || GPIO_Port == NULL)
    {
        return GPIO_OPERATION_FAIL;
    }

    Instance->_gpio_config._gpio_port = (GPIO_TypeDef*)GPIO_Port;
    Instance->_gpio_config._gpio_pin  = (uint16_t)(1 << GPIO_Pin);

    Instance->_initialized = true;

    return GPIO_OPERATION_OK;
}

/**
 * @brief   Drives the GPIO output to the HIGH logic level.
 *
 * @details Writes a logic HIGH level to the associated GPIO output pin through
 *          the underlying platform implementation.
 *
 * @param   Instance Pointer to the Platform GPIO instance.
 *
 * @note    The GPIO instance shall be initialized before calling this function.
 *
 * @return  GPIO_OPERATION_OK   - Whether the operation completes successfully.
 * @return  GPIO_OPERATION_FAIL - Whether the supplied instance is NULL.
 */
GPIO_OpStatus_t PGPIO_Set(GPIO_Handle_t* Instance)
{
    if(Instance == NULL || !PGPIO_IsInit(Instance))
    {
        return GPIO_OPERATION_FAIL;
    }

    GPIO_TypeDef* port  = (GPIO_TypeDef*) Instance->_gpio_config._gpio_port;

    uint16_t      pin   = (uint16_t)Instance->_gpio_config._gpio_pin;

    GPIO_PinState level = (GPIO_PinState)GPIO_LEVEL_HIGH;

    HAL_GPIO_WritePin(port, pin, level);

    return GPIO_OPERATION_OK;
}

/**
 * @brief   Drives the GPIO output to the LOW logic level.
 *
 * @details Writes a logic LOW level to the associated GPIO output pin through
 *          the underlying platform implementation.
 *
 * @param   Instance Pointer to the Platform GPIO instance.
 *
 * @note    The GPIO instance shall be initialized before calling this function.
 *
 * @return  GPIO_OPERATION_OK   - Whether the operation completes successfully.
 * @return  GPIO_OPERATION_FAIL - Whether the supplied instance is NULL.
 */
GPIO_OpStatus_t PGPIO_Reset(GPIO_Handle_t* Instance)
{
    if(Instance == NULL || !PGPIO_IsInit(Instance))
    {
        return GPIO_OPERATION_FAIL;
    }

    GPIO_TypeDef* port  = (GPIO_TypeDef*) Instance->_gpio_config._gpio_port;

    uint16_t      pin   = (uint16_t)Instance->_gpio_config._gpio_pin;

    GPIO_PinState level = (GPIO_PinState)GPIO_LEVEL_LOW;

    HAL_GPIO_WritePin(port, pin, level);

    return GPIO_OPERATION_OK;
}

/**
 * @brief   Toggles the current GPIO output level.
 *
 * @details Inverts the logical state of the associated GPIO output pin.
 *
 *          If the output is HIGH it becomes LOW. If the output is LOW it
 *          becomes HIGH.
 *
 * @param   Instance Pointer to the Platform GPIO instance.
 *
 * @note    The GPIO instance shall be initialized before calling this function.
 *
 * @return  GPIO_OPERATION_OK   - Whether the operation completes successfully.
 * @return  GPIO_OPERATION_FAIL - Whether the supplied instance is NULL.
 */
GPIO_OpStatus_t PGPIO_Toggle(GPIO_Handle_t* Instance)
{
    if(Instance == NULL || !PGPIO_IsInit(Instance))
    {
        return GPIO_OPERATION_FAIL;
    }

    GPIO_TypeDef* port  = (GPIO_TypeDef*) Instance->_gpio_config._gpio_port;

    uint16_t      pin   = (uint16_t)Instance->_gpio_config._gpio_pin;

    HAL_GPIO_TogglePin(port, pin);

    return GPIO_OPERATION_OK;
}

/**
 * @brief   Returns the current logical level of a GPIO pin.
 *
 * @details Reads the current logic level reported by the underlying hardware
 *          and returns the corresponding Platform GPIO level.
 *
 * @param   Instance Pointer to the Platform GPIO instance.
 *
 * @note    This function returns the actual GPIO pin state reported by the
 *          platform, which may differ from the last output value written if the
 *          pin is externally driven.
 *
 * @return  GPIO_LEVEL_HIGH    if the pin is at logic HIGH.
 * @return  GPIO_LEVEL_LOW     if the pin is at logic LOW.
 * @return  GPIO_LEVEL_UNKNOWN if the supplied instance is NULL.
 */
GPIO_Level_t PGPIO_GetLevel(const GPIO_Handle_t* Instance)
{
    if(Instance == NULL)
    {
        return GPIO_LEVEL_UNKNOWN;
    }

    GPIO_TypeDef* port  = (GPIO_TypeDef*) Instance->_gpio_config._gpio_port;

    uint16_t      pin   = (uint16_t)Instance->_gpio_config._gpio_pin;

    GPIO_Level_t  level = (GPIO_Level_t)HAL_GPIO_ReadPin(port, pin);

    return level;
}




