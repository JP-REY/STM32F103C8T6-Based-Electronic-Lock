/**********************************************************************************************************************************
 * @file    LockActuator_Driver.c
 * @brief   Polarity-independent digital lock-actuator driver implementation.
 *
 * @details Maps logical lock and unlock commands to a caller-owned GPIO Platform descriptor. GPIO readback represents only the
 *          electrical command level; it does not confirm mechanical bolt position or enforce a bounded unlock interval.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-24
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "LockActuator_Driver.h"
#include "stddef.h"

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
 * @brief   Checks whether a lock-actuator instance is initialized.
 *
 * @param   Device - Pointer to the lock-actuator handle.
 *
 * @return  true  - Whether Device is non-NULL and initialized.
 * @return  false - Whether Device is NULL or not initialized.
 */
static inline bool LockActuator_IsInit(LockActuator_Handle_t* Device)
{
    return Device == NULL ? false : Device->_initialized;
}

/**
 * @brief   Drives the GPIO to the configured locked-command level.
 *
 * @param   Device - Pointer to an initialized lock-actuator handle.
 *
 * @return  LOCK_ACTUATOR_OPERATION_OK   - Whether the GPIO write succeeds.
 * @return  LOCK_ACTUATOR_OPERATION_FAIL - Whether the GPIO write fails.
 */
static inline LockActuator_OpStatus_t LockActuator_SetLockState(LockActuator_Handle_t* Device)
{
    if(Device->_active_level == LOCK_ACTUATOR_ACTIVE_LEVEL_LOW)
    {
        if(PGPIO_Reset(Device->_gpio) != GPIO_OPERATION_OK)
        {
            return LOCK_ACTUATOR_OPERATION_FAIL;
        }
    }

    else
    {
        if(PGPIO_Set(Device->_gpio) != GPIO_OPERATION_OK)
        {
            return LOCK_ACTUATOR_OPERATION_FAIL;
        }
    }

    return LOCK_ACTUATOR_OPERATION_OK;
}

/**
 * @brief   Drives the GPIO to the inverse of the locked-command level.
 *
 * @param   Device - Pointer to an initialized lock-actuator handle.
 *
 * @return  LOCK_ACTUATOR_OPERATION_OK   - Whether the GPIO write succeeds.
 * @return  LOCK_ACTUATOR_OPERATION_FAIL - Whether the GPIO write fails.
 */
static inline LockActuator_OpStatus_t LockActuator_ResetLockState(LockActuator_Handle_t* Device)
{
    if(Device->_active_level == LOCK_ACTUATOR_ACTIVE_LEVEL_LOW)
    {
        if(PGPIO_Set(Device->_gpio) != GPIO_OPERATION_OK)
        {
            return LOCK_ACTUATOR_OPERATION_FAIL;
        }
    }

    else
    {
        if(PGPIO_Reset(Device->_gpio) != GPIO_OPERATION_OK)
        {
            return LOCK_ACTUATOR_OPERATION_FAIL;
        }
    }

    return LOCK_ACTUATOR_OPERATION_OK;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes a lock-actuator driver instance.
 *
 * @details Retains the supplied Platform GPIO handle and the electrical level
 *          representing the locked command. The function deliberately performs
 *          no GPIO write; safe-state assertion remains an explicit caller action.
 *
 * @param   Device  - Pointer to the lock-actuator handle to initialize.
 * @param   Context - Pointer to the caller-owned Platform GPIO output handle.
 * @param   Level   - Electrical level that represents the locked command.
 *
 * @return  LOCK_ACTUATOR_OPERATION_OK   - Whether initialization succeeds.
 * @return  LOCK_ACTUATOR_OPERATION_FAIL - Whether a pointer or Level is invalid.
 */
LockActuator_OpStatus_t LockActuator_Init(LockActuator_Handle_t* Device, GPIO_Handle_t* Context, LockActuator_ActiveLevel_t Level)
{
    if(Device == NULL || Context == NULL ||
      (Level != LOCK_ACTUATOR_ACTIVE_LEVEL_LOW &&
       Level != LOCK_ACTUATOR_ACTIVE_LEVEL_HIGH))
    {
        return LOCK_ACTUATOR_OPERATION_FAIL;
    }

    Device->_gpio         = Context;
    Device->_active_level = Level;
    Device->_initialized  = true;

    return LOCK_ACTUATOR_OPERATION_OK;
}

/**
 * @brief   Commands the actuator's locked state.
 *
 * @param   Device - Pointer to an initialized lock-actuator handle.
 *
 * @return  LOCK_ACTUATOR_OPERATION_OK   - Whether the locked command is written.
 * @return  LOCK_ACTUATOR_OPERATION_FAIL - Whether validation or GPIO write fails.
 */
LockActuator_OpStatus_t LockActuator_Lock(LockActuator_Handle_t* Device)
{
    if(Device == NULL || !LockActuator_IsInit(Device))
    {
        return LOCK_ACTUATOR_OPERATION_FAIL;
    }

    if(LockActuator_SetLockState(Device) != LOCK_ACTUATOR_OPERATION_OK)
    {
        return LOCK_ACTUATOR_OPERATION_FAIL;
    }

    return LOCK_ACTUATOR_OPERATION_OK;
}

/**
 * @brief   Commands the actuator's unlocked state.
 *
 * @param   Device - Pointer to an initialized lock-actuator handle.
 *
 * @return  LOCK_ACTUATOR_OPERATION_OK   - Whether the unlocked command is written.
 * @return  LOCK_ACTUATOR_OPERATION_FAIL - Whether validation or GPIO write fails.
 */
LockActuator_OpStatus_t LockActuator_Unlock(LockActuator_Handle_t* Device)
{
    if(Device == NULL || !LockActuator_IsInit(Device))
    {
        return LOCK_ACTUATOR_OPERATION_FAIL;
    }

    if(LockActuator_ResetLockState(Device) != LOCK_ACTUATOR_OPERATION_OK)
    {
        return LOCK_ACTUATOR_OPERATION_FAIL;
    }

    return LOCK_ACTUATOR_OPERATION_OK;
}

/**
 * @brief   Returns the logical command represented by the GPIO level.
 *
 * @details Reads the GPIO and maps the configured locked-command level to
 *          LOCK_ACTUATOR_STATE_LOCKED. The inverse level maps to
 *          LOCK_ACTUATOR_STATE_UNLOCKED.
 *
 * @param   Device - Pointer to an initialized lock-actuator handle.
 *
 * @return  LOCK_ACTUATOR_STATE_LOCKED   - Whether the locked level is present.
 * @return  LOCK_ACTUATOR_STATE_UNLOCKED - Whether the inverse level is present.
 *
 * @warning LOCK_ACTUATOR_STATE_UNKNOWN is reserved by the public type but is not produced by the current implementation. Invalid
 *          handles currently return the numeric LOCK_ACTUATOR_OPERATION_FAIL value through the state return type.
 */
LockActuator_State_t LockActuator_GetState(LockActuator_Handle_t* Device)
{
    if(Device == NULL || !LockActuator_IsInit(Device))
    {
        return LOCK_ACTUATOR_OPERATION_FAIL;
    }

    GPIO_Level_t lock_state = PGPIO_GetLevel(Device->_gpio);

    if(Device->_active_level == LOCK_ACTUATOR_ACTIVE_LEVEL_LOW)
    {

        return lock_state == GPIO_LEVEL_LOW              ?
                             LOCK_ACTUATOR_STATE_LOCKED  :
                             LOCK_ACTUATOR_STATE_UNLOCKED;
    }

    else
    {
        return lock_state == GPIO_LEVEL_HIGH             ?
                             LOCK_ACTUATOR_STATE_LOCKED  :
                             LOCK_ACTUATOR_STATE_UNLOCKED;
    }
}
