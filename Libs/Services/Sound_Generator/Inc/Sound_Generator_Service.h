/**********************************************************************************************************************************
 * @file    Sound_Generator_Service.h
 * @brief   Public interface of the non-blocking Sound Generator Service.
 *
 * @details The Sound Generator Service translates semantic application sound
 *          requests into timestamp-driven buzzer phases. Each phase defines a
 *          frequency, a duration and whether the PWM output shall be enabled.
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
 * @note    One SGS_Handle_t instance shall be accessed by only one execution
 *          context at a time.
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
    SGS_OPERATION_OK,
    SGS_OPERATION_IGNORED,
    SGS_OPERATION_FAIL

}SGS_OpStatus_t;

/**
 * @brief   Semantic sound patterns provided by the service.
 */
typedef enum
{
    SGS_RINGTONE_KEYPRESS,
    SGS_RINGTONE_ACCESS_GRANTED,
    SGS_RINGTONE_ERROR,
    SGS_RINGTONE_COUNT

}SGS_Ringtone_t;

/**
 * @brief   Replacement priority associated with a sound pattern.
 *
 * @note    Callers select a semantic ringtone rather than a priority. The
 *          service-owned pattern map associates each ringtone with one of
 *          these priorities.
 */
typedef enum
{
    SGS_PRIORITY_KEYPRESS,
    SGS_PRIORITY_FEEDBACK

}SGS_Priority_t;

/**
 * @brief   One timed output phase of a sound pattern.
 *
 * @details When OutputEnabled is true, FrequencyHz shall be nonzero and is
 *          applied before the buzzer output is enabled. When OutputEnabled is
 *          false, the buzzer is disabled and FrequencyHz is ignored.
 */
typedef struct
{
    uint32_t FrequencyHz;
    uint32_t DurationMs;
    bool     OutputEnabled;

}SGS_Phase_t;

/**
 * @brief   Immutable description of a complete sound pattern.
 */
typedef struct
{
    const SGS_Phase_t* Phases;
    uint8_t            PhaseCount;
    SGS_Priority_t     Priority;

}SGS_Pattern_t;

/**
 * @brief   Runtime state of one Sound Generator Service instance.
 *
 * @warning Members are private service state and shall not be read or modified
 *          directly by callers.
 */
typedef struct
{
                                               /*< Private data. Do not read or modify!                           */
    Buzzer_Handle_t*     _buzzer;              /*< Injected buzzer; ownership is not transferred.                 */
    const SGS_Pattern_t* _active_pattern;      /*< Pattern currently being executed, or NULL when idle.           */
    uint32_t             _phase_started_ms;    /*< Nominal start timestamp of the current phase.                  */
    uint8_t              _current_phase_index; /*< Index of the current phase in the active pattern.               */
    bool                 _initialized;         /*< Indicates whether the service was initialized successfully.    */

}SGS_Handle_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
SGS_OpStatus_t SGS_Init     (SGS_Handle_t* Instance, Buzzer_Handle_t* Buzzer);
SGS_OpStatus_t SGS_Ring     (SGS_Handle_t* Instance, SGS_Ringtone_t Ringtone, uint32_t CurrentTimeMs);
SGS_OpStatus_t SGS_Update   (SGS_Handle_t* Instance, uint32_t CurrentTimeMs);
SGS_OpStatus_t SGS_Stop     (SGS_Handle_t* Instance);
bool           SGS_IsActive (const SGS_Handle_t* Instance);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_SERVICES_SOUND_GENERATOR_INC_SOUND_GENERATOR_SERVICE_H_ */
