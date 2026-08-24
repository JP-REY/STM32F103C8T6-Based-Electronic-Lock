/**********************************************************************************************************************************
 * @file    App_Core_Internal.h
 * @brief   Internal cross-module interface of the electronic-lock application core.
 *
 * @details Exposes application-core operations required by sibling implementation modules without adding them to the public
 *          App_Core.h lifecycle interface.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-23
 **********************************************************************************************************************************/

#ifndef APP_CORE_INC_APP_CORE_INTERNAL_H_
#define APP_CORE_INC_APP_CORE_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "App_Config.h"
#include "stdbool.h"

/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
bool App_StartTimeout  (App_TimeoutId_t Timeout);
void App_CancelTimeout (void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CORE_INC_APP_CORE_INTERNAL_H_ */
