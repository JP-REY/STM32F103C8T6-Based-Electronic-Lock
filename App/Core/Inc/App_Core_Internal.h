/**********************************************************************************************************************************
 * @file    App_Core_Internal.h
 * @brief   Internal coordination interface shared within the App layer.
 *
 * @details Declares App Core operations required by sibling App implementation modules, such as application-timeout control, without
 *          exposing those operations through the public App_Core.h lifecycle and execution interface.
 *
 *          The interface exists to support explicit cross-translation-unit collaboration inside App/ while preserving App Core
 *          ownership of the corresponding runtime state and coordination rules.
 *
 * @note    This header is internal to the App layer and shall not be included by Platform, component, service or firmware entry-point
 *          modules outside App/.
 *
 * @note    App_Core_Internal.h shall not be used to bypass the public App_Core.h interface or to expose App Config runtime objects.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    Aug 30, 2026
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
