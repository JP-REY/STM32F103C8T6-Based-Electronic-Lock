/**********************************************************************************************************************************
 * @file    App_Executor.h
 * @brief   Internal semantic-action execution interface of the App layer.
 *
 * @details Declares the boundary used by App Core to execute LCS_Action_t values selected by the Lock Control Service. App Executor
 *          translates those semantic actions into concrete application side effects involving credential services, presentation
 *          services, application-owned timeouts and Door Control operations.
 *
 *          Action execution may synchronously produce a semantic LCS_Event_t follow-up when the requested operation has an immediate
 *          result, allowing App Core to continue the bounded Lock Control action/event dispatch chain.
 *
 *          Runtime dependencies and application-owned buffers are supplied through the object registry composed by App Config.
 *          Product-state decisions remain exclusively owned by the Lock Control Service.
 *
 *          This is an App-internal interface. Firmware entry points outside the App layer shall include App_Core.h instead.
 *
 * @note    The executor assumes App_Init() has successfully established the required runtime-object graph before normal action
 *          execution begins.
 *
 * @note    The interface is intended for serialized cooperative use and is not reentrant.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    Aug 30, 2026
 **********************************************************************************************************************************/

#ifndef APP_EXECUTOR_H
#define APP_EXECUTOR_H

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "App_Config.h"

/**********************************************************************************************************************************
 Defines
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
LCS_Event_t App_ExecuteAction(LCS_Action_t Action);

#ifdef __cplusplus
}
#endif

#endif /* APP_EXECUTOR_H */
