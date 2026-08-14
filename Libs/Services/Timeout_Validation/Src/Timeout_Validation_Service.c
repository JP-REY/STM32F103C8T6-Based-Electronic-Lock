/**********************************************************************************************************************************
 * @file    Timeout_Validation_Service.c
 * @brief   Timeout Validation Service implementation.
 *
 * @details Implements rollover-safe elapsed and remaining-time calculations
 *          for caller-owned timeout intervals expressed in milliseconds.
 *
 *          The implementation is stateless and performs no hardware access,
 *          blocking delay, time-source read, RTOS operation, event production
 *          or application state transition.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 15, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "Timeout_Validation_Service.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Calculates the elapsed duration between two millisecond timestamps.
 *
 * @details Uses unsigned subtraction so that one 32-bit timestamp rollover
 *          between StartTimestampMs and CurrentTimestampMs is handled without
 *          an absolute-deadline comparison.
 *
 * @param   StartTimestampMs   - Timestamp at which the interval started.
 * @param   CurrentTimestampMs - Timestamp at which the interval is evaluated.
 *
 * @note    The interval between both timestamps shall be shorter than one
 *          complete 32-bit timestamp-counter period.
 *
 * @return  Elapsed duration in milliseconds.
 */
static inline TVS_DurationMs_t TVS_GetElapsedMs(TVS_TimestampMs_t StartTimestampMs,
                                                TVS_TimestampMs_t CurrentTimestampMs)
{
    return (TVS_DurationMs_t)((CurrentTimestampMs) - (StartTimestampMs));
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Reports whether a caller-owned timeout interval has elapsed.
 *
 * @details Compares the rollover-safe elapsed duration with DurationMs. The
 *          exact duration boundary is considered elapsed.
 *
 * @param   StartTimestampMs   - Timestamp at which the interval started or was
 *                              last restarted.
 * @param   CurrentTimestampMs - Current timestamp supplied by the caller.
 * @param   DurationMs         - Duration of the interval to validate.
 *
 * @return  true when the configured duration has elapsed; otherwise false.
 */
bool TVS_HasElapsed(TVS_TimestampMs_t StartTimestampMs,
                    TVS_TimestampMs_t CurrentTimestampMs,
                    TVS_DurationMs_t  DurationMs)
{
    return (TVS_GetElapsedMs(StartTimestampMs, CurrentTimestampMs) >= DurationMs);
}

/**
 * @brief   Returns the remaining duration of a caller-owned timeout interval.
 *
 * @details Returns the difference between DurationMs and the rollover-safe
 *          elapsed duration while the interval remains active. The result
 *          saturates at 0U once the configured duration has elapsed.
 *
 * @param   StartTimestampMs   - Timestamp at which the interval started or was
 *                              last restarted.
 * @param   CurrentTimestampMs - Current timestamp supplied by the caller.
 * @param   DurationMs         - Duration of the interval to evaluate.
 *
 * @return  Remaining interval duration in milliseconds, saturated at 0U.
 */
TVS_DurationMs_t TVS_GetRemainingMs(TVS_TimestampMs_t StartTimestampMs,
                                    TVS_TimestampMs_t CurrentTimestampMs,
                                    TVS_DurationMs_t  DurationMs)
{
    TVS_DurationMs_t elapsed_ms = TVS_GetElapsedMs(StartTimestampMs, CurrentTimestampMs);

    return (elapsed_ms >= DurationMs) ? 0U : (DurationMs - elapsed_ms);
}
