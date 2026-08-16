/**********************************************************************************************************************************
 * @file    Sound_Generator_Service.c
 * @brief   Sound Generator Service implementation.
 *
 * @details Implements non-blocking, priority-based sound patterns through an
 *          injected Buzzer Driver instance. Pattern timing uses caller-supplied
 *          millisecond timestamps and remains safe across one unsigned 32-bit
 *          timestamp rollover.
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
#define SGS_ARRAY_LENGTH(Array) ((uint8_t)(sizeof(Array) / sizeof((Array)[0])))

/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**
 * @brief Short acknowledgement emitted for each accepted key press.
 */
static const SGS_Phase_t SGS_KeypressPhases[] =
{
    { .FrequencyHz = 2600U, .DurationMs =  40U, .OutputEnabled = true }
};

/**
 * @brief Rising two-tone pattern emitted after access is granted.
 */
static const SGS_Phase_t SGS_AccessGrantedPhases[] =
{
    { .FrequencyHz = 2000U, .DurationMs =  80U, .OutputEnabled = true  },
    { .FrequencyHz =    0U, .DurationMs =  40U, .OutputEnabled = false },
    { .FrequencyHz = 3000U, .DurationMs = 140U, .OutputEnabled = true  }
};

/**
 * @brief Descending two-tone pattern used for invalid or incomplete input.
 */
static const SGS_Phase_t SGS_ErrorPhases[] =
{
    { .FrequencyHz = 2600U, .DurationMs = 120U, .OutputEnabled = true  },
    { .FrequencyHz =    0U, .DurationMs =  50U, .OutputEnabled = false },
    { .FrequencyHz = 1800U, .DurationMs = 220U, .OutputEnabled = true  }
};

/**
 * @brief Semantic ringtone-to-pattern map.
 *
 * @details Feedback patterns have a higher replacement priority than the key
 *          acknowledgement. Patterns at the same priority replace one another.
 */
static const SGS_Pattern_t SGS_PatternMap[SGS_RINGTONE_COUNT] =
{
    [SGS_RINGTONE_KEYPRESS] =
    {
        .Phases     = SGS_KeypressPhases,
        .PhaseCount = SGS_ARRAY_LENGTH(SGS_KeypressPhases),
        .Priority   = SGS_PRIORITY_KEYPRESS
    },

    [SGS_RINGTONE_ACCESS_GRANTED] =
    {
        .Phases     = SGS_AccessGrantedPhases,
        .PhaseCount = SGS_ARRAY_LENGTH(SGS_AccessGrantedPhases),
        .Priority   = SGS_PRIORITY_FEEDBACK
    },

    [SGS_RINGTONE_ERROR] =
    {
        .Phases     = SGS_ErrorPhases,
        .PhaseCount = SGS_ARRAY_LENGTH(SGS_ErrorPhases),
        .Priority   = SGS_PRIORITY_FEEDBACK
    }
};

/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
static void           SGS_ClearPatternState (SGS_Handle_t* Instance);
static SGS_OpStatus_t SGS_ApplyPhase        (SGS_Handle_t* Instance, const SGS_Phase_t* Phase);
static SGS_OpStatus_t SGS_AbortPattern      (SGS_Handle_t* Instance);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Clears only the active-pattern runtime state.
 *
 * @note    The injected buzzer association and initialization state are
 *          preserved.
 */
static void SGS_ClearPatternState(SGS_Handle_t* Instance)
{
    Instance->_active_pattern      = NULL;
    Instance->_phase_started_ms    = 0U;
    Instance->_current_phase_index = 0U;
}

/**
 * @brief   Applies one pattern phase to the injected buzzer.
 *
 * @param   Instance - Pointer to an initialized Sound Generator instance.
 * @param   Phase    - Pointer to the phase that shall become active.
 *
 * @return  SGS_OPERATION_OK when the phase was applied successfully;
 *          otherwise SGS_OPERATION_FAIL.
 */
static SGS_OpStatus_t SGS_ApplyPhase(SGS_Handle_t* Instance, const SGS_Phase_t* Phase)
{
    if(Instance == NULL || Phase == NULL || Instance->_buzzer == NULL)
    {
        return SGS_OPERATION_FAIL;
    }

    if(!Phase->OutputEnabled)
    {
        return (Buzzer_Off(Instance->_buzzer) == BUZZER_OPERATION_OK) ?
                                                 SGS_OPERATION_OK     :
                                                 SGS_OPERATION_FAIL   ;
    }

    if(Phase->FrequencyHz == 0U)
    {
        return SGS_OPERATION_FAIL;
    }

    if(Buzzer_SetFrequency(Instance->_buzzer, Phase->FrequencyHz) != BUZZER_OPERATION_OK)
    {
        return SGS_OPERATION_FAIL;
    }

    if(Buzzer_On(Instance->_buzzer) != BUZZER_OPERATION_OK)
    {
        return SGS_OPERATION_FAIL;
    }

    return SGS_OPERATION_OK;
}

/**
 * @brief   Forces the buzzer off and clears the current pattern.
 *
 * @details Runtime state is cleared even if the underlying disable operation
 *          fails, preventing a failed pattern from continuing logically.
 */
static SGS_OpStatus_t SGS_AbortPattern(SGS_Handle_t* Instance)
{
    Buzzer_OpStatus_t buzzer_status = Buzzer_Off(Instance->_buzzer);

    SGS_ClearPatternState(Instance);

    return (buzzer_status == BUZZER_OPERATION_OK) ? SGS_OPERATION_OK : SGS_OPERATION_FAIL;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes one Sound Generator Service instance.
 *
 * @details Stores the caller-owned Buzzer Driver reference, forces the output
 *          to its safe disabled state and initializes the pattern state as idle.
 *          Ownership of the buzzer is not transferred.
 *
 * @param   Instance - Pointer to caller-owned Sound Generator storage.
 * @param   Buzzer   - Pointer to a previously initialized Buzzer Driver.
 *
 * @return  SGS_OPERATION_OK when initialization succeeds; otherwise
 *          SGS_OPERATION_FAIL.
 */
SGS_OpStatus_t SGS_Init(SGS_Handle_t* Instance, Buzzer_Handle_t* Buzzer)
{
    if(Instance == NULL || Buzzer == NULL)
    {
        return SGS_OPERATION_FAIL;
    }

    Instance->_buzzer      = Buzzer;
    Instance->_initialized = false;

    SGS_ClearPatternState(Instance);

    if(Buzzer_Off(Instance->_buzzer) != BUZZER_OPERATION_OK)
    {
        Instance->_buzzer = NULL;

        return SGS_OPERATION_FAIL;
    }

    Instance->_initialized = true;

    return SGS_OPERATION_OK;
}

/**
 * @brief   Starts or requests replacement of a semantic sound pattern.
 *
 * @details The first pattern phase is applied immediately. A request whose
 *          priority is lower than the active pattern is ignored. Equal or
 *          higher priority requests replace the active pattern.
 *
 * @param   Instance      - Pointer to an initialized Sound Generator instance.
 * @param   Ringtone      - Semantic pattern to start.
 * @param   CurrentTimeMs - Current monotonically increasing timestamp in ms.
 *
 * @return  SGS_OPERATION_OK      - When the requested pattern starts;
 *          SGS_OPERATION_IGNORED - When a higher-priority pattern remains active;
 *          SGS_OPERATION_FAIL    - When parameters are invalid or buzzer control fails.
 */
SGS_OpStatus_t SGS_Ring(SGS_Handle_t* Instance, SGS_Ringtone_t Ringtone, uint32_t CurrentTimeMs)
{
    if(Instance == NULL || !Instance->_initialized ||
       (uint32_t)Ringtone >= (uint32_t)SGS_RINGTONE_COUNT)
    {
        return SGS_OPERATION_FAIL;
    }

    const SGS_Pattern_t* requested_pattern = &SGS_PatternMap[Ringtone];

    if(requested_pattern->Phases == NULL || requested_pattern->PhaseCount == 0U)
    {
        return SGS_OPERATION_FAIL;
    }

    if(SGS_IsActive(Instance) &&
       SGS_Update(Instance, CurrentTimeMs) != SGS_OPERATION_OK)
    {
        return SGS_OPERATION_FAIL;
    }

    if(SGS_IsActive(Instance) &&
       requested_pattern->Priority < Instance->_active_pattern->Priority)
    {
        return SGS_OPERATION_IGNORED;
    }

    Instance->_active_pattern      = requested_pattern;
    Instance->_current_phase_index = 0U;
    Instance->_phase_started_ms    = CurrentTimeMs;

    if(SGS_ApplyPhase(Instance, &requested_pattern->Phases[0]) != SGS_OPERATION_OK)
    {
        (void)SGS_AbortPattern(Instance);

        return SGS_OPERATION_FAIL;
    }

    return SGS_OPERATION_OK;
}

/**
 * @brief   Advances the active pattern according to elapsed time.
 *
 * @details Every expired phase advances the nominal phase-start timestamp by
 *          its configured duration instead of assigning CurrentTimeMs. This
 *          prevents update jitter from accumulating across a pattern.
 *
 *          If more than one phase expired between calls, all expired phases are
 *          skipped and only the phase that should currently be active is applied.
 *          Therefore delayed application execution does not stretch a pattern.
 *
 * @param   Instance      - Pointer to an initialized Sound Generator instance.
 * @param   CurrentTimeMs - Current monotonically increasing timestamp in ms.
 *
 * @return  SGS_OPERATION_OK when the update completes successfully; otherwise
 *          SGS_OPERATION_FAIL.
 */
SGS_OpStatus_t SGS_Update(SGS_Handle_t* Instance, uint32_t CurrentTimeMs)
{
    if(Instance == NULL || !Instance->_initialized)
    {
        return SGS_OPERATION_FAIL;
    }

    if(!SGS_IsActive(Instance))
    {
        return SGS_OPERATION_OK;
    }

    bool phase_changed = false;

    while(Instance->_current_phase_index < Instance->_active_pattern->PhaseCount)
    {
        const SGS_Phase_t* current_phase =
            &Instance->_active_pattern->Phases[Instance->_current_phase_index];

        if(current_phase->DurationMs == 0U)
        {
            (void)SGS_AbortPattern(Instance);

            return SGS_OPERATION_FAIL;
        }

        if(!TVS_HasElapsed(Instance->_phase_started_ms,
                           CurrentTimeMs,
                           current_phase->DurationMs))
        {
            break;
        }

        Instance->_phase_started_ms += current_phase->DurationMs;
        Instance->_current_phase_index++;
        phase_changed = true;

        if(Instance->_current_phase_index >= Instance->_active_pattern->PhaseCount)
        {
            return SGS_AbortPattern(Instance);
        }
    }

    if(phase_changed)
    {
        const SGS_Phase_t* next_phase =
            &Instance->_active_pattern->Phases[Instance->_current_phase_index];

        if(SGS_ApplyPhase(Instance, next_phase) != SGS_OPERATION_OK)
        {
            (void)SGS_AbortPattern(Instance);

            return SGS_OPERATION_FAIL;
        }
    }

    return SGS_OPERATION_OK;
}

/**
 * @brief   Cancels the active pattern and disables the buzzer output.
 *
 * @return  SGS_OPERATION_OK when the buzzer is disabled successfully;
 *          otherwise SGS_OPERATION_FAIL.
 */
SGS_OpStatus_t SGS_Stop(SGS_Handle_t* Instance)
{
    if(Instance == NULL || !Instance->_initialized)
    {
        return SGS_OPERATION_FAIL;
    }

    return SGS_AbortPattern(Instance);
}

/**
 * @brief   Reports whether a sound pattern is currently active.
 *
 * @return  true when Instance is initialized and owns an active pattern;
 *          otherwise false.
 */
bool SGS_IsActive(const SGS_Handle_t* Instance)
{
    return (Instance != NULL &&
            Instance->_initialized &&
            Instance->_active_pattern != NULL);
}
