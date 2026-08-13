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
#include "PWM_Platform_Interface.h"
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
 * @brief   Defines the operation status returned by the buzzer driver.
 *
 * @details This enumeration indicates whether a buzzer operation was
 *          successfully executed or failed.
 **********************************************************************************************************************************/
typedef enum
{
    BUZZER_OPERATION_OK,
    BUZZER_OPERATION_FAIL

}Buzzer_OpStatus_t;

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
    /* << Pointer to the PWM platform handle used by the buzzer. >> */ PWM_Handle_t* _context;
    /* << Indicates wheter buzzer instance has been initialized. >> */ bool          _initialized;

}Buzzer_Handle_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
Buzzer_OpStatus_t Buzzer_Init         (Buzzer_Handle_t* Device, PWM_Handle_t* Context);
Buzzer_OpStatus_t Buzzer_SetFrequency (Buzzer_Handle_t* Device, uint32_t Frequency);
Buzzer_OpStatus_t Buzzer_On           (Buzzer_Handle_t* Device);
Buzzer_OpStatus_t Buzzer_Off          (Buzzer_Handle_t* Device);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_BUZZER_INC_BUZZER_DRIVER_H_*/
