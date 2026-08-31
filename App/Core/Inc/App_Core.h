/**********************************************************************************************************************************
 * @file    App_Core.h
 * @brief   Public lifecycle and execution interface of the electronic-lock application.
 *
 * @details Defines the public entry points used to initialize and cooperatively execute the electronic-lock application. App_Init()
 *          establishes the complete application runtime before App_ReadInput() and App_Dispatch() are used during normal operation.
 *
 *          App_ReadInput() owns Matrix Keyboard acquisition at the public application boundary, while App_Dispatch() advances
 *          application timing, Door Control, presentation services and deferred physical-input event processing. Product-state
 *          decisions remain owned by the Lock Control Service and concrete semantic side effects are executed internally by
 *          App Executor.
 *
 *          Product bindings, Platform descriptors, component handles, service runtimes and credential buffers are owned internally
 *          by App Config. Public callers therefore interact with the application without depending on STM32 HAL, Platform, adapter,
 *          component or service implementation types.
 *
 * @note    App_Init() shall be called after CubeMX-generated peripheral initialization and shall complete successfully before the
 *          first App_ReadInput() or App_Dispatch() call.
 *
 * @note    Public execution entry points are intended for serialized cooperative use and are not reentrant.
 *
 * @author  Joao Pedro Rey
 * @version 1.2.0
 * @date    Aug 30, 2026
 **********************************************************************************************************************************/

#ifndef APP_CORE_INC_APP_CORE_H_
#define APP_CORE_INC_APP_CORE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   Application initialization result.
 *
 * @details Reports whether the complete application dependency graph was initialized and is ready for use. A failed result means
 *          at least one required Platform object, component, adapter or service could not be initialized.
 */
typedef enum
{
    APP_INIT_SUCCESSFULLY, /*< Every application dependency was initialized successfully.         */
    APP_INIT_FAILED        /*< At least one required application dependency failed to initialize. */

}App_InitStatus_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
App_InitStatus_t App_Init      (void);
void             App_ReadInput (void);
void             App_Dispatch  (void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CORE_INC_APP_CORE_H_ */
