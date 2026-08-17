/**********************************************************************************************************************************
 * @file    Sound_Generator_Service.c
 * @brief   Sound Generator Service implementation.
 *
 * @details Implements a singleton, non-blocking and priority-based sound engine
 *          through an injected Buzzer Driver instance. Pattern timing uses
 *          caller-supplied millisecond timestamps and remains safe across one
 *          unsigned 32-bit timestamp rollover.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 16, 2026
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Sound_Generator_Service.h"
#include "Timeout_Validation_Service.h"
#include "stddef.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**
 * @brief   Returns the number of elements in a compile-time array.
 *
 * @details Divides the complete array size by the size of its first element and
 *          converts the result to the uint8_t pattern phase-count representation.
 *
 * @param   Array - Compile-time array whose element count is required.
 *
 * @note    Array shall be an actual array expression, not a pointer.
 */
#define SGS_ARRAY_LENGTH(Array) ((uint8_t)(sizeof(Array) / sizeof((Array)[0])))

/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**
 * @brief   Replacement priority associated with a sound pattern.
 *
 * @details Orders ringtone replacement policy from the short keypress
 *          acknowledgement to application feedback. Higher enumeration values
 *          represent higher priority.
 *
 * @note    Priority is selected by the private pattern map and is never supplied
 *          directly by public callers.
 */
typedef enum
{
    SGS_PRIORITY_KEYPRESS, /*< Replaceable acknowledgement priority.      */
    SGS_PRIORITY_FEEDBACK  /*< Access-result and error feedback priority. */

}SGS_priority_t;

/**
 * @brief   One timed output phase of a sound pattern.
 *
 * @details Describes the buzzer frequency, nominal duration and explicit output
 *          state applied for one contiguous interval in a ringtone pattern.
 *
 * @note    Enabled phases require a nonzero frequency. Disabled phases ignore
 *          frequency_hz and request Buzzer_Off(). A zero duration is invalid.
 */
typedef struct
{
    uint32_t frequency_hz;   /*< Buzzer frequency applied to an enabled phase. */
    uint32_t duration_ms;    /*< Nominal phase duration in milliseconds.       */
    bool     output_enabled; /*< Selects an audible tone or silent phase.      */

}SGS_Phase_t;

/**
 * @brief   Immutable description of a complete semantic sound pattern.
 *
 * @details Associates a nonempty phase sequence with the replacement priority
 *          enforced when SGS_Ring() receives a new request.
 *
 * @note    Pattern objects and their phase arrays are compile-time constants
 *          owned by this translation unit.
 */
typedef struct
{
    const SGS_Phase_t* phases;      /*< Pointer to the first immutable phase.         */
    uint8_t            phase_count; /*< Number of valid phases in the sequence.       */
    SGS_priority_t     priority;    /*< Replacement priority assigned to the pattern. */

}SGS_Pattern_t;

/**
 * @brief   Private runtime state of the Sound Generator Service singleton.
 *
 * @details Retains the borrowed Buzzer Driver dependency, currently active
 *          immutable pattern, nominal phase timeline, current phase index and
 *          service lifecycle state.
 *
 * @note    Callers cannot allocate this type through the public interface and
 *          shall interact with the singleton only through SGS functions.
 */
typedef struct
{
    Buzzer_Handle_t*     buzzer;              /*< Borrowed initialized buzzer; ownership is not transferred. */
    const SGS_Pattern_t* active_pattern;      /*< Pattern currently executing, or NULL while idle.           */
    uint32_t             phase_started_ms;    /*< Nominal start timestamp of the current phase.              */
    uint8_t              current_phase_index; /*< Index of the current phase in the active pattern.          */
    bool                 initialized;         /*< Indicates whether singleton initialization succeeded.      */

}SGS_Handle_t;

/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**
 * @brief   Short acknowledgement emitted for an accepted keypress.
 *
 * @details Defines one enabled 2600 Hz tone phase with a nominal duration of
 *          40 milliseconds.
 *
 * @note    This pattern uses SGS_PRIORITY_KEYPRESS and may be ignored while
 *          higher-priority feedback remains active.
 */
static const SGS_Phase_t SGS_Keypressphases[] =
{
    { .frequency_hz = 2600U, .duration_ms =  40U, .output_enabled = true }
};

/**
 * @brief   Rising two-tone pattern emitted after access is granted.
 *
 * @details Defines an 80 ms tone at 2000 Hz, 40 ms of silence and a 140 ms tone
 *          at 3000 Hz for a total nominal duration of 260 ms.
 *
 * @note    This pattern uses SGS_PRIORITY_FEEDBACK.
 */
static const SGS_Phase_t SGS_AccessGrantedphases[] =
{
    { .frequency_hz = 2000U, .duration_ms =  80U, .output_enabled = true  },
    { .frequency_hz =    0U, .duration_ms =  40U, .output_enabled = false },
    { .frequency_hz = 3000U, .duration_ms = 140U, .output_enabled = true  }
};

/**
 * @brief   Descending two-tone pattern used for application errors.
 *
 * @details Defines a 120 ms tone at 2600 Hz, 50 ms of silence and a 220 ms tone
 *          at 1800 Hz for a total nominal duration of 390 ms.
 *
 * @note    This pattern uses SGS_PRIORITY_FEEDBACK.
 */
static const SGS_Phase_t SGS_Errorphases[] =
{
    { .frequency_hz = 2600U, .duration_ms = 120U, .output_enabled = true  },
    { .frequency_hz =    0U, .duration_ms =  50U, .output_enabled = false },
    { .frequency_hz = 1800U, .duration_ms = 220U, .output_enabled = true  }
};

/**
 * @brief   Semantic ringtone-to-pattern map.
 *
 * @details Uses SGS_Ringtone_t values as designated indexes so each playable
 *          public ringtone resolves directly to its immutable phase sequence,
 *          phase count and replacement priority.
 *
 * @note    Feedback patterns have higher priority than the keypress pattern.
 *          Patterns with equal priority replace one another.
 */
static const SGS_Pattern_t SGS_PatternMap[SGS_RINGTONE_COUNT] =
{
    [SGS_RINGTONE_KEYPRESS] =
    {
        .phases     = SGS_Keypressphases,
        .phase_count = SGS_ARRAY_LENGTH(SGS_Keypressphases),
        .priority   = SGS_PRIORITY_KEYPRESS
    },

    [SGS_RINGTONE_ACCESS_GRANTED] =
    {
        .phases     = SGS_AccessGrantedphases,
        .phase_count = SGS_ARRAY_LENGTH(SGS_AccessGrantedphases),
        .priority   = SGS_PRIORITY_FEEDBACK
    },

    [SGS_RINGTONE_ERROR] =
    {
        .phases     = SGS_Errorphases,
        .phase_count = SGS_ARRAY_LENGTH(SGS_Errorphases),
        .priority   = SGS_PRIORITY_FEEDBACK
    }
};

/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/**
 * @brief   Module-owned singleton runtime instance.
 *
 * @details Holds all mutable Sound Generator Service state and the Buzzer Driver
 *          dependency injected through SGS_Init(). Zero initialization leaves
 *          the service unbound, uninitialized and idle before its first use.
 *
 * @note    The object is not declared by the public header and shall be accessed
 *          only by functions in this implementation.
 */
SGS_Handle_t SGS_Runtime_Instance;

/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
static void           SGS_ClearPatternState (void);
static SGS_OpStatus_t SGS_ApplyPhase        (const SGS_Phase_t* Phase);
static SGS_OpStatus_t SGS_AbortPattern      (void);
static bool           SGS_IsActive          (void);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Clears only the active-pattern runtime state.
 *
 * @details Sets the active pattern to NULL and resets both the nominal phase
 *          start timestamp and current phase index to zero.
 *
 * @note    The retained buzzer association and initialization state are
 *          preserved. This helper does not access the Buzzer Driver.
 */
static void SGS_ClearPatternState(void)
{
    SGS_Runtime_Instance.active_pattern      = NULL;
    SGS_Runtime_Instance.phase_started_ms    = 0U;
    SGS_Runtime_Instance.current_phase_index = 0U;
}

/**
 * @brief   Applies one pattern phase to the retained buzzer.
 *
 * @details A disabled phase calls Buzzer_Off(). An enabled phase validates a
 *          nonzero frequency, configures that frequency and then calls
 *          Buzzer_On().
 *
 * @param   Phase - Pointer to the immutable phase that shall become active.
 *
 * @note    Frequency is configured before output is enabled. A disabled phase
 *          does not inspect or apply its frequency_hz member.
 *
 * @return  SGS_OPERATION_OK   - When the requested buzzer state was applied;
 *          SGS_OPERATION_FAIL - When Phase or the retained buzzer is null, an
 *                               enabled frequency is zero or a buzzer operation
 *                               fails.
 */
static SGS_OpStatus_t SGS_ApplyPhase(const SGS_Phase_t* Phase)
{
    if(Phase == NULL || SGS_Runtime_Instance.buzzer == NULL)
    {
        return SGS_OPERATION_FAIL;
    }

    if(!Phase->output_enabled)
    {
        return (Buzzer_Off(SGS_Runtime_Instance.buzzer) == BUZZER_OPERATION_OK) ?
                                                           SGS_OPERATION_OK     :
                                                           SGS_OPERATION_FAIL   ;
    }

    if(Phase->frequency_hz == 0U)
    {
        return SGS_OPERATION_FAIL;
    }

    if(Buzzer_SetFrequency(SGS_Runtime_Instance.buzzer, Phase->frequency_hz) != BUZZER_OPERATION_OK)
    {
        return SGS_OPERATION_FAIL;
    }

    if(Buzzer_On(SGS_Runtime_Instance.buzzer) != BUZZER_OPERATION_OK)
    {
        return SGS_OPERATION_FAIL;
    }

    return SGS_OPERATION_OK;
}

/**
 * @brief   Forces the buzzer off and clears the current pattern.
 *
 * @details Calls Buzzer_Off() through the retained dependency and then clears
 *          active pattern state independently from the returned buzzer status.
 *
 * @note    Logical pattern state is cleared even when the hardware disable
 *          operation fails, preventing the pattern from continuing logically.
 *
 * @return  SGS_OPERATION_OK   - When the buzzer was disabled successfully;
 *          SGS_OPERATION_FAIL - When Buzzer_Off() reports failure.
 */
static SGS_OpStatus_t SGS_AbortPattern(void)
{
    Buzzer_OpStatus_t buzzer_status = Buzzer_Off(SGS_Runtime_Instance.buzzer);

    SGS_ClearPatternState();

    return (buzzer_status == BUZZER_OPERATION_OK) ? 
                             SGS_OPERATION_OK     : 
                             SGS_OPERATION_FAIL   ;
}

/**
 * @brief   Reports whether the singleton currently owns an active pattern.
 *
 * @details Combines successful initialization state with a non-null active
 *          pattern reference. No timestamp is evaluated and no phase advances.
 *
 * @note    This helper is private and does not access the Buzzer Driver.
 *
 * @return  true when the initialized singleton has an active pattern;
 *          otherwise false.
 */
static bool SGS_IsActive(void)
{
    return (SGS_Runtime_Instance.initialized &&
            SGS_Runtime_Instance.active_pattern != NULL);
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes the Sound Generator Service singleton.
 *
 * @details Validates and stores the borrowed buzzer reference, marks the service
 *          uninitialized, clears all pattern progress and forces the buzzer off.
 *          The singleton becomes initialized only after the disable operation
 *          succeeds.
 *
 * @param   Buzzer - Pointer to a caller-owned, initialized Buzzer Driver.
 *
 * @note    Reinitialization replaces the retained dependency and resets logical
 *          pattern state. Ownership of Buzzer remains with the composition root.
 *
 * @return  SGS_OPERATION_OK   - When the buzzer was retained and disabled;
 *          SGS_OPERATION_FAIL - When Buzzer is null or cannot be disabled. The
 *                               retained reference is cleared after a disable
 *                               failure.
 */
SGS_OpStatus_t SGS_Init(Buzzer_Handle_t* Buzzer)
{
    if(Buzzer == NULL)
    {
        return SGS_OPERATION_FAIL;
    }

    SGS_Runtime_Instance.buzzer      = Buzzer;
    SGS_Runtime_Instance.initialized = false;

    SGS_ClearPatternState();

    if(Buzzer_Off(SGS_Runtime_Instance.buzzer) != BUZZER_OPERATION_OK)
    {
        SGS_Runtime_Instance.buzzer = NULL;

        return SGS_OPERATION_FAIL;
    }

    SGS_Runtime_Instance.initialized = true;

    return SGS_OPERATION_OK;
}

/**
 * @brief   Requests a semantic ringtone at a supplied timestamp.
 *
 * @details Validates and resolves the ringtone, advances any active pattern to
 *          CurrentTimeMs and applies the replacement-priority policy. An accepted
 *          request becomes the active pattern at phase zero and its first phase
 *          is applied immediately.
 *
 * @param   Ringtone      - Semantic ringtone identifier to request.
 * @param   CurrentTimeMs - Current timestamp from the monotonic millisecond time
 *                          base used for subsequent updates.
 *
 * @note    A lower-priority request leaves the active pattern unchanged and
 *          returns SGS_OPERATION_IGNORED. Equal or higher priority replaces it.
 *
 * @return  SGS_OPERATION_OK      - When the requested pattern started;
 *          SGS_OPERATION_IGNORED - When a higher-priority pattern remains active;
 *          SGS_OPERATION_FAIL    - When validation, active-pattern synchronization
 *                                  or a required buzzer operation fails.
 */
SGS_OpStatus_t SGS_Ring(SGS_Ringtone_t Ringtone, uint32_t CurrentTimeMs)
{
    if(!SGS_Runtime_Instance.initialized ||
       (uint32_t)Ringtone >= (uint32_t)SGS_RINGTONE_COUNT)
    {
        return SGS_OPERATION_FAIL;
    }

    const SGS_Pattern_t* requested_pattern = &SGS_PatternMap[Ringtone];

    if(requested_pattern->phases == NULL || requested_pattern->phase_count == 0U)
    {
        return SGS_OPERATION_FAIL;
    }

    if(SGS_IsActive() &&
       SGS_Update(CurrentTimeMs) != SGS_OPERATION_OK)
    {
        return SGS_OPERATION_FAIL;
    }

    if(SGS_IsActive() &&
       requested_pattern->priority < (SGS_Runtime_Instance.active_pattern->priority))
    {
        return SGS_OPERATION_IGNORED;
    }

    SGS_Runtime_Instance.active_pattern      = requested_pattern;
    SGS_Runtime_Instance.current_phase_index = 0U;
    SGS_Runtime_Instance.phase_started_ms    = CurrentTimeMs;

    if(SGS_ApplyPhase(&requested_pattern->phases[0]) != SGS_OPERATION_OK)
    {
        (void)SGS_AbortPattern();

        return SGS_OPERATION_FAIL;
    }

    return SGS_OPERATION_OK;
}

/**
 * @brief   Advances the active pattern to a supplied timestamp.
 *
 * @details Uses TVS_HasElapsed() to advance across every expired phase while
 *          incrementing the stored nominal start by configured durations. It
 *          applies only the phase that shall currently be active, or aborts the
 *          pattern after its final phase.
 *
 * @param   CurrentTimeMs - Current monotonic timestamp in milliseconds.
 *
 * @note    Delayed calls skip expired intermediate phases rather than replaying
 *          them. Advancing the nominal timeline prevents update jitter from
 *          accumulating in the pattern duration.
 *
 * @return  SGS_OPERATION_OK   - When the singleton is idle or the pattern was
 *                               advanced successfully;
 *          SGS_OPERATION_FAIL - When the service is not initialized, a phase
 *                               has zero duration or a required buzzer operation
 *                               fails.
 */
SGS_OpStatus_t SGS_Update(uint32_t CurrentTimeMs)
{
    if(!SGS_Runtime_Instance.initialized)
    {
        return SGS_OPERATION_FAIL;
    }

    if(!SGS_IsActive())
    {
        return SGS_OPERATION_OK;
    }

    bool phase_changed = false;

    while(SGS_Runtime_Instance.current_phase_index < (SGS_Runtime_Instance.active_pattern->phase_count))
    {
        const SGS_Phase_t* current_phase =
            &(SGS_Runtime_Instance.active_pattern->phases[SGS_Runtime_Instance.current_phase_index]);

        if(current_phase->duration_ms == 0U)
        {
            (void)SGS_AbortPattern();

            return SGS_OPERATION_FAIL;
        }

        if(!TVS_HasElapsed(SGS_Runtime_Instance.phase_started_ms, CurrentTimeMs, current_phase->duration_ms))
        {
            break;
        }

        SGS_Runtime_Instance.phase_started_ms += current_phase->duration_ms;

        SGS_Runtime_Instance.current_phase_index++;

        phase_changed = true;

        if(SGS_Runtime_Instance.current_phase_index >= (SGS_Runtime_Instance.active_pattern->phase_count))
        {
            return SGS_AbortPattern();
        }
    }

    if(phase_changed)
    {
        const SGS_Phase_t* next_phase =
            &(SGS_Runtime_Instance.active_pattern->phases[SGS_Runtime_Instance.current_phase_index]);

        if(SGS_ApplyPhase(next_phase) != SGS_OPERATION_OK)
        {
            (void)SGS_AbortPattern();

            return SGS_OPERATION_FAIL;
        }
    }

    return SGS_OPERATION_OK;
}

/**
 * @brief   Cancels the active pattern and disables the buzzer output.
 *
 * @details Aborts the current pattern through SGS_AbortPattern(). The buzzer is
 *          forced off even when the singleton is already idle, and logical
 *          pattern state is cleared regardless of the disable result.
 *
 * @note    The retained buzzer association and initialization state are
 *          preserved for subsequent ringtone requests.
 *
 * @return  SGS_OPERATION_OK   - When the buzzer was disabled successfully;
 *          SGS_OPERATION_FAIL - When the service is not initialized or the
 *                               buzzer cannot be disabled.
 */
SGS_OpStatus_t SGS_Stop(void)
{
    if(!SGS_Runtime_Instance.initialized)
    {
        return SGS_OPERATION_FAIL;
    }

    return SGS_AbortPattern();
}
