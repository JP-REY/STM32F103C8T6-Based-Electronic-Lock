/**********************************************************************************************************************************
 * @file    LockActuator_Driver.h
 * @brief   Digital lock-actuator driver public interface.
 *
 * @details This module exposes logical lock, unlock and state-query operations
 *          over a caller-provided Platform GPIO handle. The electrical level
 *          that represents the locked command is selected during initialization,
 *          allowing active-high and active-low actuator control circuits.
 *
 *          Reported state is derived from the GPIO level. It represents the
 *          electrical command and does not confirm mechanical bolt position.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-24
 **********************************************************************************************************************************/

#ifndef LOCKACTUATOR_DRIVER_H
#define LOCKACTUATOR_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "GPIO_Platform_Interface.h"
#include "stdbool.h"

/**********************************************************************************************************************************
 Defines
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   Defines the result of a lock-actuator driver operation.
 */
typedef enum
{
    LOCK_ACTUATOR_OPERATION_OK,
    LOCK_ACTUATOR_OPERATION_FAIL

}LockActuator_OpStatus_t;

/**
 * @brief   Selects the electrical level that commands the locked state.
 *
 * @details Active-high makes GPIO HIGH the locked command. Active-low makes
 *          GPIO LOW the locked command. Unlock always uses the inverse level.
 */
typedef enum
{
    LOCK_ACTUATOR_ACTIVE_LEVEL_HIGH,
    LOCK_ACTUATOR_ACTIVE_LEVEL_LOW

}LockActuator_ActiveLevel_t;

/**
 * @brief   Defines the logical state inferred from the actuator GPIO.
 *
 * @details LOCKED and UNLOCKED describe the electrical command currently read
 *          from the GPIO. UNKNOWN is reserved for a state that cannot be
 *          determined. None of these values confirms mechanical position.
 */
typedef enum
{
    LOCK_ACTUATOR_STATE_LOCKED,
    LOCK_ACTUATOR_STATE_UNLOCKED,
    LOCK_ACTUATOR_STATE_UNKNOWN

}LockActuator_State_t;

/**
 * @brief   Represents one lock-actuator driver instance.
 *
 * @details The handle retains a caller-owned Platform GPIO descriptor, the
 *          configured locked-command polarity and the initialization state.
 *
 * @warning Members are private driver data and shall not be read or modified
 *          directly by application code.
 */
typedef struct
{
                                              /*< Private data. Do not read or modify!                     */
    GPIO_Handle_t*             _gpio;         /*< Caller-owned Platform GPIO handle used for actuator I/O. */
    LockActuator_ActiveLevel_t _active_level; /*< Electrical level representing the locked command.        */
    bool                       _initialized;  /*< Indicates whether this driver instance was initialized.  */

}LockActuator_Handle_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
LockActuator_OpStatus_t LockActuator_Init     (LockActuator_Handle_t* Device, GPIO_Handle_t* Context, LockActuator_ActiveLevel_t Level);
LockActuator_OpStatus_t LockActuator_Lock     (LockActuator_Handle_t* Device);
LockActuator_OpStatus_t LockActuator_Unlock   (LockActuator_Handle_t* Device);
LockActuator_State_t    LockActuator_GetState (LockActuator_Handle_t* Device);

#ifdef __cplusplus
}
#endif

#endif /* LOCKACTUATOR_DRIVER_H */
