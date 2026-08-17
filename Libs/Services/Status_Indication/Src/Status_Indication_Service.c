/**********************************************************************************************************************************
 * @file    Status_Indication_Service.c
 * @brief   Status Indication Service implementation.
 *
 * @details Implements instance-based, non-blocking semantic LED patterns
 *          through an injected LED Driver. Service phase timing is sampled
 *          from the Time Platform Interface and evaluated through the
 *          rollover-safe Timeout Validation Service. The injected LED Driver
 *          progresses its effects from the same platform time base.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 17, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Status_Indication_Service.h"
#include "Timeout_Validation_Service.h"
#include "Time_Platform_Interface.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**
 * @brief   Returns the number of elements in a compile-time array.
 *
 * @details Divides the complete array size by the size of its first element and
 *          converts the result to uint8_t before assignment to the pattern phase
 *          count.
 *
 * @param   Array - Compile-time array whose element count is required.
 *
 * @note    Array shall be an actual array expression, not a pointer. Counts
 *          above UINT8_MAX are truncated by the explicit cast.
 */
#define SIS_ARRAY_LENGTH(Array) ((uint8_t)(sizeof(Array) / sizeof((Array)[0])))

/**
 * @brief   Compile-time LED effect parameters used by semantic indications.
 *
 * @details Defines transition intervals and repetition requests forwarded to
 *          LED_TriggerEffect() when each service phase is applied.
 *
 */

/** @brief Reserved static-effect interval for the locked indication. */
#define LOCKED_INDICATION_LED_EFFECT_INTERVAL_MS         ( 0U  )

/** @brief Reserved static-effect repeat count for the locked indication. */
#define LOCKED_INDICATION_LED_EFFECT_REPEATS             ( 0U  )

/** @brief Transition interval used by the access-granted flash effect.   */
#define ACCESS_GRANTED_INDICATION_LED_EFFECT_INTERVAL_MS ( 55U )

/** @brief Repetition request used by the access-granted flash effect.    */
#define ACCESS_GRANTED_INDICATION_LED_EFFECT_REPEATS     ( 7U )

/** @brief Transition interval used by access-denied pulse phases.        */
#define ACCESS_DENIED_INDICATION_LED_EFFECT_INTERVAL_MS  ( 100U )

/** @brief Repetition request used by access-denied pulse phases.         */
#define ACCESS_DENIED_INDICATION_LED_EFFECT_REPEATS      ( 1U  )

/** @brief Transition interval used by lockout-entry pulse phases.        */
#define LOCKOUT_ENTRY_INDICATION_LED_EFFECT_INTERVAL_MS  ( 100U )

/** @brief Repetition request used by lockout-entry pulse phases.         */
#define LOCKOUT_ENTRY_INDICATION_LED_EFFECT_REPEATS      ( 1U  )

/** @brief Static-effect interval currently used by the locked phase.     */
#define LOCKOUT_INDICATION_LED_EFFECT_INTERVAL_MS        ( 0U  )

/** @brief Static-effect repeat count currently used by the locked phase. */
#define LOCKOUT_INDICATION_LED_EFFECT_REPEATS            ( 0U  ) 

/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**
 * @brief   Stable LED-off pattern used for the normal locked indication.
 *
 * @details Contains one static LED-off phase with zero service duration. The
 *          phase is applied immediately and marked complete on the next update,
 *          while the LED remains in its static off state.
 *
 * @note    The initializer uses the dedicated locked-indication effect
 *          constants.
 */
static const SIS_Phase_t SIS_LockedPhases[] =
{
    {   
        ._led_state          = LED_STATE_OFF,
        ._effect             = LED_EFFECT_STATIC, 
        ._effect_interval_ms = LOCKED_INDICATION_LED_EFFECT_INTERVAL_MS,
        ._effect_repeats     = LOCKED_INDICATION_LED_EFFECT_REPEATS,
        ._duration_ms        = 0U
    },
};

 /**
 * @brief   Flash pattern used for the access-granted indication.
 *
 * @details Starts from LED off and triggers LED_EFFECT_FLASH with a 55 ms
 *          transition interval and seven requested repetitions. The single
 *          service phase has a nominal duration of 770 ms.
 *
 * @note    The LED Driver internally expands each flash repetition into two
 *          transitions. Fourteen transitions at 55 ms produce the same nominal
 *          770 ms timeline configured for this service phase.
 */
static const SIS_Phase_t SIS_AccessGrantedPhases[] =
{
    {  
        ._led_state          = LED_STATE_OFF, 
        ._effect             = LED_EFFECT_FLASH, 
        ._effect_interval_ms = ACCESS_GRANTED_INDICATION_LED_EFFECT_INTERVAL_MS,
        ._effect_repeats     = ACCESS_GRANTED_INDICATION_LED_EFFECT_REPEATS,
        ._duration_ms        = 770U
    },
};

/**
 * @brief   Two-pulse pattern used for the access-denied indication.
 *
 * @details Contains two equivalent LED-off pulse phases. Each phase requests a
 *          100 ms transition interval, one repetition and a nominal service
 *          duration of 220 ms, producing 440 ms of service-level sequencing.
 *
 * @note    LED_EFFECT_PULSE is forced to one repetition by the LED Driver.
 */
static const SIS_Phase_t SIS_AccessDeniedPhases[] =
{
    {   
        ._led_state          = LED_STATE_OFF,
        ._effect             = LED_EFFECT_PULSE, 
        ._effect_interval_ms = ACCESS_DENIED_INDICATION_LED_EFFECT_INTERVAL_MS,
        ._effect_repeats     = ACCESS_DENIED_INDICATION_LED_EFFECT_REPEATS,
        ._duration_ms        = 220U
    },

    {   
        ._led_state          = LED_STATE_OFF,
        ._effect             = LED_EFFECT_PULSE, 
        ._effect_interval_ms = ACCESS_DENIED_INDICATION_LED_EFFECT_INTERVAL_MS,
        ._effect_repeats     = ACCESS_DENIED_INDICATION_LED_EFFECT_REPEATS,
        ._duration_ms        = 220U
    },
};

/**
 * @brief   Accelerating pulse sequence used when entering lockout.
 *
 * @details Contains seven LED-off pulse phases with nominal durations of 220,
 *          210, 200, 190, 190, 170 and 160 ms. A final zero-duration static
 *          LED-on phase leaves the indicator illuminated after the sequence.
 *
 * @note    Every pulse phase requests a 100 ms interval and one repetition. The
 *          seven timed phases total 1340 ms before the static terminal phase.
 *          That terminal phase also forwards the lockout-entry interval and
 *          repetition values even though its selected effect is static.
 */
static const SIS_Phase_t SIS_LockoutEntryPhases[] =
{
    {
        ._led_state          = LED_STATE_OFF,
        ._effect             = LED_EFFECT_PULSE,
        ._effect_interval_ms = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_INTERVAL_MS,
        ._effect_repeats     = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_REPEATS,
        ._duration_ms        = 220U
    },

    {
        ._led_state          = LED_STATE_OFF,
        ._effect             = LED_EFFECT_PULSE,
        ._effect_interval_ms = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_INTERVAL_MS,
        ._effect_repeats     = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_REPEATS,
        ._duration_ms        = 210U
    },

    {
        ._led_state          = LED_STATE_OFF,
        ._effect             = LED_EFFECT_PULSE,
        ._effect_interval_ms = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_INTERVAL_MS,
        ._effect_repeats     = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_REPEATS,
        ._duration_ms        = 200U
    },

    {
        ._led_state          = LED_STATE_OFF,
        ._effect             = LED_EFFECT_PULSE,
        ._effect_interval_ms = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_INTERVAL_MS,
        ._effect_repeats     = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_REPEATS,
        ._duration_ms        = 190U
    },

    {
        ._led_state          = LED_STATE_OFF,
        ._effect             = LED_EFFECT_PULSE,
        ._effect_interval_ms = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_INTERVAL_MS,
        ._effect_repeats     = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_REPEATS,
        ._duration_ms        = 190U
    },

    {
        ._led_state          = LED_STATE_OFF,
        ._effect             = LED_EFFECT_PULSE,
        ._effect_interval_ms = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_INTERVAL_MS,
        ._effect_repeats     = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_REPEATS,
        ._duration_ms        = 170U
    },

    {
        ._led_state          = LED_STATE_OFF,
        ._effect             = LED_EFFECT_PULSE,
        ._effect_interval_ms = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_INTERVAL_MS,
        ._effect_repeats     = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_REPEATS,
        ._duration_ms        = 160U
    },

    {
        ._led_state          = LED_STATE_ON,
        ._effect             = LED_EFFECT_STATIC,
        ._effect_interval_ms = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_INTERVAL_MS,
        ._effect_repeats     = LOCKOUT_ENTRY_INDICATION_LED_EFFECT_REPEATS,
        ._duration_ms        = 0U
    }
};

/**
 * @brief   Semantic indication-to-pattern map.
 *
 * @details Uses SIS_Indication_t values as designated indexes so each playable
 *          public indication resolves to its immutable phase array and count.
 *
 * @note    SIS_INDICATION_COUNT sizes the array and is not a valid map index.
 */
static const SIS_Pattern_t SIS_PatternMap[SIS_INDICATION_COUNT] = 
{
    [SIS_INDICATION_LOCKED] =
    {
        ._phases      = SIS_LockedPhases,
        ._phase_count = SIS_ARRAY_LENGTH(SIS_LockedPhases)
    },
    
    [SIS_INDICATION_ACCESS_GRANTED] =
    {
        ._phases      = SIS_AccessGrantedPhases,
        ._phase_count = SIS_ARRAY_LENGTH(SIS_AccessGrantedPhases)
    },

    [SIS_INDICATION_ACCESS_DENIED] =
    {
        ._phases      = SIS_AccessDeniedPhases,
        ._phase_count = SIS_ARRAY_LENGTH(SIS_AccessDeniedPhases)
    },

    [SIS_INDICATION_LOCKOUT_ENTRY] =
    {
        ._phases      = SIS_LockoutEntryPhases,
        ._phase_count = SIS_ARRAY_LENGTH(SIS_LockoutEntryPhases)
    }
};

/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
static SIS_OpStatus_t SIS_ApplyPhase (SIS_Handle_t* Instance, const SIS_Phase_t* Phase);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Reports whether a service instance completed initialization.
 *
 * @details Returns the instance lifecycle flag after first validating the
 *          pointer. Despite its name, this helper does not test whether the
 *          selected pattern still has an unfinished phase.
 *
 * @param   Instance - Pointer to the service instance to inspect.
 *
 * @note    This helper does not modify service state or access the LED Driver.
 *
 * @return  true when Instance is non-null and initialized; otherwise false.
 */
static inline bool SIS_IsActive(const SIS_Handle_t* Instance)
{
    return (Instance == NULL) ? false : Instance->_initialized;
}

/**
 * @brief   Applies one service phase to the injected LED Driver.
 *
 * @details First applies the phase logical state through LED_On() or LED_Off(),
 *          then forwards the effect, transition interval and repetition request
 *          to LED_TriggerEffect().
 *
 * @param   Instance - Pointer to the service instance owning the LED reference.
 * @param   Phase    - Pointer to the immutable phase to apply.
 *
 * @note    If effect triggering fails after the state change, the physical LED
 *          may already reflect the requested initial state.
 *
 * @return  SIS_OPERATION_OK   - When the state and effect were applied;
 *          SIS_OPERATION_FAIL - When an argument is null or an LED Driver
 *                               operation fails.
 */
static SIS_OpStatus_t SIS_ApplyPhase(SIS_Handle_t* Instance, const SIS_Phase_t* Phase)
{
    if(Instance == NULL || Phase == NULL)
    {
        return SIS_OPERATION_FAIL;
    }

    if(Phase->_led_state == LED_STATE_ON)
    {
        if(LED_On(Instance->_led) != LED_OPERATION_OK)
        {
            return SIS_OPERATION_FAIL;
        }
    }

    else
    {
        if(LED_Off(Instance->_led) != LED_OPERATION_OK)
        {
            return SIS_OPERATION_FAIL;
        }
    }

    if(LED_TriggerEffect(Instance->_led, 
                            Phase->_effect, 
                            Phase->_effect_interval_ms, 
                            Phase->_effect_repeats) != LED_OPERATION_OK)
    {
        return SIS_OPERATION_FAIL;
    }

    return SIS_OPERATION_OK;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes one Status Indication Service instance.
 *
 * @details Retains the caller-owned LED reference, selects the locked pattern,
 *          resets phase progression and marks the instance uninitialized. It
 *          applies the first locked phase and enables the lifecycle flag only
 *          after that operation succeeds.
 *
 * @param   Instance - Pointer to caller-owned service storage.
 * @param   Led      - Pointer to a caller-owned, initialized LED Driver.
 *
 * @note    Ownership of both objects remains with the composition root. Calling
 *          this function again resets the supplied instance to locked state.
 *
 * @return  SIS_OPERATION_OK   - When the instance and locked phase initialize;
 *          SIS_OPERATION_FAIL - When an argument is null or an LED Driver
 *                               operation fails.
 */
SIS_OpStatus_t SIS_Init(SIS_Handle_t* Instance, LED_Handle_t* Led)
{
    if(Instance == NULL || Led == NULL)
    {
        return SIS_OPERATION_FAIL;
    }

    Instance->_led                 = Led;
    Instance->_active_pattern      = &SIS_PatternMap[SIS_INDICATION_LOCKED];
    Instance->_phase_started_ms    = 0U;
    Instance->_current_phase_index = 0;
    Instance->_initialized         = false;

    if(SIS_ApplyPhase(Instance, Instance->_active_pattern->_phases) != SIS_OPERATION_OK)
    {
        return SIS_OPERATION_FAIL;
    }

    Instance->_initialized = true;

    return SIS_OPERATION_OK;
}

/**
 * @brief   Selects and immediately starts a semantic indication pattern.
 *
 * @details Validates the instance and requested pattern, applies phase zero
 *          synchronously and commits the new pattern only after successful
 *          application. The phase-start timestamp is then sampled through
 *          Platform_GetMillis() and the phase index is reset.
 *
 * @param   Instance   - Pointer to an initialized service instance.
 * @param   Indication - Semantic pattern identifier to select.
 *
 * @note    SIS_INDICATION_COUNT is rejected as the non-playable pattern-map
 *          boundary.
 *
 * @return  SIS_OPERATION_OK   - When the pattern and first phase were applied;
 *          SIS_OPERATION_FAIL - When validation, lookup or an LED Driver
 *                               operation fails.
 */
SIS_OpStatus_t SIS_SetIndication(SIS_Handle_t* Instance, SIS_Indication_t Indication)
{
    if(Instance == NULL || Indication >= SIS_INDICATION_COUNT || !SIS_IsActive(Instance))
    {
        return SIS_OPERATION_FAIL;
    }

    const SIS_Pattern_t* requested_pattern = &(SIS_PatternMap[Indication]);

    if(requested_pattern->_phases == NULL || requested_pattern->_phase_count == 0)
    {
        return SIS_OPERATION_FAIL;
    }

    if(SIS_ApplyPhase(Instance, &requested_pattern->_phases[0]) != SIS_OPERATION_OK)
    {
        return SIS_OPERATION_FAIL;
    }

    Instance->_active_pattern      = requested_pattern;
    Instance->_phase_started_ms    = Platform_GetMillis();
    Instance->_current_phase_index = 0;

    return SIS_OPERATION_OK;
}

/**
 * @brief   Updates the LED effect and advances indication pattern state.
 *
 * @details Calls LED_Update() before inspecting the current service phase. When
 *          a phase is pending, the function samples Platform_GetMillis(). A
 *          nonzero-duration phase advances after TVS_HasElapsed() reports its
 *          nominal duration, while a zero-duration phase advances immediately.
 *          When a next phase exists, it is applied during the same call.
 *
 * @param   Instance - Pointer to an initialized service instance.
 *
 * @note    At most one service phase advances per call. The nominal phase start
 *          is incremented by the configured duration, so repeated calls can
 *          catch up without moving the original pattern timeline.
 *
 * @return  SIS_OPERATION_OK   - When the LED and phase update complete;
 *          SIS_OPERATION_FAIL - When the instance is invalid or an LED Driver
 *                               operation fails.
 */
SIS_OpStatus_t SIS_Update(SIS_Handle_t* Instance)
{
    if(Instance == NULL || !SIS_IsActive(Instance))
    {
        return SIS_OPERATION_FAIL;
    }

    if(LED_Update(Instance->_led) != LED_OPERATION_OK)
    {
        return SIS_OPERATION_FAIL;
    }

    bool phase_changed = false;

    const SIS_Pattern_t* current_pattern = Instance->_active_pattern;

    if(Instance->_current_phase_index < current_pattern->_phase_count)
    {
        uint32_t now = Platform_GetMillis();

        const SIS_Phase_t* current_phase = &(current_pattern->_phases[Instance->_current_phase_index]);

        if(current_phase->_duration_ms != 0U)
        {
            if(!TVS_HasElapsed(Instance->_phase_started_ms, now, current_phase->_duration_ms))
            {
                return SIS_OPERATION_OK;
            }
        }
        
        phase_changed = true;

        Instance->_phase_started_ms += current_phase->_duration_ms;

        Instance->_current_phase_index++;

        if(Instance->_current_phase_index >= current_pattern->_phase_count)
        {
            return SIS_OPERATION_OK;
        }
    }

    if(phase_changed)
    {
        const SIS_Phase_t* next_phase = &(current_pattern->_phases[Instance->_current_phase_index]);

        if(SIS_ApplyPhase(Instance, next_phase) != SIS_OPERATION_OK)
        {
            return SIS_OPERATION_FAIL;
        }
    }

    return SIS_OPERATION_OK;
}
