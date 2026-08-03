/**********************************************************************************************************************************
 * @file    GPIO_Platform_Interface.h
 * @brief   Platform abstraction interface for General Purpose Input/Output (GPIO).
 *
 * @details This module defines a generic Platform Abstraction Layer (PAL) for
 *          General Purpose Input/Output peripherals.
 *
 *          It provides a hardware-independent interface used to configure and
 *          control digital GPIO pins while hiding all microcontroller-specific
 *          implementation details.
 *
 *          The module follows the Platform Interface design adopted throughout
 *          this project:
 *              - The application interacts only with this API.
 *              - The implementation is platform specific.
 *              - Platform-dependent objects are hidden through opaque pointers.
 *
 * @note    All public functions are prefixed with PGPIO (Platform GPIO).
 *
 * @note    This interface currently supports only basic digital output
 *          operations and input level reading.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 3, 2026
 **********************************************************************************************************************************/

#ifndef INC_GPIO_PLATFORM_INTERFACE_H_
#define INC_GPIO_PLATFORM_INTERFACE_H_

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
 * @brief   Defines the operation status returned by the Platform GPIO interface.
 *
 * @details This enumeration represents the execution result of a Platform GPIO
 *          operation. It is returned by all functions capable of succeeding
 *          or failing due to invalid parameters or platform-specific errors.
 *
 * @note    Every Platform GPIO function returning this type shall return
 *          GPIO_OPERATION_OK when the requested operation completes
 *          successfully.
 **********************************************************************************************************************************/
typedef enum
{
    GPIO_OPERATION_OK = 0U,
    GPIO_OPERATION_FAIL

}GPIO_OpStatusTypeDef;

/**********************************************************************************************************************************
 * @brief   Defines the logical level of a GPIO pin.
 *
 * @details This enumeration represents the current electrical state of a GPIO
 *          configured as either an input or an output.
 *
 *          The mapping between logical levels and actual voltage thresholds
 *          depends on the target platform and hardware configuration.
 *
 * @note    GPIO_LEVEL_UNKNOWN may be returned when the pin level cannot be
 *          determined due to an invalid handle or a platform-specific error.
 **********************************************************************************************************************************/
typedef enum
{
    GPIO_LEVEL_HIGH = 1U,
    GPIO_LEVEL_LOW  = 0U,

    GPIO_LEVEL_UNKNOWN = 0xFFU

}GPIO_LevelTypeDef;


/**********************************************************************************************************************************
 * @brief   GPIO hardware configuration.
 *
 * @details This structure stores the minimum platform-specific information
 *          required to identify a GPIO pin.
 *
 *          The GPIO port is intentionally stored as an opaque pointer to avoid
 *          exposing any vendor-specific peripheral types through the Platform
 *          Interface.
 **********************************************************************************************************************************/
typedef struct
{
    void*    _gpio_port;
    uint16_t _gpio_pin;

}GPIO_ConfigTypeDef;

/**********************************************************************************************************************************
 * @brief   Platform GPIO instance.
 *
 * @details This structure represents a single logical GPIO managed by the
 *          Platform GPIO interface.
 *
 *          Besides storing the GPIO hardware configuration, it also maintains
 *          the last logical output level written through the Platform API and
 *          the initialization state of the instance.
 *
 *          Application code shall never access or modify any member directly
 *          after initialization. Runtime operations shall always be performed
 *          through the Platform GPIO API.
 **********************************************************************************************************************************/
typedef struct
{
    /* << Private data. Do not read or modify!                >> */
    /* << Platform GPIO hardware configuration                >> */ GPIO_ConfigTypeDef _gpio_config;
    /* << Indicates whether the instance has been initialized >> */ bool               _initialized;

}GPIO_HandleTypeDef;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
GPIO_OpStatusTypeDef PGPIO_Create   (GPIO_HandleTypeDef* Instance, void* GPIO_Port, uint16_t GPIO_Pin);
GPIO_OpStatusTypeDef PGPIO_Init     (GPIO_HandleTypeDef* Instance);
GPIO_OpStatusTypeDef PGPIO_Set      (GPIO_HandleTypeDef* Instance);
GPIO_OpStatusTypeDef PGPIO_Reset    (GPIO_HandleTypeDef* Instance);
GPIO_OpStatusTypeDef PGPIO_Toggle   (GPIO_HandleTypeDef* Instance);
GPIO_LevelTypeDef    PGPIO_GetLevel (const GPIO_HandleTypeDef* Instance);

#ifdef __cplusplus
}
#endif

#endif /* INC_GPIO_PLATFORM_INTERFACE_H_ */
