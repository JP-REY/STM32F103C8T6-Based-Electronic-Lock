/**********************************************************************************************************************************
 * @file    Timeout_Validation_Service.h
 * @brief   Public interface of the Timeout Validation Service.
 *
 * @details The Timeout Validation Service is a synchronous, stateless and
 *          hardware-independent temporal utility responsible for performing
 *          rollover-safe timeout calculations with unsigned millisecond
 *          timestamps.
 *
 *          The service determines whether a caller-owned interval has elapsed
 *          and reports the remaining duration of that interval. Start
 *          timestamps, configured durations and timeout lifecycle state remain
 *          entirely owned by the calling module.
 *
 *          The service does not read a hardware or operating-system time
 *          source. Callers shall obtain the current timestamp through the
 *          appropriate application or Platform boundary and provide it
 *          explicitly to each operation.
 *
 *          The service does not start, restart, cancel, pause or store
 *          timeouts. It does not create tasks, software timers, queues or other
 *          RTOS objects and does not produce Lock Controller events or perform
 *          application state-machine transitions.
 *
 *          Multiple independent timeouts may be evaluated through this API
 *          without creating service instances or public handles because every
 *          operation depends only on its supplied arguments.
 *
 *          The acronym TVS means Timeout Validation Service and is used as the
 *          public symbol prefix throughout this module.
 *
 * @note    All public time units are milliseconds.
 *
 * @note    Elapsed-time calculations remain valid across one unsigned 32-bit
 *          timestamp rollover. A caller shall evaluate an interval before a
 *          complete timestamp-counter period has elapsed.
 *
 * @note    A duration of 0U is considered elapsed immediately and has 0U
 *          milliseconds remaining. Domain modules remain responsible for
 *          rejecting zero when their own policy requires a finite nonzero
 *          duration.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 15, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_SERVICES_TIMEOUT_VALIDATION_INC_TIMEOUT_VALIDATION_SERVICE_H_
#define LIBS_SERVICES_TIMEOUT_VALIDATION_INC_TIMEOUT_VALIDATION_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "stdbool.h"
#include "stdint.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   Unsigned millisecond timestamp used by the Timeout Validation
 *          Service.
 *
 * @details Represents one sample from a monotonically increasing 32-bit time
 *          source whose arithmetic wraps according to the rules of unsigned C
 *          integer arithmetic.
 *
 * @note    Timestamp values are supplied by callers. The service does not own
 *          or read the underlying time source.
 */
typedef uint32_t TVS_TimestampMs_t;

/**
 * @brief   Unsigned duration in milliseconds.
 *
 * @details Represents an elapsed, configured or remaining interval used by the
 *          Timeout Validation Service.
 */
typedef uint32_t TVS_DurationMs_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
bool             TVS_HasElapsed     (TVS_TimestampMs_t StartTimestampMs, TVS_TimestampMs_t CurrentTimestampMs, TVS_DurationMs_t  DurationMs);
TVS_DurationMs_t TVS_GetRemainingMs (TVS_TimestampMs_t StartTimestampMs, TVS_TimestampMs_t CurrentTimestampMs, TVS_DurationMs_t  DurationMs);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_SERVICES_TIMEOUT_VALIDATION_INC_TIMEOUT_VALIDATION_SERVICE_H_ */
