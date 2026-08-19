/**********************************************************************************************************************************
 * @file    Credential_Entry_Service.h
 * @brief   Public interface of the Credential Entry Service.
 *
 * @details The Credential Entry Service is a synchronous, hardware-independent
 *          domain module responsible for managing one active candidate
 *          credential-entry session.
 *
 *          The service receives semantic input commands from the application
 *          layer, stores accepted decimal digits in a fixed-size internal
 *          buffer and returns domain events that describe the outcome of each
 *          processed command.
 * 
 *          When a complete candidate is requested, the service copies its
 *          internal credential digits into a caller-provided destination
 *          buffer. Ownership of that destination storage remains with the
 *          application.
 *
 *          The service does not access the Matrix Keyboard Driver and does not
 *          interpret physical key codes, electrical levels or hardware-specific
 *          keyboard actions. Translation from a keyboard output to CES input
 *          commands is an application-layer responsibility.
 *
 *          The service does not validate a completed candidate credential,
 *          manage inactivity timeouts, count failed authentication attempts,
 *          apply lockout policy, control the lock actuator or produce display,
 *          sound or status-indication side effects. These responsibilities
 *          belong respectively to other services and to the Lock Controller.
 *
 *          The application shall process CES events through the Lock Controller,
 *          which decides the corresponding state-machine transition and
 *          application side effects.
 *
 *          The acronym CES means Credential Entry Service and is used as the
 *          public symbol prefix throughout this module.
 *
 * @note    This module does not create tasks, use RTOS primitives, access STM32
 *          peripherals or perform dynamic memory allocation.
 *
 * @note    The module owns one static internal service instance and exposes a
 *          singleton API. Its public functions shall be accessed by only one
 *          execution context at a time.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 13, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_SERVICES_CREDENTIAL_ENTRY_INC_CREDENTIAL_ENTRY_SERVICE_H_
#define LIBS_SERVICES_CREDENTIAL_ENTRY_INC_CREDENTIAL_ENTRY_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "stdint.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**
 * @brief   Fixed number of decimal digits required by the V1 credential model.
 *
 * @details Defines the capacity of the internal candidate credential buffer
 *          and the exact candidate length required for a confirm command to
 *          produce CES_EVENT_READY.
 *
 * @note    A digit input received when the candidate already contains
 *          CES_CREDENTIAL_LENGTH digits shall not modify the candidate and
 *          shall produce CES_EVENT_NONE.
 */
 #define CES_CREDENTIAL_LENGTH (6U)

/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**
 * @brief   Normalized decimal digit used by the Credential Entry Service.
 *
 * @details Represents the numeric value of one candidate credential digit.
 *          Valid digit values are in the inclusive range from 0U through 9U.
 *
 * @note    This type does not represent an ASCII character, a Matrix Keyboard
 *          key code or any other hardware-specific value.
 */
typedef uint8_t CES_Digit_t;

/**
 * @brief   Number of valid digits stored in the candidate credential.
 *
 * @details Represents a value in the inclusive range from 0U through
 *          CES_CREDENTIAL_LENGTH.
 */
typedef uint8_t CES_Length_t;

/**
 * @brief   Execution status of a Credential Entry Service API operation.
 *
 * @details Indicates whether an operation was accepted and completed according
 *          to its API preconditions.
 *
 *          This status is distinct from CES_Event_t. CES_OpStatus_t reports
 *          API-level success or failure, whereas CES_Event_t reports the
 *          domain outcome of a processed input command.
 */
typedef enum
{
    CES_OPERATION_OK,   /*< The requested operation completed successfully. */
    CES_OPERATION_FAIL  /*< The requested operation could not be completed. */

}CES_OpStatus_t;

/**
 * @brief   Semantic input command accepted by the Credential Entry Service.
 *
 * @details Identifies the meaning of an application input after translation
 *          from a physical keyboard output. The service is therefore
 *          independent from Matrix Keyboard Driver types, key maps and
 *          hardware-specific representations.
 */
typedef enum
{
    CES_INPUT_KIND_NONE,         /*< No semantic input command is available.                 */

    CES_INPUT_KIND_DIGIT,        /*< Append one numeric digit to the candidate.              */

    CES_INPUT_KIND_CONFIRM,      /*< Request completion of the current candidate.            */

    CES_INPUT_KIND_CLEAR_CANCEL  /*< Clear a non-empty candidate or cancel an empty session. */

}CES_InputKind_t;

/**
 * @brief   Semantic command processed by the Credential Entry Service.
 *
 * @details Associates an input kind with its optional normalized decimal digit.
 *
 *          When Kind is CES_INPUT_KIND_DIGIT, Digit shall contain a value from
 *          0U through 9U. For all other input kinds, Digit is ignored.
 *
 * @note    The application normally filters unsupported keyboard outputs before
 *          calling the service. The service shall nevertheless preserve its
 *          state when an invalid digit is received.
 */
typedef struct
{
    CES_InputKind_t Kind;    /*< Semantic category of the input command.             */
    CES_Digit_t     Digit;   /*< Numeric digit; valid only for CES_INPUT_KIND_DIGIT. */

}CES_Input_t;

/**
 * @brief   Domain event produced while processing a credential-entry command.
 *
 * @details Describes the observable outcome of CES_ProcessInput(). The Lock
 *          Controller interprets this event and decides the corresponding
 *          state-machine transition and application side effects.
 */
typedef enum
{
    CES_EVENT_NONE,             /*< No candidate or session state change occurred.                      */

    CES_EVENT_INPUT_ACCEPTED,   /*< A valid digit was appended to the candidate.                        */

    CES_EVENT_INCOMPLETE,       /*< Confirmation was requested before all digits were entered.          */

    CES_EVENT_READY,            /*< A complete candidate was confirmed and is ready for authentication. */

    CES_EVENT_CLEARED,          /*< A non-empty candidate was erased while the session remained active. */

    CES_EVENT_CANCELLED,        /*< An empty entry session was cancelled.                               */

}CES_Event_t;

/**
 * @brief   Caller-owned representation of a complete candidate credential.
 *
 * @details Stores a fixed-size copy of the candidate credential retrieved from
 *          the Credential Entry Service.
 *
 *          Digits contains the copied decimal digits in their original entry
 *          order. Length indicates how many elements of Digits contain valid
 *          candidate data.
 *
 *          When CES_GetCandidate() succeeds, Digits contains exactly
 *          CES_CREDENTIAL_LENGTH valid digits and Length is set to
 *          CES_CREDENTIAL_LENGTH.
 *
 *          The structure and its storage belong entirely to the caller. The
 *          service does not expose or transfer ownership of its internal
 *          candidate buffer.
 *
 * @note    A candidate shall be requested only after CES_ProcessInput()
 *          returns CES_EVENT_READY.
 *
 * @note    The copied credential remains valid independently from subsequent
 *          CES operations, including CES_EndSession().
 *
 * @warning Candidate credential data shall not be logged, displayed or
 *          retained in long-lived application storage.
 *
 * @warning The caller is responsible for erasing the complete structure after
 *          authentication processing is finished.
 */
typedef struct
{
    CES_Digit_t  Digits[CES_CREDENTIAL_LENGTH]; /*< Copied candidate digits in their original entry order. */
    CES_Length_t Length;                        /*< Number of valid digits available through Digits.       */

}CES_Candidate_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
CES_OpStatus_t CES_BeginSession     (void);
CES_Event_t    CES_ProcessInput     (const CES_Input_t* Input);
CES_Length_t   CES_GetCurrentLength (void);
CES_OpStatus_t CES_GetCandidate     (CES_Candidate_t* Candidate);
CES_OpStatus_t CES_RefreshSession   (void);
CES_OpStatus_t CES_EndSession       (void);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_SERVICES_CREDENTIAL_ENTRY_INC_CREDENTIAL_ENTRY_SERVICE_H_ */
