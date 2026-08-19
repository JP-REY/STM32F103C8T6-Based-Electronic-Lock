/**********************************************************************************************************************************
 * @file    Sound_Generator_Service.h
 * @brief   Public interface of the non-blocking Sound Generator Service.
 *
 * @details The Sound Generator Service is a singleton presentation module that
 *          translates semantic application sound requests into timestamp-driven
 *          buzzer phases. Each phase defines a frequency, a duration and whether
 *          the PWM output shall be enabled.
 *
 *          The service does not create or initialize PWM hardware. A Buzzer
 *          Driver instance owned by the application composition root shall be
 *          initialized and injected through SGS_Init().
 *
 *          New sound requests follow a replacement policy based on pattern
 *          priority. A request whose priority is lower than the active pattern
 *          is ignored. A request with equal or higher priority replaces the
 *          active pattern immediately. No sound queue is maintained.
 *
 *          Pattern execution is non-blocking. The application shall call
 *          SGS_Update() periodically and provide a monotonically increasing
 *          millisecond timestamp. A maximum update cadence of 10 ms is
 *          recommended for the configured V1 patterns.
 *
 * @note    This module does not create tasks, use RTOS primitives, perform
 *          dynamic memory allocation or include STM32 peripheral headers.
 *
 * @note    All mutable runtime state is owned by the implementation. The
 *          function-based API shall be accessed by only one serialized
 *          execution context at a time.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 16, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_SERVICES_SOUND_GENERATOR_INC_SOUND_GENERATOR_SERVICE_H_
#define LIBS_SERVICES_SOUND_GENERATOR_INC_SOUND_GENERATOR_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Buzzer_Driver.h"
#include "stdbool.h"
#include "stdint.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   Execution status returned by Sound Generator operations.
 *
 * @details SGS_OPERATION_IGNORED is a successful policy outcome rather than a
 *          hardware or API failure. It reports that a lower-priority request
 *          did not replace the currently active pattern.
 */
typedef enum
{
    SGS_OPERATION_OK,      /*< The requested operation completed successfully.           */
    SGS_OPERATION_IGNORED, /*< Replacement policy preserved a higher-priority pattern.   */
    SGS_OPERATION_FAIL     /*< The requested operation could not be completed.           */

}SGS_OpStatus_t;

/**
 * @brief   Semantic sound patterns provided by the service.
 *
 * @details Identifies the fixed, service-owned phase sequence requested by the
 *          application. Callers select an application meaning rather than
 *          supplying buzzer frequencies or timing data directly.
 *
 * @note    SGS_RINGTONE_COUNT is a boundary and pattern-map size. It is not a
 *          playable ringtone and shall not be passed to SGS_Ring().
 */
typedef enum
{
    SGS_RINGTONE_KEYPRESS,          /*< Short acknowledgement for an accepted keypress. */

    SGS_RINGTONE_ENTRY_INCOMPLETE,  /*< Short feedback for an entry incomplete.         */

    SGS_RINGTONE_ENTRY_TIMEOUT,     /*< Short feedback for an entry timeout.            */

    SGS_RINGTONE_ACCESS_GRANTED,    /*< Rising feedback pattern for granted access.     */

    SGS_RINGTONE_ERROR,             /*< Descending feedback pattern for an error.       */

    SGS_RINGTONE_LOCKOUT,           /*< Feedback pattern for lockout entry.             */

    SGS_RINGTONE_COUNT              /*< Number of patterns and invalid ringtone marker. */

}SGS_Ringtone_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
SGS_OpStatus_t SGS_Init   (Buzzer_Handle_t* Buzzer);
SGS_OpStatus_t SGS_Ring   (SGS_Ringtone_t Ringtone, uint32_t CurrentTimeMs);
SGS_OpStatus_t SGS_Update (uint32_t CurrentTimeMs);
SGS_OpStatus_t SGS_Stop   (void);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_SERVICES_SOUND_GENERATOR_INC_SOUND_GENERATOR_SERVICE_H_ */
