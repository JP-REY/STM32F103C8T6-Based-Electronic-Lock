/**********************************************************************************************************************************
 * @file    Buzzer_Driver.h
 * @brief   Buzzer driver public interface.
 *
 * @details This module provides the public interface for controlling a buzzer
 *          through a PWM platform interface. The driver abstracts the PWM
 *          hardware configuration required to generate the buzzer output
 *          frequency and provides operations for initialization, frequency
 *          configuration, and output control.
 *
 *          The buzzer driver depends on the PWM platform interface to perform
 *          the underlying PWM operations, keeping the buzzer functionality
 *          independent from the specific timer or PWM peripheral implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Ago 08, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_COMPONENTS_BUZZER_INC_BUZZER_DRIVER_H_
#define LIBS_COMPONENTS_BUZZER_INC_BUZZER_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "stm32f4xx.h"
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include "PWM_Platform_Interface.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Defines the operation status returned by the buzzer driver.
 *
 * @details This enumeration indicates whether a buzzer operation was
 *          successfully executed or failed.
 **********************************************************************************************************************************/
typedef enum
{
    BUZZER_OPERATION_OK,
    BUZZER_OPERATION_FAIL

}Buzzer_OpStatusTypeDef;

/**********************************************************************************************************************************
 * @brief   Represents the buzzer device instance.
 *
 * @details This structure stores the runtime context required by the buzzer
 *          driver. The PWM context provides access to the platform layer used
 *          to control the buzzer output.
 *
 *          The initialization flag indicates whether the buzzer device has
 *          been successfully initialized and is ready for operation.
 *
 * @warning The structure members are private data and shall not be accessed
 *          or modified directly by the application. The buzzer driver API
 *          shall be used to manage the device state.
 **********************************************************************************************************************************/
typedef struct
{
    /* << Private data. Do not read or modify >> */
    /* << Pointer to the PWM platform handle used by the buzzer. >> */ PWM_HandleTypeDef* _context;
    /* << Indicates wheter buzzer instance has been initialized. >> */ bool               _initialized;

}Buzzer_HandleTypeDef;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
Buzzer_OpStatusTypeDef Buzzer_Init         (Buzzer_HandleTypeDef* Device, PWM_HandleTypeDef* Context);
Buzzer_OpStatusTypeDef Buzzer_SetFrequency (Buzzer_HandleTypeDef* Device, uint32_t Frequency);
Buzzer_OpStatusTypeDef Buzzer_On           (Buzzer_HandleTypeDef* Device);
Buzzer_OpStatusTypeDef Buzzer_Off          (Buzzer_HandleTypeDef* Device);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_BUZZER_INC_BUZZER_DRIVER_H_*/
