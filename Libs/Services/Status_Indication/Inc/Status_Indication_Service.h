/**********************************************************************************************************************************
 * @file    Status_Indication_Service.h
 * @brief   Public interface of the non-blocking Status Indication Service.
 *
 * @details The Status Indication Service translates semantic electronic-lock
 *          states into timed LED patterns. Each pattern contains one or more
 *          phases that select an initial LED state, an LED Driver effect, an
 *          effect interval, a repetition count and a service-level duration.
 *
 *          The service does not create or initialize GPIO hardware. A
 *          caller-owned LED Driver handle shall be initialized and injected
 *          into a caller-owned SIS_Handle_t through SIS_Init().
 *
 *          Pattern execution is non-blocking. The service obtains its phase
 *          timestamps from the Time Platform Interface and the application
 *          shall call SIS_Update() periodically. The LED Driver and this
 *          service therefore progress from the same platform time base.
 *
 * @note    The current implementation is instance-based rather than singleton.
 *          Each initialized SIS_Handle_t retains one LED Driver reference and
 *          its own pattern-progression state.
 *
 * @note    One service instance and its injected LED Driver shall be accessed
 *          by only one serialized execution context at a time.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 17, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_SERVICES_STATUS_INDICATOR_INC_STATUS_INDICATION_SERVICE_H_
#define LIBS_SERVICES_STATUS_INDICATOR_INC_STATUS_INDICATION_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Led_Driver.h"
#include "stdbool.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   Execution status returned by Status Indication Service operations.
 *
 * @details Reports whether validation, phase application and required LED
 *          Driver operations completed successfully.
 */
typedef enum
{
    SIS_OPERATION_OK,   /*< The requested operation completed successfully. */
    SIS_OPERATION_FAIL  /*< The requested operation could not be completed. */

}SIS_OpStatus_t;

/**
 * @brief   Semantic LED indications provided by the service.
 *
 * @details Identifies the fixed service-owned pattern requested by application
 *          code without exposing LED effect sequencing at the call site.
 *
 * @note    SIS_INDICATION_COUNT defines the pattern-map size and is not a
 *          playable semantic indication.
 */
typedef enum
{
    SIS_INDICATION_LOCKED = 0U,    /*< Stable indication for the normal locked state. */

    SIS_INDICATION_ACCESS_GRANTED, /*< Flash feedback after granted access.           */

    SIS_INDICATION_ACCESS_DENIED,  /*< Two-phase pulse feedback after denial.         */

    SIS_INDICATION_LOCKOUT_ENTRY,  /*< Accelerating pulses ending in a steady LED.    */

    SIS_INDICATION_COUNT           /*< Number of patterns and invalid-map boundary.   */

}SIS_Indication_t;

/**
 * @brief   One timed phase of a semantic LED indication.
 *
 * @details Defines the logical LED state applied before triggering an LED
 *          Driver effect, the effect timing parameters and the nominal duration
 *          used by the service to advance to the next phase.
 *
 * @note    A duration of 0U is advanced without an elapsed-time comparison on
 *          the next SIS_Update() call. It is used by terminal static phases.
 */
typedef struct
{
    LED_State_t  _led_state;          /*< Logical state applied before the effect.         */
    LED_Effect_t _effect;             /*< LED Driver effect triggered for this phase.      */
    uint32_t     _effect_interval_ms; /*< Interval between effect transitions.             */
    uint16_t     _effect_repeats;     /*< Repetition request forwarded to the LED Driver.  */
    uint32_t     _duration_ms;        /*< Nominal service-level phase duration.            */
    
}SIS_Phase_t;

/**
 * @brief   Immutable description of one complete indication pattern.
 *
 * @details References the first phase in a service-owned compile-time sequence
 *          and records how many phases belong to that pattern.
 */
typedef struct
{
    const SIS_Phase_t* _phases;      /*< Pointer to the first immutable pattern phase. */
    uint16_t           _phase_count; /*< Number of valid phases in the sequence.        */

}SIS_Pattern_t;

/**
 * @brief   Runtime state of one Status Indication Service instance.
 *
 * @details Retains the borrowed LED Driver dependency, selected immutable
 *          pattern, nominal phase timeline, current phase index and successful
 *          initialization state.
 *
 * @warning Members are private service state. Callers own the structure storage
 *          but shall not read or modify its members directly.
 */
typedef struct
{
    LED_Handle_t*        _led;                 /*< Borrowed initialized LED Driver handle.              */
    const SIS_Pattern_t* _active_pattern;      /*< Selected pattern retained after phase completion.    */
    uint32_t             _phase_started_ms;    /*< Nominal phase start from the platform time base.     */
    uint16_t             _current_phase_index; /*< Current phase index or phase count after completion. */
    bool                 _initialized;         /*< Indicates whether initialization succeeded.          */

}SIS_Handle_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
SIS_OpStatus_t SIS_Init          (SIS_Handle_t* Instance, LED_Handle_t* Led);
SIS_OpStatus_t SIS_SetIndication (SIS_Handle_t* Instance, SIS_Indication_t Indication);
SIS_OpStatus_t SIS_Update        (SIS_Handle_t* Instance);


#ifdef __cplusplus
}
#endif

#endif /* LIBS_SERVICES_STATUS_INDICATOR_INC_STATUS_INDICATION_SERVICE_H_ */
