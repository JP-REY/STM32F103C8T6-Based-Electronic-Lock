/**********************************************************************************************************************************
 * @file    App_Core.h
 * @brief   Public synchronous interface of the electronic-lock application core.
 *
 * @details Defines the public lifecycle and execution entry points of the electronic-lock application. The module acts as the
 *          composition boundary between CubeMX-generated resources, platform interfaces, component drivers and domain/UI
 *          services, including credential entry, replacement, persistence and authentication coordination.
 *
 *          Concrete handles and dependency bindings remain private to App_Core.c. Callers therefore interact with application
 *          operations without depending on STM32, platform, adapter, component or service implementation types.
 *
 * @note    App_Init() shall be called after CubeMX peripheral initialization and before the first App_ReadInput() or
 *          App_Dispatch() call.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    Aug 22, 2026
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
