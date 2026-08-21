<h1 align="left">Lock Control Service</h1>

<p align="left">
  <big>
    Table-driven, hardware-independent finite-state machine for coordinating<br>
    the product-level behavior of an embedded electronic lock.
  </big>
</p>

> [!IMPORTANT]
> The Lock Control Service is a **decision component**, not an action executor. It receives semantic events, updates only its own private runtime state, and returns one semantic `LCS_Action_t` to the application. It never calls credential-entry, credential-register staging, credential storage, authentication, presentation, timeout, or lock-actuator interfaces directly. The application is the composition boundary that coordinates those elements to realize the requested action.

> [!NOTE]
> The finite-state machine is represented by a linear, immutable transition table. Existing behavior can be extended primarily by adding transition records instead of spreading transition logic across nested `switch` statements. A genuinely new state, event, guard, internal effect, or public action still requires adding the corresponding identifier and, when applicable, its small interpreter case.

---

## Table of Contents

* [1. Overview](#1-overview)
* [2. Features](#2-features)
* [3. Architecture](#3-architecture)

  * [3.1 Layer Placement](#31-layer-placement)
  * [3.2 Decision and Execution Boundary](#32-decision-and-execution-boundary)
  * [3.3 Singleton Ownership](#33-singleton-ownership)
* [4. Directory Structure](#4-directory-structure)
* [5. Service Responsibilities](#5-service-responsibilities)

  * [5.1 Responsibilities](#51-responsibilities)
  * [5.2 Explicit Non-Responsibilities](#52-explicit-non-responsibilities)
* [6. Dependencies](#6-dependencies)
* [7. Public Data Model](#7-public-data-model)

  * [7.1 Semantic Events](#71-semantic-events)
  * [7.2 Semantic Actions](#72-semantic-actions)
  * [7.3 Meaning of LCS_ACTION_NONE](#73-meaning-of-lcs_action_none)
* [8. Private Runtime Model](#8-private-runtime-model)

  * [8.1 States](#81-states)
  * [8.2 Singleton Handle](#82-singleton-handle)
  * [8.3 Pending Authentication Purpose](#83-pending-authentication-purpose)
  * [8.4 Runtime Invariants](#84-runtime-invariants)
* [9. Transition Model](#9-transition-model)

  * [9.1 Transition Record](#91-transition-record)
  * [9.2 Guards](#92-guards)
  * [9.3 Internal Effects](#93-internal-effects)
  * [9.4 Transition Table](#94-transition-table)
  * [9.5 Selection Rules](#95-selection-rules)
* [10. Processing Algorithm](#10-processing-algorithm)
* [11. Scalability and Extension](#11-scalability-and-extension)

  * [11.1 Why the Table Is Scalable](#111-why-the-table-is-scalable)
  * [11.2 Adding Behavior](#112-adding-behavior)
  * [11.3 Ordering and Ambiguity](#113-ordering-and-ambiguity)
* [12. Application Integration](#12-application-integration)

  * [12.1 Action Dispatcher](#121-action-dispatcher)
  * [12.2 Wake-Key Semantics](#122-wake-key-semantics)
  * [12.3 Authentication Feedback](#123-authentication-feedback)
  * [12.4 Timeout Ownership](#124-timeout-ownership)
* [13. Operation Flows](#13-operation-flows)

  * [13.1 Successful Access](#131-successful-access)
  * [13.2 Authorized Credential Registration](#132-authorized-credential-registration)
  * [13.3 First-Boot Registration Route](#133-first-boot-registration-route)
  * [13.4 Rejected Access and Lockout](#134-rejected-access-and-lockout)
  * [13.5 Cancellation or Entry Timeout](#135-cancellation-or-entry-timeout)
  * [13.6 Ignored Events](#136-ignored-events)
* [14. API Reference](#14-api-reference)

  * [14.1 LCS_Process](#141-lcs_process)
* [15. Design Decisions](#15-design-decisions)
* [16. Error Handling and Fail-Safe Behavior](#16-error-handling-and-fail-safe-behavior)
* [17. Timing and Concurrency](#17-timing-and-concurrency)
* [18. Security and Safety Considerations](#18-security-and-safety-considerations)
* [19. Testing and Acceptance Criteria](#19-testing-and-acceptance-criteria)
* [20. Limitations and Future Improvements](#20-limitations-and-future-improvements)
* [21. License](#21-license)

---

## 1. Overview

The Lock Control Service is the hardware-independent domain service that owns the authoritative operating state of the electronic lock, the consecutive authentication-failure policy, the credential-register confirmation-retry policy, and the purpose of the credential operation awaiting authentication.

The service processes one semantic event at a time through `LCS_Process()`. For each event, it searches an immutable transition table, evaluates the transition guard, applies a private internal effect, commits the target state, and returns a semantic action to the application.

The FSM answers two questions:

1. **What is the next product state?**
2. **What application-level operation is now required?**

For an installed credential, the same credential-entry and authentication path authorizes two distinct operations. A private
pending-request value records whether success shall grant bounded unlock or enter the first phase of credential registration.
This avoids duplicating entry and authentication states while keeping the result deterministic. Once registration is authorized,
the LCS explicitly models first entry, confirmation entry, staging validation, persistent storage and bounded success feedback.

It deliberately does not answer how that operation is physically performed. For example, granting access may require the application to coordinate:

- Lock-actuator control.
- A bounded unlock timeout.
- Display content and backlight policy.
- Status LED indication.
- Audible feedback.
- Credential-data cleanup.

Those operations cross several service and platform boundaries, so they belong to the application composition layer. The Lock Control Service communicates only the semantic intent `LCS_ACTION_GRANT_ACCESS_UNLOCK`.

This separation keeps the FSM independent from current hardware, presentation choices, and the exact collection of cooperating modules. An LCD can be replaced, an LED pattern can change, or a new sound service can be added without embedding those dependencies in the state machine.

The acronym `LCS` means **Lock Control Service** and is used as the prefix for every public symbol exposed by this module.

---

## 2. Features

- Linear, immutable, table-driven finite-state machine.
- Explicit source state, event, guard, target state, internal effect, and action per transition.
- Explicit pending-operation discriminator for authentication-success routing.
- First-match transition selection with bounded linear lookup.
- Semantic event input independent from hardware representations.
- Semantic action output independent from action implementation.
- Private states, guards, transition records, internal effects, and runtime handle.
- Consecutive authentication-failure counting.
- Saturating failure counter.
- Temporary lockout after three consecutive failures.
- Failure-counter reset after successful authentication or completed lockout.
- Shared authentication path for unlock and credential-register authorization.
- Explicit credential-register first-entry, confirmation, validation, persistence, and success-feedback states.
- Independent, saturating credential-confirmation mismatch counter.
- Confirmation retry after the first two mismatches and registration abortion on the third.
- Explicit pending-request and registration-mismatch cleanup on terminal paths.
- Direct first-boot registration activation when no stored credential exists.
- Safe rejection of events that are invalid in the current state.
- Boot gating through explicit initialization-result events.
- Synchronous and deterministic processing.
- Singleton runtime instance.
- Static storage only.
- No dynamic memory allocation.
- No direct hardware access.
- No direct calls to collaborating services.
- No HAL, LL, or RTOS dependency.
- No blocking operations.

---

## 3. Architecture

### 3.1 Layer Placement

The Lock Control Service belongs to the hardware-independent domain-services layer. The application layer translates external facts into LCS events and interprets the returned LCS action.

```mermaid
flowchart LR
    subgraph SOURCES["Event Sources"]
        INPUT["Debounced user input"]
        CES["Credential Entry Service"]
        AUTH["Authentication Service"]
        REGISTER["Credential Register Service<br/>staging + comparison"]
        STORAGE["Credential Storage Service"]
        TIME["Timeout owner"]
        BOOT["Startup validation"]
    end

    subgraph DOMAIN["Domain Services"]
        LCS["Lock Control Service<br/>state + policy + <br/>transition table"]
    end

    subgraph APP["Application Layer"]
        DISPATCH["Event mapping and<br/>semantic action dispatcher"]
    end

    subgraph EFFECTS["Coordinated Effects"]
        DISPLAY["Display / backlight"]
        STATUS["Status indication"]
        SOUND["Sound feedback"]
        LOCK["Lock actuator"]
        TIMERS["Timeout lifecycle"]
    end

    INPUT --> DISPATCH
    CES --> DISPATCH
    AUTH --> DISPATCH
    REGISTER --> DISPATCH
    STORAGE --> DISPATCH
    TIME --> DISPATCH
    BOOT --> DISPATCH
    DISPATCH <-->|"LCS_Event_t / LCS_Action_t"| LCS
    DISPATCH --> DISPLAY
    DISPATCH --> STATUS
    DISPATCH --> SOUND
    DISPATCH --> LOCK
    DISPATCH --> TIMERS
```

The arrows describe orchestration and responsibility boundaries. They do not imply that the Lock Control Service includes or calls any event source, presentation service, timer, or actuator adapter.

### 3.2 Decision and Execution Boundary

The central architectural boundary is:

```text
semantic fact -> Lock Control decision -> semantic action -> application execution
```

The Lock Control Service owns decisions such as:

- Whether credential entry is currently allowed.
- Whether active entry is authorizing unlock or credential registration.
- Whether an authentication result can grant or deny access.
- Whether successful authentication enters credential registration.
- Whether the first and confirmation entries match or another confirmation attempt is allowed.
- Whether storage success starts bounded feedback or storage failure enters the fault policy.
- Whether the failure limit requires lockout.
- Whether an elapsed timeout returns the product to locked mode.

The application owns execution such as:

- Beginning and ending a Credential Entry Service session.
- Copying and erasing a candidate credential.
- Invoking authentication.
- Staging and comparing the two new-credential entries.
- Invoking persistent credential storage and mapping its result back to LCS.
- Rendering an access-granted or access-denied message.
- Selecting LED and buzzer patterns.
- Starting or stopping timeout measurement.
- Energizing or de-energizing the lock actuator.
- Requesting a controlled platform reset.

Consequently, the service can be tested on a native host without providing mocks for LCD, GPIO, timers, the actuator, or any other hardware-facing function. Those dependencies do not cross its public boundary.

### 3.3 Singleton Ownership

The source file allocates one private `LCS_Handle_t` instance. It is the single authoritative runtime context for the product lock
FSM, including the operation currently awaiting authentication.

The singleton is appropriate while the firmware owns exactly one physical lock and invokes the service from one serialized application context. No public handle is exposed, so callers cannot mutate the current state, failure counter, or initialization flag directly.

---

## 4. Directory Structure

```text
Services/
|
└── Lock_Control/
    |
    ├── Inc/
    │   └── Lock_Control_Service.h
    |
    ├── Src/
    │   └── Lock_Control_Service.c
    |
    └── README.md
```

`Inc/Lock_Control_Service.h` contains only the semantic events, semantic actions, and `LCS_Process()` declaration.

`Src/Lock_Control_Service.c` owns all implementation details:

- Private states.
- Private guards.
- Private internal effects.
- Transition-record layout.
- Immutable transition table.
- Singleton runtime handle.
- Guard, effect, and transition-selection logic.

---

## 5. Service Responsibilities

### 5.1 Responsibilities

The Lock Control Service is responsible for:

- Owning the authoritative product-level lock state.
- Accepting semantic events from the application.
- Selecting at most one authorized transition per processed event.
- Evaluating private guards against private policy state.
- Distinguishing the unlock and credential-register destinations of successful authentication.
- Owning and clearing the pending authentication purpose.
- Owning the credential-register phase transitions and confirmation-mismatch counter.
- Selecting another confirmation attempt or session abortion after a mismatch.
- Selecting success feedback or the controlled fault policy after persistent-storage completion.
- Applying private internal effects.
- Committing the target state of an accepted transition.
- Returning the semantic action associated with an accepted transition.
- Ignoring events that are not valid for the current state.
- Gating normal processing until startup validation selects normal locked operation or first registration.
- Counting consecutive authentication failures with saturation.
- Selecting lockout after the configured failure limit.
- Resetting the failure count after authentication success or completed lockout.
- Modeling the complete product-level credential-register lifecycle without storing credential bytes.
- Preserving its domain and hardware independence.

### 5.2 Explicit Non-Responsibilities

The Lock Control Service is **not responsible** for:

- Reading keyboard GPIOs or matrix scan codes.
- Debouncing physical keys.
- Collecting, storing, copying, comparing, or erasing credential digits.
- Holding the first candidate or comparing it with the confirmation candidate.
- Persisting a newly registered credential.
- Calling the Credential Entry Service.
- Calling the Authentication Service.
- Reading a clock or measuring elapsed time.
- Starting a hardware or RTOS timer directly.
- Rendering text or symbols on a display.
- Controlling the display backlight.
- Driving LEDs or a buzzer.
- Driving the physical lock actuator.
- Calling HAL or LL functions.
- Creating tasks, queues, mutexes, or timer objects.
- Persisting the failed-attempt counter across resets.
- Verifying that a requested semantic action was successfully executed.
- Coordinating multi-service cleanup after an application action fails.

---

## 6. Dependencies

The public interface does not require any external or standard-library type.

The private implementation uses only:

```c
#include "Lock_Control_Service.h"
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
```

These headers provide the service contract, the fixed-width failure counter, the initialization flag, `size_t`, and `NULL`.

The service does not depend on:

- STM32 HAL or LL headers.
- CMSIS device headers.
- Platform interfaces or peripheral handles.
- Credential Entry Service headers.
- Authentication Service headers.
- Display, status-indication, or sound-service headers.
- Timeout Validation Service headers.
- Application-layer headers.
- FreeRTOS headers or primitives.
- A heap allocator.

---

## 7. Public Data Model

### 7.1 Semantic Events

`LCS_Event_t` describes product facts already interpreted by App Core. The named module is the fact's origin; App Core translates
its result and is the only integration boundary that calls `LCS_Process()`.

| Event | Origin mapped by App Core | Meaning |
|---|---|---|
| `LCS_EVENT_NONE` | App Core | No semantic follow-up event is available; never causes a transition. |
| `LCS_EVENT_INIT_OK` | App Core startup | Critical startup dependencies are valid and normal operation may begin. |
| `LCS_EVENT_INIT_FAIL` | App Core startup | Startup validation failed and the controlled fault path is required. |
| `LCS_EVENT_CREDENTIAL_NOT_REGISTERED` | Credential Storage Service | No stored credential exists and initial registration is required. |
| `LCS_EVENT_CREDENTIAL_REGISTER_REQUESTED` | User command mapping | Active entry shall restart as authorization for credential registration. |
| `LCS_EVENT_CREDENTIAL_REGISTER_DONE` | Registration-feedback timing | Successful registration feedback completed and locked idle may resume. |
| `LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED` | Matrix Keyboard Driver | A wake key requests entry mode; the wake key is not a credential digit. |
| `LCS_EVENT_CREDENTIAL_CANCELLED` | Credential Entry Service | The active credential-entry session was cancelled. |
| `LCS_EVENT_CANDIDATE_READY` | Credential Entry Service | A complete candidate credential is ready for the state-specific next operation. |
| `LCS_EVENT_AUTH_SUCCESS` | Authentication Service | Authentication accepted the current credential candidate. |
| `LCS_EVENT_AUTH_FAILURE` | Authentication Service or integration fallback | Authentication or its secure transfer/cleanup path failed. |
| `LCS_EVENT_STAGING_VALIDATION_SUCCESS` | Credential Register Service | The confirmation candidate matches the staged first entry. |
| `LCS_EVENT_STAGING_VALIDATION_FAILURE` | Credential Register Service | The confirmation candidate differs from the staged first entry. |
| `LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_SUCCESS` | Credential Storage Service | The confirmed credential was persisted and verified. |
| `LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_FAILURE` | Credential Storage Service | The confirmed credential could not be persisted or verified. |
| `LCS_EVENT_CANDIDATE_INCOMPLETE` | Credential Entry Service | Confirmation occurred before the active candidate reached full length. |
| `LCS_EVENT_ENTRY_TIMEOUT` | Timeout Validation Service | The bounded normal or registration entry interval elapsed. |
| `LCS_EVENT_UNLOCK_TIMEOUT` | Timeout Validation Service | The bounded authorized-unlock interval elapsed. |
| `LCS_EVENT_DENIED_ACCESS_TIMEOUT` | Timeout Validation Service | The bounded access-denied feedback interval elapsed. |
| `LCS_EVENT_LOCKOUT_TIMEOUT` | Timeout Validation Service | The temporary lockout interval elapsed. |
| `LCS_EVENT_COUNT` | No producer | Number of event identifiers; not a dispatchable event. |

Events contain no hardware codes, peripheral handles, pointers, timestamps, or credential payloads.

### 7.2 Semantic Actions

`LCS_Action_t` tells the application what product-level operation is pending.

| Action | Application intent |
|---|---|
| `LCS_ACTION_NONE` | No application coordination is requested. |
| `LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION` | Begin collection of the first new credential entry. |
| `LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION` | Erase and restart an incomplete first entry. |
| `LCS_ACTION_END_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION` | Abort first entry, erase transient registration data and restore locked idle. |
| `LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_TO_CONFIRM_ENTRY_SESSION` | Stage the first entry and begin confirmation entry. |
| `LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION` | Erase and restart only confirmation entry while retaining the staged first entry. |
| `LCS_ACTION_END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION` | Abort confirmation, erase both transient entries and restore locked idle. |
| `LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_SAVING_SESSION` | Reserved saving-session preparation action; currently not selected by the table. |
| `LCS_ACTION_END_CREDENTIAL_REGISTER_SAVING_SESSION` | Finish saving cleanup and begin bounded registration-success feedback. |
| `LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_TO_REGISTER_SESSION` | Erase/restart active entry so the next complete candidate authorizes credential registration. |
| `LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION` | Wake the UI, begin credential entry, and establish its inactivity timing. |
| `LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_SESSION` | Erase the incomplete candidate, preserve the session, and restart entry timing. |
| `LCS_ACTION_END_CREDENTIAL_ENTRY_SESSION` | End and erase the entry session, then restore the locked-idle presentation. |
| `LCS_ACTION_REQUEST_AUTHENTICATION` | Obtain, erase, and authenticate the completed candidate, then report the result. |
| `LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STAGES_VALIDATION` | Compare confirmation entry with the staged first entry. |
| `LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STORAGE` | Persist the confirmed credential through Credential Storage. |
| `LCS_ACTION_GRANT_ACCESS_UNLOCK` | Start bounded unlock operation and access-granted feedback. |
| `LCS_ACTION_DENY_ACCESS` | Preserve the safe lock state and start bounded access-denied feedback. |
| `LCS_ACTION_ENTER_LOCKOUT` | Preserve the safe lock state, reject entry, and start the lockout interval. |
| `LCS_ACTION_RETURN_TO_LOCKED` | Restore the safe locked-idle mode and its user-interface policy. |
| `LCS_ACTION_RETURN_TO_LOCKED_FROM_ENTRY_TIMEOUT` | End entry, restore locked idle, and provide entry-timeout feedback. |
| `LCS_ACTION_RETURN_TO_LOCKED_FROM_CREDENTIAL_REGISTER_SESSION` | Restore locked idle after successful registration feedback completes. |
| `LCS_ACTION_REQUEST_CONTROLLED_RESET` | Preserve safe outputs and request the application-controlled reset path. |

Actions are intentionally semantic. `LCS_ACTION_GRANT_ACCESS_UNLOCK`, for example, is not a GPIO command. Its application handler may coordinate an actuator interface, a display view, LEDs, a sound pattern, and a timeout as one product operation.

### 7.3 Meaning of LCS_ACTION_NONE

`LCS_ACTION_NONE` means that no external application work was requested by the selected transition. It does **not** always mean that the FSM rejected the event.

For example:

```text
BOOT + LCS_EVENT_INIT_OK -> LOCKED + service activation + LCS_ACTION_NONE
```

The transition occurs and private runtime state changes even though the application receives no semantic action. Therefore, application code shall interpret `LCS_ACTION_NONE` as "nothing to execute," not as a universal success/failure status.

---

## 8. Private Runtime Model

### 8.1 States

`LCS_State_t` remains private so the application cannot couple itself to the FSM representation.

| Private state | Meaning |
|---|---|
| `LCS_STATE_BOOT` | Awaiting the application startup-validation result. |
| `LCS_STATE_LOCKED` | Secure idle state awaiting a credential-entry request. |
| `LCS_STATE_CREDENTIAL_REGISTER_FIRST_ENTRY` | The first new credential is being collected. |
| `LCS_STATE_CREDENTIAL_REGISTER_CONFIRM_ENTRY` | Confirmation of the staged credential is being collected. |
| `LCS_STATE_CREDENTIAL_REGISTER_VALIDATING` | The staged and confirmation credentials are being compared. |
| `LCS_STATE_CREDENTIAL_REGISTER_PERSISTING` | The confirmed credential is awaiting its storage result. |
| `LCS_STATE_CREDENTIAL_REGISTER_SUCCESS_FEEDBACK` | Bounded successful-registration feedback is active. |
| `LCS_STATE_CREDENTIAL_SESSION_ACTIVE` | Credential entry is active and may produce session events. |
| `LCS_STATE_AUTHENTICATING` | A complete candidate is awaiting its authentication result. |
| `LCS_STATE_ACCESS_GRANTED_UNLOCKED` | Access was granted and the bounded unlock interval is active. |
| `LCS_STATE_ACCESS_DENIED_LOCKED` | Access was denied and bounded denial feedback is active. |
| `LCS_STATE_LOCKOUT` | Entry requests are rejected until the lockout interval expires. |
| `LCS_STATE_FAULT` | A critical startup or credential-storage failure prevents normal operation. |
| `LCS_STATE_COUNT` | Number of private states. It is not a runtime state. |

The topology is split into two compact views so the registration detail does not obscure the normal access path.

```mermaid
flowchart TB
    START((Start)) --> BOOT["BOOT"]

    BOOT -->|T01| LOCKED["LOCKED<br/>idle"]
    BOOT -->|T02| FAULT["FAULT"]
    BOOT -->|T03| FIRST["REGISTER<br/>FIRST ENTRY"]

    LOCKED -->|T04| ENTRY["CREDENTIAL SESSION<br/>ACTIVE<br/>T06 / T08 ↺"]
    ENTRY -->|T09| AUTH["AUTHENTICATING"]

    AUTH -->|T10| FIRST
    AUTH -->|T11| GRANTED["ACCESS GRANTED<br/>UNLOCKED"]
    AUTH -->|T13| DENIED["ACCESS DENIED<br/>LOCKED"]

    ENTRY -->|"T05 / T07"| RETURN["LOCKED<br/>return rail"]
    GRANTED -->|T12| RETURN
    DENIED -->|T14| RETURN
    DENIED -->|T15| LOCKOUT["LOCKOUT"]
    LOCKOUT -->|T16| RETURN

    classDef initial fill:#f4f6f7,stroke:#667681,color:#263642
    classDef secure fill:#eaf5ed,stroke:#3d7e57,color:#17324d
    classDef active fill:#eaf4fb,stroke:#3478a8,color:#17324d
    classDef denied fill:#fff2e5,stroke:#d77a27,color:#5e3514
    classDef connector fill:#f4f6f7,stroke:#667681,color:#263642,stroke-dasharray: 5 3
    classDef fault fill:#fbecec,stroke:#b54040,color:#7a2525

    class START initial
    class BOOT,LOCKED secure
    class ENTRY,AUTH,GRANTED,FIRST active
    class DENIED,LOCKOUT denied
    class RETURN connector
    class FAULT fault
```

```mermaid
flowchart LR
    FIRST["FIRST ENTRY<br/>T19 ↺"] -->|T17| CONFIRM["CONFIRM ENTRY<br/>T23 ↺"]
    CONFIRM -->|T21| VALIDATE["VALIDATING"]
    VALIDATE -->|T25| PERSIST["PERSISTING"]
    PERSIST -->|T28| FEEDBACK["SUCCESS<br/>FEEDBACK"]
    FEEDBACK -->|T30| LOCKED["LOCKED"]

    VALIDATE -->|T26| CONFIRM
    FIRST -->|"T18 / T20"| LOCKED
    CONFIRM -->|"T22 / T24"| LOCKED
    VALIDATE -->|T27| LOCKED
    PERSIST -->|T29| FAULT["FAULT"]

    classDef secure fill:#eaf5ed,stroke:#3d7e57,color:#17324d
    classDef active fill:#eaf4fb,stroke:#3478a8,color:#17324d
    classDef fault fill:#fbecec,stroke:#b54040,color:#7a2525

    class FIRST,CONFIRM,VALIDATE,PERSIST,FEEDBACK active
    class LOCKED secure
    class FAULT fault
```

Every edge corresponds to the transition identifier in [Section 9.4](#94-transition-table). `START` is only the static-initialization
marker. `RETURN` is a layout-only duplicate of `LOCKED`, not another `LCS_State_t`; no long edge reconnects the two boxes because
that visual cycle would obscure the operational paths. `T06`, `T08`, `T19` and `T23` are self-transitions displayed inside their
state nodes to keep the graphs free of looping edges.

Event names in the diagram omit the common `LCS_EVENT_` prefix, and action labels omit `LCS_ACTION_`, to keep the graph readable. The transition table in [Section 9.4](#94-transition-table) remains the normative record-by-record representation of the implementation.

### 8.2 Singleton Handle

The private runtime handle contains:

```c
typedef struct
{
    LCS_State_t   current_state;
    uint8_t       failed_attempt_count;
    uint8_t       credential_register_mismatch_count;
    LCS_Pending_t pending_request;
    bool          initialized;

}LCS_Handle_t;
```

| Field | Ownership and purpose |
|---|---|
| `current_state` | Authoritative source state used during transition lookup. |
| `failed_attempt_count` | Saturating number of consecutive rejected authentications. |
| `credential_register_mismatch_count` | Saturating number of confirmation mismatches in the active registration session. |
| `pending_request` | Operation selected after successful authentication: none, unlock, or credential registration. |
| `initialized` | Enables normal event processing only after successful startup. |

The transition table is not part of the mutable handle because it is immutable module policy stored in read-only static data.

### 8.3 Pending Authentication Purpose

`LCS_Pending_t` is private routing context, not a public action and not a second FSM state. It has three values:

| Pending value | Meaning |
|---|---|
| `LCS_PENDING_NONE` | No operation awaits authentication. |
| `LCS_PENDING_UNLOCK` | Authentication success shall enter bounded unlock. |
| `LCS_PENDING_CREDENTIAL_REGISTER` | Authentication success shall enter credential-register first entry. |

Normal entry from `LOCKED` sets `LCS_PENDING_UNLOCK`. While entry remains active,
`LCS_EVENT_CREDENTIAL_REGISTER_REQUESTED` replaces that purpose with `LCS_PENDING_CREDENTIAL_REGISTER` and asks the application
to erase/restart the candidate. Entry refresh and the move to `AUTHENTICATING` preserve the pending value. Success, failure,
cancellation and timeout resolve the request and clear it.

This context makes the two `AUTH_SUCCESS` records deterministic without duplicating the credential-entry and authentication
states. `AUTH_SUCCESS` with `LCS_PENDING_NONE` is rejected without mutation.

### 8.4 Runtime Invariants

The implementation preserves these invariants:

- The initial state is `LCS_STATE_BOOT`.
- The initial failure count is zero.
- The initial registration-mismatch count is zero.
- The initial pending request is `LCS_PENDING_NONE`.
- Normal processing is initially inactive.
- `LCS_EVENT_INIT_OK` and `LCS_EVENT_CREDENTIAL_NOT_REGISTERED` can set `initialized` to `true` from boot.
- Before activation, events other than `INIT_OK`, `INIT_FAIL`, and `CREDENTIAL_NOT_REGISTERED` cannot mutate runtime state.
- The failure counter never exceeds `LCS_FAILURE_ATTEMPTS_LIMIT`.
- The registration-mismatch counter never exceeds `LCS_CREDENTIAL_REGISTER_MISMATCH_LIMIT`.
- Authentication failures and registration mismatches use independent counters.
- Authentication success resets the consecutive-failure counter.
- `LOCKED` has no pending request after every completed, failed, cancelled, or timed-out operation.
- Active entry and authentication preserve exactly one pending purpose.
- Authentication success is accepted only when its record's pending-operation discriminator equals `pending_request`.
- Authentication failure clears the pending request before denial feedback begins.
- Lockout completion resets the consecutive-failure counter.
- Every terminal registration path resets the registration-mismatch counter.
- Invalid current-state/event combinations do not mutate runtime state.
- At most one transition is committed by one `LCS_Process()` call.

---

## 9. Transition Model

### 9.1 Transition Record

Each `LCS_Transition_t` record completely describes one transition:

```c
typedef struct
{
    LCS_State_t          source_state;
    LCS_Event_t          event;
    LCS_Guard_t          guard;
    LCS_Pending_t        pending_op;
    LCS_State_t          target_state;
    LCS_InternalEffect_t internal_effect;
    LCS_Action_t         action;

}LCS_Transition_t;
```

The fields have distinct responsibilities:

| Field | Purpose |
|---|---|
| `source_state` | State in which the record may be selected. |
| `event` | Semantic fact required by the record. |
| `guard` | Runtime condition that authorizes the record. |
| `pending_op` | Required pending purpose for an authentication-success route; `NONE` on records that do not use this discriminator. |
| `target_state` | State committed after the transition is accepted. |
| `internal_effect` | Private mutation performed inside LCS. |
| `action` | Semantic work returned to the application. |

The distinction between `internal_effect` and `action` is essential:

- An **internal effect** changes data owned exclusively by the service, such as its failure counter.
- An **action** requests work outside the service boundary, such as starting a session or actuating the lock through the application.

### 9.2 Guards

The current private guards are:

| Guard | Condition |
|---|---|
| `LCS_GUARD_ALWAYS` | Always authorizes the matching source/event record. |
| `LCS_GUARD_UNDER_ATTEMPT_LIMIT` | Authorizes while the failure count is below three. |
| `LCS_GUARD_ATTEMPT_COUNT_LIMIT` | Authorizes once the failure count reaches three. |
| `LCS_GUARD_REGISTER_RETRY_AVAILABLE` | Authorizes when the current mismatch still leaves another confirmation attempt. |
| `LCS_GUARD_REGISTER_ATTEMPT_LIMIT` | Authorizes when the current mismatch consumes the third and final confirmation attempt. |

The authentication-attempt pair is complementary and selects return-to-locked or lockout after denial feedback. The
registration-attempt pair is also complementary and selects another confirmation attempt or registration abortion on the third
mismatch.

An unknown guard evaluates to `false`. This fail-closed default prevents malformed transition data from authorizing a state change.

### 9.3 Internal Effects

The current private internal effects are:

| Internal effect | Mutation |
|---|---|
| `LCS_INTERNAL_EFFECT_NONE` | Preserves all runtime fields except the mandatory target-state commit. |
| `LCS_INTERNAL_EFFECT_SET_SERVICE_ACTIVE` | Marks successful startup activation. |
| `LCS_INTERNAL_EFFECT_SET_PENDING_REGISTER_SESSION` | Routes the next successful authentication to credential registration. |
| `LCS_INTERNAL_EFFECT_SET_PENDING_UNLOCK` | Routes the next successful authentication to bounded unlock. |
| `LCS_INTERNAL_EFFECT_CLEAR_PENDING` | Restores the no-pending-request invariant. |
| `LCS_INTERNAL_EFFECT_INCREMENT_ATTEMPT_COUNT` | Saturating increment of consecutive authentication failures. |
| `LCS_INTERNAL_EFFECT_RESET_ATTEMPT_COUNT` | Clears the failure counter. |
| `LCS_INTERNAL_EFFECT_CLEAR_PENDING_AND_INCREMENT_ATTEMPT_COUNT` | Clears the pending purpose and records one rejected authentication. |
| `LCS_INTERNAL_EFFECT_CLEAR_PENDING_AND_RESET_ATTEMPT_COUNT` | Clears the pending purpose and all consecutive failures. |
| `LCS_INTERNAL_EFFECT_INCREMENT_REGISTER_MISMATCH_COUNT` | Saturating increment of confirmation mismatches in the active registration session. |
| `LCS_INTERNAL_EFFECT_RESET_REGISTER_MISMATCH_COUNT` | Clears registration-confirmation mismatch history. |

An internal effect is not a user-visible product action and must not call external modules. It exists so transition-local changes to LCS-owned data remain declarative in the table instead of being hidden in event-specific branches.

Target-state assignment is mandatory for every accepted transition and is therefore performed separately rather than represented as another internal effect.

### 9.4 Transition Table

The current table contains the following declared transition records:

| ID | Source state | Event | Guard | Pending discriminator | Target state | Internal effect | Returned action |
|---:|---|---|---|---|---|---|---|
| `T01` | `BOOT` | `INIT_OK` | Always | None | `LOCKED` | Set service active | `NONE` |
| `T02` | `BOOT` | `INIT_FAIL` | Always | None | `FAULT` | None | `REQUEST_CONTROLLED_RESET` |
| `T03` | `BOOT` | `CREDENTIAL_NOT_REGISTERED` | Always | None | `CREDENTIAL_REGISTER_FIRST_ENTRY` | Set service active | `BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION` |
| `T04` | `LOCKED` | `CREDENTIAL_ENTRY_REQUESTED` | Always | None | `CREDENTIAL_SESSION_ACTIVE` | Set pending unlock | `BEGIN_CREDENTIAL_ENTRY_SESSION` |
| `T05` | `CREDENTIAL_SESSION_ACTIVE` | `CREDENTIAL_CANCELLED` | Always | None | `LOCKED` | Clear pending | `END_CREDENTIAL_ENTRY_SESSION` |
| `T06` | `CREDENTIAL_SESSION_ACTIVE` | `CREDENTIAL_REGISTER_REQUESTED` | Always | None | `CREDENTIAL_SESSION_ACTIVE` | Set pending register session | `REFRESH_CREDENTIAL_ENTRY_TO_REGISTER_SESSION` |
| `T07` | `CREDENTIAL_SESSION_ACTIVE` | `ENTRY_TIMEOUT` | Always | None | `LOCKED` | Clear pending | `RETURN_TO_LOCKED_FROM_ENTRY_TIMEOUT` |
| `T08` | `CREDENTIAL_SESSION_ACTIVE` | `CANDIDATE_INCOMPLETE` | Always | None | `CREDENTIAL_SESSION_ACTIVE` | None | `REFRESH_CREDENTIAL_ENTRY_SESSION` |
| `T09` | `CREDENTIAL_SESSION_ACTIVE` | `CANDIDATE_READY` | Always | None | `AUTHENTICATING` | None | `REQUEST_AUTHENTICATION` |
| `T10` | `AUTHENTICATING` | `AUTH_SUCCESS` | Always | Credential register | `CREDENTIAL_REGISTER_FIRST_ENTRY` | Clear pending and reset failures | `BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION` |
| `T11` | `AUTHENTICATING` | `AUTH_SUCCESS` | Always | Unlock | `ACCESS_GRANTED_UNLOCKED` | Clear pending and reset failures | `GRANT_ACCESS_UNLOCK` |
| `T12` | `ACCESS_GRANTED_UNLOCKED` | `UNLOCK_TIMEOUT` | Always | None | `LOCKED` | Clear pending | `RETURN_TO_LOCKED` |
| `T13` | `AUTHENTICATING` | `AUTH_FAILURE` | Always | None | `ACCESS_DENIED_LOCKED` | Clear pending and increment failures | `DENY_ACCESS` |
| `T14` | `ACCESS_DENIED_LOCKED` | `DENIED_ACCESS_TIMEOUT` | Under attempt limit | None | `LOCKED` | Clear pending | `RETURN_TO_LOCKED` |
| `T15` | `ACCESS_DENIED_LOCKED` | `DENIED_ACCESS_TIMEOUT` | Attempt count at limit | None | `LOCKOUT` | Clear pending | `ENTER_LOCKOUT` |
| `T16` | `LOCKOUT` | `LOCKOUT_TIMEOUT` | Always | None | `LOCKED` | Reset failure count | `RETURN_TO_LOCKED` |
| `T17` | `CREDENTIAL_REGISTER_FIRST_ENTRY` | `CANDIDATE_READY` | Always | None | `CREDENTIAL_REGISTER_CONFIRM_ENTRY` | Reset register mismatch count | `REFRESH_CREDENTIAL_REGISTER_FIRST_TO_CONFIRM_ENTRY_SESSION` |
| `T18` | `CREDENTIAL_REGISTER_FIRST_ENTRY` | `CREDENTIAL_CANCELLED` | Always | None | `LOCKED` | Reset register mismatch count | `END_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION` |
| `T19` | `CREDENTIAL_REGISTER_FIRST_ENTRY` | `CANDIDATE_INCOMPLETE` | Always | None | `CREDENTIAL_REGISTER_FIRST_ENTRY` | None | `REFRESH_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION` |
| `T20` | `CREDENTIAL_REGISTER_FIRST_ENTRY` | `ENTRY_TIMEOUT` | Always | None | `LOCKED` | Reset register mismatch count | `END_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION` |
| `T21` | `CREDENTIAL_REGISTER_CONFIRM_ENTRY` | `CANDIDATE_READY` | Always | None | `CREDENTIAL_REGISTER_VALIDATING` | None | `REQUEST_CREDENTIAL_REGISTER_STAGES_VALIDATION` |
| `T22` | `CREDENTIAL_REGISTER_CONFIRM_ENTRY` | `CREDENTIAL_CANCELLED` | Always | None | `LOCKED` | Reset register mismatch count | `END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION` |
| `T23` | `CREDENTIAL_REGISTER_CONFIRM_ENTRY` | `CANDIDATE_INCOMPLETE` | Always | None | `CREDENTIAL_REGISTER_CONFIRM_ENTRY` | None | `REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION` |
| `T24` | `CREDENTIAL_REGISTER_CONFIRM_ENTRY` | `ENTRY_TIMEOUT` | Always | None | `LOCKED` | Reset register mismatch count | `END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION` |
| `T25` | `CREDENTIAL_REGISTER_VALIDATING` | `STAGING_VALIDATION_SUCCESS` | Always | None | `CREDENTIAL_REGISTER_PERSISTING` | Reset register mismatch count | `REQUEST_CREDENTIAL_REGISTER_STORAGE` |
| `T26` | `CREDENTIAL_REGISTER_VALIDATING` | `STAGING_VALIDATION_FAILURE` | Register retry available | None | `CREDENTIAL_REGISTER_CONFIRM_ENTRY` | Increment register mismatch count | `REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION` |
| `T27` | `CREDENTIAL_REGISTER_VALIDATING` | `STAGING_VALIDATION_FAILURE` | Register attempt limit | None | `LOCKED` | Reset register mismatch count | `END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION` |
| `T28` | `CREDENTIAL_REGISTER_PERSISTING` | `CREDENTIAL_REGISTER_STORAGE_SUCCESS` | Always | None | `CREDENTIAL_REGISTER_SUCCESS_FEEDBACK` | Reset register mismatch count | `END_CREDENTIAL_REGISTER_SAVING_SESSION` |
| `T29` | `CREDENTIAL_REGISTER_PERSISTING` | `CREDENTIAL_REGISTER_STORAGE_FAILURE` | Always | None | `FAULT` | Reset register mismatch count | `REQUEST_CONTROLLED_RESET` |
| `T30` | `CREDENTIAL_REGISTER_SUCCESS_FEEDBACK` | `CREDENTIAL_REGISTER_DONE` | Always | None | `LOCKED` | Reset register mismatch count | `RETURN_TO_LOCKED_FROM_CREDENTIAL_REGISTER_SESSION` |

### 9.5 Selection Rules

`LCS_FindTransition()` scans the table from the first record to the last. A record is selected only when the common conditions are
true:

1. `source_state` equals the current runtime state.
2. `event` equals the event supplied to `LCS_Process()`.
3. The private guard evaluates to `true`.

For `AUTHENTICATING + AUTH_SUCCESS`, one additional condition applies:

4. `pending_op` equals the singleton `pending_request` and that request is not `LCS_PENDING_NONE`.

The first authorized record wins. If none is found, the event is ignored, no runtime field changes, and `LCS_ACTION_NONE` is returned.

The pending discriminator makes the two authentication-success routes mutually exclusive. The authentication-attempt guards make
the two denied-timeout routes mutually exclusive, while the registration-attempt guards do the same for validation failure.
Therefore, every complete runtime context selects at most one record.

The linear representation stores only declared transition records. It avoids a dense state-by-event matrix containing mostly
empty cells and naturally supports multiple policy-selected outcomes for one source/event pair.

---

## 10. Processing Algorithm

`LCS_Process()` performs a short, synchronous pipeline:

```mermaid
flowchart TD
    EVENT["Receive one LCS_Event_t"] --> ACTIVE{"Service active?"}
    ACTIVE -->|"No"| BOOT_EVENT{"Boot-admitted event?"}
    BOOT_EVENT -->|"No"| NONE["Return LCS_ACTION_NONE<br/>without mutation"]
    BOOT_EVENT -->|"INIT_OK, INIT_FAIL or<br/>CREDENTIAL_NOT_REGISTERED"| FIND["Scan ordered<br/>transition table"]
    ACTIVE -->|"Yes"| FIND
    FIND --> MATCH{"Source + event + guard<br/>and required pending match?"}
    MATCH -->|"No record"| NONE
    MATCH -->|"First authorized record"| EFFECT["Apply private internal effect"]
    EFFECT --> COMMIT["Commit target state"]
    COMMIT --> ACTION["Return semantic action"]
```

The internal effect and target-state commit occur before the action is returned. Therefore, if the application executes an action synchronously and then dispatches a result event, that new event observes the already-committed authoritative state.

The service never recursively dispatches an event and never executes the returned action itself.

---

## 11. Scalability and Extension

### 11.1 Why the Table Is Scalable

Transition behavior is data-driven rather than distributed across nested conditionals. Each rule can be read as one row:

```text
source + event + guard + pending discriminator -> target + internal effect + semantic action
```

This provides several extension advantages:

- Valid behavior is visible in one ordered list.
- A new route between existing states generally requires one new record.
- Multiple outcomes for one event can be expressed with guards.
- Successful authentication can select its authorized destination from private pending-request context.
- Internal policy mutations remain explicit beside the transition that triggers them.
- Application effects remain semantic and do not introduce hardware dependencies.
- State/event combinations absent from the table are rejected by default.
- Tests can be derived directly from transition records and product flows.

The design scales in behavior without turning `LCS_Process()` into a growing state-specific dispatcher.

### 11.2 Adding Behavior

The exact work depends on what is new.

#### Add a route using existing concepts

When the source state, event, target state, internal effect, and action already exist, add one `LCS_Transition_t` record and add a corresponding test.

No processing algorithm or application dependency changes are required.

#### Add a new state

1. Add a private `LCS_State_t` identifier before `LCS_STATE_COUNT`.
2. Add incoming and outgoing transition records.
3. Reuse existing guards, effects, and actions when their semantics fit.
4. Add tests for entry, exit, and invalid-event behavior.

The state remains private. The application does not need to know its numeric value.

#### Add a new event

1. Add the semantic value to public `LCS_Event_t`.
2. Add the required transition records.
3. Map the external application fact to the event.
4. Add acceptance and rejection tests for relevant states.

#### Add a new private guard

1. Add a private `LCS_Guard_t` value.
2. Implement its predicate in `LCS_EvaluateGuard()`.
3. Reference it from transition records.
4. Verify overlap and boundary conditions.

Unknown guards remain fail-closed.

#### Add a new internal effect

1. Confirm the mutated data is owned exclusively by LCS.
2. Add a private `LCS_InternalEffect_t` value.
3. Implement its mutation in `LCS_ApplyInternalEffect()`.
4. Select it from the relevant transition records.
5. Test the mutation through observable subsequent behavior.

If the operation touches another service or hardware, it is not an internal effect; it belongs in a semantic application action.

#### Add a new semantic action

1. Add the value to public `LCS_Action_t`.
2. Return it from the appropriate transition records.
3. Add its application-layer executor case.
4. Coordinate all required services and platform interfaces there.
5. Define how execution failure is reported back to the FSM, if required.

Only a genuinely new application intent requires changing the action dispatcher. Reusing an existing action keeps the application integration unchanged.

### 11.3 Ordering and Ambiguity

Because selection uses first-match semantics, table order is behavior when two records can both match the same complete runtime
context.

Records sharing a source/event pair should normally use guards that are demonstrably mutually exclusive. The current denied-timeout pair follows this rule:

```text
failed_attempt_count < 3  -> return to LOCKED
failed_attempt_count >= 3 -> enter LOCKOUT
```

The authentication-success pair uses a different mutually exclusive discriminator:

```text
pending_request == UNLOCK              -> enter ACCESS_GRANTED_UNLOCKED
pending_request == CREDENTIAL_REGISTER -> enter CREDENTIAL_REGISTER_FIRST_ENTRY
pending_request == NONE                -> reject AUTH_SUCCESS
```

The validation-failure pair uses complementary guards evaluated against the current mismatch as the next attempt:

```text
current failure leaves count below 3 -> retry CONFIRM_ENTRY
current failure reaches count 3      -> abort registration to LOCKED
```

If deliberate priority is introduced later, it shall be documented beside those records and validated by tests. Accidental overlapping guards should be treated as a transition-table defect.

---

## 12. Application Integration

### 12.1 Action Dispatcher

The application calls `LCS_Process()` and dispatches the returned semantic action. A simplified composition pattern is:

```c
static void APP_DispatchLockEvent(LCS_Event_t Event)
{
    const LCS_Action_t action = LCS_Process(Event);

    switch(action)
    {
        case LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION:
            /* Begin CES for the first new credential and start entry timing. */
            break;

        case LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION:
            /* Erase/restart an incomplete first entry. */
            break;

        case LCS_ACTION_END_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION:
            /* Erase first-entry staging, end CES, cancel timing, and restore locked idle. */
            break;

        case LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_TO_CONFIRM_ENTRY_SESSION:
            /* Stage the first candidate, erase CES, and begin confirmation entry. */
            break;

        case LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION:
            /* Erase/restart confirmation while preserving the staged first candidate. */
            break;

        case LCS_ACTION_END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION:
            /* Erase both registration candidates, end CES, cancel timing, and restore locked idle. */
            break;

        case LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_SAVING_SESSION:
            /* Reserved: no current transition returns this action. */
            break;

        case LCS_ACTION_END_CREDENTIAL_REGISTER_SAVING_SESSION:
            /* Erase transient registration data and start bounded success feedback. */
            break;

        case LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_TO_REGISTER_SESSION:
            /* Erase/restart CES and presentation so the next candidate authorizes registration. */
            break;

        case LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION:
            /* Begin CES, wake/render the display, set indication, and start entry timing. */
            break;

        case LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_SESSION:
            /* Erase the incomplete candidate, preserve CES, and restart entry timing. */
            break;

        case LCS_ACTION_REQUEST_AUTHENTICATION:
            /* Copy and erase the candidate, authenticate it, then dispatch AUTH_SUCCESS or AUTH_FAILURE. */
            break;

        case LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STAGES_VALIDATION:
            /* Compare confirmation with staged first entry, erase confirmation, and dispatch the result. */
            break;

        case LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STORAGE:
            /* Persist the confirmed credential and dispatch STORAGE_SUCCESS or STORAGE_FAILURE. */
            break;

        case LCS_ACTION_GRANT_ACCESS_UNLOCK:
            /* Command the actuator, render feedback, signal success, and start unlock timing. */
            break;

        case LCS_ACTION_DENY_ACCESS:
            /* Keep the actuator safely locked, render denial, signal failure, and start feedback timing. */
            break;

        case LCS_ACTION_ENTER_LOCKOUT:
            /* Keep the actuator locked, reject entry, render lockout, and start lockout timing. */
            break;

        case LCS_ACTION_END_CREDENTIAL_ENTRY_SESSION:
        case LCS_ACTION_RETURN_TO_LOCKED:
            /* End sensitive sessions, enforce locked output, stop obsolete timing, and restore idle UI. */
            break;

        case LCS_ACTION_RETURN_TO_LOCKED_FROM_ENTRY_TIMEOUT:
            /* End entry, enforce lock, restore idle UI, and provide timeout feedback. */
            break;

        case LCS_ACTION_RETURN_TO_LOCKED_FROM_CREDENTIAL_REGISTER_SESSION:
            /* End registration-success feedback and restore locked idle. */
            break;

        case LCS_ACTION_REQUEST_CONTROLLED_RESET:
            /* Preserve safe outputs and execute the platform's controlled reset policy. */
            break;

        case LCS_ACTION_NONE:
        default:
            break;
    }
}
```

This is an integration example, not an implementation inside LCS. The final application may split handlers or use an action table while preserving the same boundary.

The application should complete one returned action coherently. If an action involves several modules, it should define safe ordering and rollback or fallback policy. For example, the actuator should be placed in its safe state even if display rendering fails.

### 12.2 Wake-Key Semantics

While the product is locked, the display may remain off until user activity occurs. Any accepted debounced key may be mapped to:

```c
LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED
```

That event requests entry mode and returns `LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION`.

The initiating key is a **wake/session-request key only**. It shall not be forwarded as the first credential digit. Credential commands are processed only after the application has executed the begin-session action. This prevents the same physical press from both waking the interface and silently entering data.

While that session is active, a product-specific command may dispatch `LCS_EVENT_CREDENTIAL_REGISTER_REQUESTED`. LCS preserves the
entry state, replaces the pending purpose with credential registration, and returns
`LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_TO_REGISTER_SESSION`. The application shall erase the existing candidate and restart entry
presentation/timing so digits originally entered for unlock cannot be reused silently as registration authorization.

### 12.3 Authentication Feedback

When LCS returns `LCS_ACTION_REQUEST_AUTHENTICATION`, the authoritative state is already `LCS_STATE_AUTHENTICATING` and exactly
one pending purpose identifies the operation being authorized.

The application may then:

1. Obtain a complete candidate copy from the Credential Entry Service.
2. End the credential-entry session so its internal candidate is erased.
3. Call the Authentication Service.
4. Erase the application-owned candidate copy.
5. Dispatch `LCS_EVENT_AUTH_SUCCESS` or `LCS_EVENT_AUTH_FAILURE` only after the original `LCS_Process()` call has returned.

On failure, LCS clears the pending purpose and applies the same denial/lockout policy to both requests. On success, it clears the
purpose, resets consecutive failures, and returns either `LCS_ACTION_GRANT_ACCESS_UNLOCK` or
`LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION`. The FSM does not receive credential bytes and does not know how authentication is
implemented.

### 12.4 Timeout Ownership

The service does not read timestamps. The application or a timeout service owns interval measurement and converts elapsed intervals into semantic events:

| Application interval | Event dispatched to LCS |
|---|---|
| Credential-entry inactivity | `LCS_EVENT_ENTRY_TIMEOUT` |
| Credential-register first/confirmation entry inactivity | `LCS_EVENT_ENTRY_TIMEOUT` |
| Authorized unlock duration | `LCS_EVENT_UNLOCK_TIMEOUT` |
| Access-denied feedback duration | `LCS_EVENT_DENIED_ACCESS_TIMEOUT` |
| Temporary lockout duration | `LCS_EVENT_LOCKOUT_TIMEOUT` |
| Successful-registration feedback duration | `LCS_EVENT_CREDENTIAL_REGISTER_DONE` |

A stale timeout event is harmless when no transition accepts it in the current state; it is ignored and returns `LCS_ACTION_NONE`. The application should still cancel obsolete timing sources to keep its own lifecycle clear.

---

## 13. Operation Flows

### 13.1 Successful Access

```mermaid
sequenceDiagram
    participant U as User Input
    participant APP as Application
    participant LCS as Lock Control Service
    participant AUTH as Authentication Service
    participant FX as UI / Sound / Actuator / Timing

    U->>APP: Debounced wake key
    APP->>LCS: CREDENTIAL_ENTRY_REQUESTED
    LCS-->>APP: BEGIN_CREDENTIAL_ENTRY_SESSION
    Note over LCS: pending_request = UNLOCK
    APP->>FX: Wake UI and start entry session/timing
    APP->>LCS: CANDIDATE_READY
    LCS-->>APP: REQUEST_AUTHENTICATION
    APP->>AUTH: Complete candidate
    AUTH-->>APP: Authenticated
    APP->>LCS: AUTH_SUCCESS
    LCS-->>APP: GRANT_ACCESS_UNLOCK
    Note over LCS: pending_request = NONE<br/>failure count = 0
    APP->>FX: Unlock + granted feedback + unlock timing
    APP->>LCS: UNLOCK_TIMEOUT
    LCS-->>APP: RETURN_TO_LOCKED
    APP->>FX: Enforce lock and restore idle presentation
```

Normal entry establishes `LCS_PENDING_UNLOCK`. Authentication success selects only the unlock transition, clears that pending
purpose, and resets the consecutive-failure counter before the unlock action is returned.

### 13.2 Authorized Credential Registration

The runtime registration route reuses normal credential entry and authentication to authorize the change, then follows the
registration states owned by LCS:

```mermaid
sequenceDiagram
    participant U as User Input
    participant APP as Application
    participant LCS as Lock Control Service
    participant AUTH as Authentication Service
    participant CRS as Credential Register Staging
    participant CSS as Credential Storage

    U->>APP: Wake / begin entry
    APP->>LCS: CREDENTIAL_ENTRY_REQUESTED
    LCS-->>APP: BEGIN_CREDENTIAL_ENTRY_SESSION
    Note over LCS: pending_request = UNLOCK
    U->>APP: Request credential registration
    APP->>LCS: CREDENTIAL_REGISTER_REQUESTED
    LCS-->>APP: REFRESH_CREDENTIAL_ENTRY_TO_REGISTER_SESSION
    Note over LCS: pending_request = CREDENTIAL_REGISTER
    APP->>APP: Erase and restart credential entry
    APP->>LCS: CANDIDATE_READY
    LCS-->>APP: REQUEST_AUTHENTICATION
    APP->>AUTH: Current credential candidate
    AUTH-->>APP: Authenticated
    APP->>LCS: AUTH_SUCCESS
    LCS-->>APP: BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION
    Note over LCS: pending_request = NONE
    U->>APP: Enter first new credential
    APP->>LCS: CANDIDATE_READY
    LCS-->>APP: REFRESH_CREDENTIAL_REGISTER_FIRST_TO_CONFIRM_ENTRY_SESSION
    APP->>CRS: Stage first credential
    U->>APP: Enter confirmation credential
    APP->>LCS: CANDIDATE_READY
    LCS-->>APP: REQUEST_CREDENTIAL_REGISTER_STAGES_VALIDATION
    APP->>CRS: Compare confirmation with staged entry
    CRS-->>APP: Validation success
    APP->>LCS: STAGING_VALIDATION_SUCCESS
    LCS-->>APP: REQUEST_CREDENTIAL_REGISTER_STORAGE
    APP->>CSS: Save confirmed credential
    CSS-->>APP: Storage success
    APP->>LCS: CREDENTIAL_REGISTER_STORAGE_SUCCESS
    LCS-->>APP: END_CREDENTIAL_REGISTER_SAVING_SESSION
    APP->>APP: Erase staging and run success feedback
    APP->>LCS: CREDENTIAL_REGISTER_DONE
    LCS-->>APP: RETURN_TO_LOCKED_FROM_CREDENTIAL_REGISTER_SESSION
```

LCS owns the product phase and retry policy but never owns credential digits. The Credential Register staging component holds the
first entry and compares the confirmation; CSS owns persistent storage. App Core maps their results back into LCS events.

On validation failure, the first two mismatches return to `CREDENTIAL_REGISTER_CONFIRM_ENTRY` and increment only the registration
mismatch counter. The third mismatch resets that counter, erases transient registration data through
`LCS_ACTION_END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION`, and returns to `LOCKED`. Authentication failures and registration
mismatches remain independent policies.

### 13.3 First-Boot Registration Route

When startup retrieval reports that no credential exists, App Core may dispatch `LCS_EVENT_CREDENTIAL_NOT_REGISTERED` while LCS is
still in `BOOT`. That transition activates the service directly in `CREDENTIAL_REGISTER_FIRST_ENTRY`; authentication is
intentionally bypassed because no installed credential exists. The remaining registration phases are identical to the authorized
replacement flow.

The LCS route is operational, but App Core still has to perform startup CSS retrieval and select this event instead of
`LCS_EVENT_INIT_OK`. Cancellation or entry timeout currently returns to `LOCKED` even on first boot; because that leaves no usable
credential, a future product policy may need to distinguish initial enrollment from credential replacement.

### 13.4 Rejected Access and Lockout

For each rejected candidate:

1. `LCS_EVENT_AUTH_FAILURE` increments the private failure counter with saturation.
2. The FSM enters `ACCESS_DENIED_LOCKED`.
3. The application receives `LCS_ACTION_DENY_ACCESS` and performs bounded denial feedback while preserving the physical lock.
4. When that feedback interval ends, the application dispatches `LCS_EVENT_DENIED_ACCESS_TIMEOUT`.
5. Below three failures, the FSM returns to `LOCKED`.
6. At three failures, the FSM enters `LOCKOUT` and returns `LCS_ACTION_ENTER_LOCKOUT`.

Entry requests have no valid transition during lockout and are ignored. When `LCS_EVENT_LOCKOUT_TIMEOUT` is processed, the counter resets and the FSM returns to `LOCKED`.

Lockout is deliberately selected after the third denial-feedback interval ends, not immediately inside the `AUTH_FAILURE` transition. This allows the application to complete the access-denied presentation before beginning the lockout presentation and timer.

### 13.5 Cancellation or Entry Timeout

Both of these events return an active entry phase to `LOCKED`:

- `LCS_EVENT_CREDENTIAL_CANCELLED`
- `LCS_EVENT_ENTRY_TIMEOUT`

For normal credential entry, cancellation returns `LCS_ACTION_END_CREDENTIAL_ENTRY_SESSION` and timeout returns
`LCS_ACTION_RETURN_TO_LOCKED_FROM_ENTRY_TIMEOUT`; both clear the pending purpose. In registration first entry and confirmation
entry, cancellation and timeout return their phase-specific end action, reset the mismatch counter and require App Core to erase
all transient registration data.

### 13.6 Ignored Events

An event is ignored when:

- Normal operation is inactive and the event is not `INIT_OK`, `INIT_FAIL`, or `CREDENTIAL_NOT_REGISTERED`.
- No table record matches the current state and event.
- Matching source/event records exist but no guard authorizes them.
- A sentinel such as `LCS_EVENT_NONE` or `LCS_EVENT_COUNT` is supplied.
- An out-of-range enum value reaches the API.

Ignored events preserve the complete runtime context and return `LCS_ACTION_NONE`.

---

## 14. API Reference

### 14.1 LCS_Process

```c
LCS_Action_t LCS_Process(LCS_Event_t Event);
```

Processes one semantic event through the authoritative state machine.

#### Parameter

`Event` is one `LCS_Event_t` fact supplied by the application.

#### Return Value

Returns the semantic application action associated with the accepted transition, or `LCS_ACTION_NONE` when no external work is requested.

#### Processing Order

For an accepted transition, the function:

1. Selects the first authorized table record.
2. For authentication success, requires the record's pending discriminator to match `pending_request`.
3. Applies its private internal effect.
4. Commits its target state.
5. Returns its semantic action.

#### Preconditions

- The service shall leave boot through `LCS_EVENT_INIT_OK`, `LCS_EVENT_INIT_FAIL`, or
  `LCS_EVENT_CREDENTIAL_NOT_REGISTERED` before later operational events are expected to work.
- Every path into `AUTHENTICATING` shall retain a nonzero pending purpose established by LCS itself.
- Events shall be dispatched from one serialized execution context.
- Result events produced by executing an action shall be dispatched after the current call returns.

#### Side Effects

The function may modify only the private singleton runtime. It does not call hardware, allocate memory, block, or invoke another service.

---

## 15. Design Decisions

### Table-Driven FSM

The transition table separates the stable processing mechanism from evolving product policy. `LCS_Process()` does not need one branch for every state and event combination.

### Sparse Linear Representation

Only valid transitions consume storage. The current table has thirty records, while a dense matrix for thirteen runtime states and
nineteen dispatchable event identifiers would reserve many unused cells.

### Private State Representation

Application code reacts to semantic actions rather than inspecting LCS state. The FSM can therefore be reorganized internally without turning private states into a cross-layer API.

### Semantic Events

Events represent already-interpreted facts rather than GPIO levels, raw key codes, timestamps, or service-specific internal states.

### Semantic Actions

Actions represent product intent rather than calls or electrical commands. This prevents the state machine from depending on which LCD, sound device, indicator, timer implementation, or actuator is used.

### Separate Internal Effects

Private policy mutations are declared in transition records and handled by a small internal interpreter. They remain distinct from target-state commit and application-visible action execution.

### Shared Authentication with Explicit Purpose

Unlock and credential-register authorization share one entry/authentication path. A private pending value records the purpose,
and each authentication-success record stores the purpose required to select it. This avoids duplicated states and keeps routing
explicit in the table.

### Explicit Registration Phases

Five private states expose the product phases required to register a credential without storing any credential bytes inside LCS.
The FSM decides when first entry, confirmation, validation, persistence and success feedback are active; App Core executes each
semantic action through CES, credential-register staging, CSS and presentation services.

### One Event and One Action per Call

Each invocation consumes one event and returns at most one action. This keeps call ordering explicit and makes state evolution straightforward to test.

### Independent Saturating Counters

Authentication failures and registration-confirmation mismatches have separate counters and separate limits. Both saturate at
three, preventing wraparound and preventing a confirmation mistake from changing the access lockout policy.

### Explicit Boot Gate

The singleton begins in a safe inactive state. Normal events are ignored until startup validation dispatches
`LCS_EVENT_INIT_OK` or `LCS_EVENT_CREDENTIAL_NOT_REGISTERED`. The latter activates the FSM directly in registration first entry;
`LCS_EVENT_INIT_FAIL` remains admitted only to enter the fault path.

---

## 16. Error Handling and Fail-Safe Behavior

The service uses safe default behavior at its input and policy boundaries:

- Unknown or context-invalid events do not change state.
- Unknown guards evaluate to `false`.
- Unknown internal effects perform no private mutation.
- The failed-attempt counter saturates instead of wrapping.
- The registration-mismatch counter saturates independently instead of wrapping.
- Access denial and lockout preserve the semantic safe-lock requirement.
- Startup failure enters `FAULT` and requests a controlled reset.
- Credential-storage failure enters `FAULT` and requests a controlled reset.

`LCS_EVENT_INIT_FAIL` and `LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_FAILURE` are the two transitions into `LCS_STATE_FAULT`. Once
fault is entered, the current table defines no outgoing transition. The application is expected to preserve safe outputs and
perform the controlled-reset policy requested by `LCS_ACTION_REQUEST_CONTROLLED_RESET`.

The current implementation does not yet define a global critical runtime-fault event with precedence over all operational states. If that requirement is added, its priority and recovery policy shall be explicitly represented and tested rather than inferred from the existing boot-failure path.

Failure to execute a returned action is outside the current LCS contract. A future integration that can detect actuator, display, timer, or service-execution failures should map them to explicit semantic events and transitions, with safe actuator behavior taking precedence over presentation effects.

---

## 17. Timing and Concurrency

`LCS_Process()` is synchronous, non-blocking, and deterministic for the current runtime context and event.

Its transition lookup is `O(N)`, where `N` is the number of transition records. With the current small product FSM, the bounded linear scan favors readability and extensibility over a larger indexed structure.

The service does not:

- Read the system tick.
- Measure durations.
- Busy-wait or sleep.
- Schedule callbacks.
- Create or consume RTOS objects.
- Protect the singleton with a mutex or critical section.

All calls shall originate from one serialized execution context. If events can arrive from interrupts, callbacks, or multiple tasks, the application shall serialize them before calling `LCS_Process()`.

An ISR should normally publish a semantic fact to the application event mechanism instead of calling LCS concurrently with the main loop.

---

## 18. Security and Safety Considerations

- The service never receives or stores credential digits.
- Failed-attempt state is private and cannot be modified through the public API.
- Successful authentication resets prior consecutive failures.
- Three consecutive failures lead to temporary lockout after denial feedback completes.
- Entry requests are rejected by omission while lockout is active.
- Invalid events cannot directly force an unlock transition.
- Authentication success cannot select a destination without a pending purpose established by an accepted LCS transition.
- Registration authorization uses the same failed-attempt and lockout policy as unlock authorization.
- Registration confirmation mismatches do not increment or reset authentication failures.
- The third confirmation mismatch aborts registration and clears transient mismatch history.
- Persistent storage failure cannot produce registration-success feedback or return normal operation directly.
- Pending operation context contains no credential bytes and is cleared on terminal paths.
- The wake key does not become an implicit credential digit.
- The service requests unlock semantically; only the application can operate the actuator.
- A returned unlock action is not proof that the physical lock successfully moved.
- The application shall enforce safe locked output during denial, lockout, startup failure, and action-execution failure.
- Sensitive candidate cleanup remains an application and Credential Entry Service responsibility.

The in-memory failure counter is not persistent. Resetting or power-cycling the device clears it through static initialization. If persistent anti-brute-force policy is required, it must be designed with storage endurance, atomicity, corruption handling, and tamper behavior in mind.

---

## 19. Testing and Acceptance Criteria

The dedicated [host test suite](../../../Tests/README.md) compiles the production `Lock_Control_Service.c` directly and validates
the public `LCS_Process()` contract without accessing private state. Its scenarios cover:

- Every transition record.
- Sentinel, out-of-range, and representative state-invalid events with explicit state-preservation checks.
- Guard boundary conditions.
- Counter-reset behavior after success and lockout.
- Registration mismatch retry, third-mismatch abortion, and counter cleanup.
- Pending unlock selection after normal entry.
- Pending registration selection after active entry is reclassified.
- Pending preservation through incomplete entry and the move to `AUTHENTICATING`.
- Pending cleanup after success, failure, cancellation, timeout, unlock completion, and registration authorization.
- Registration first-entry and confirmation cancellation, incomplete-entry, and timeout paths.
- Storage success, storage failure, and success-feedback completion paths.
- Rejection of `AUTH_SUCCESS` when no request is pending.
- The first-boot registration route and its activation effect.
- Startup-failure behavior in an isolated process.
- Sentinel, out-of-range and state-invalid events without runtime mutation.

Because the service has a private singleton and no public reset API, CTest runs each scenario in a separate host process. Every
scenario therefore starts from the statically initialized `BOOT` state without adding a production reset hook.

The host-test README is the authoritative execution and maintenance guide. Its [scenario catalog](../../../Tests/README.md#7-lcs-scenario-catalog)
documents the objective and observable acceptance result of every test, while its [extension procedure](../../../Tests/README.md#12-extending-the-suite-when-lcs-changes)
defines the coverage that shall accompany new states, events, guards, effects, actions, or transitions.

Future critical-fault precedence and application action-execution feedback will require additional tests when those contracts are
defined.

---

## 20. Limitations and Future Improvements

Current limitations include:

- One private singleton instance; multiple physical locks are not supported by one process.
- No public reset, snapshot, state-query, or diagnostic API.
- No event payload; all event values are identifier-only.
- One semantic action returned per processed event.
- Pending-operation discrimination is specialized to `AUTHENTICATING + AUTH_SUCCESS` in the lookup helper.
- First-match behavior makes overlapping guard order significant.
- Linear lookup cost grows with the number of transition records.
- The failure limit is a private compile-time policy constant.
- The failure counter is not persistent across resets.
- Startup failure is currently the only modeled fault transition.
- No global critical runtime-fault precedence is currently modeled.
- No transition exists out of `LCS_STATE_FAULT`.
- `LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_SAVING_SESSION` is reserved but currently selected by no transition.
- Initial registration and authorized replacement share the same registration states, so cancellation during first boot returns to
  `LOCKED` even though no credential exists.
- Validation and persistence states accept no cancellation event because they represent synchronous result boundaries.
- No explicit feedback event reports that an application action failed to execute.
- No automatic static validation currently checks table reachability, duplicate unconditional records, or guard overlap.

Possible future improvements include:

- Generate transition-coverage cases directly from the table.
- Add compile-time or host-side table validation.
- Add explicit critical-fault and action-failure events after defining product precedence.
- Add optional diagnostic snapshots without exposing mutable state.
- Parameterize policy constants through immutable configuration.
- Add persistent lockout policy if the threat model requires it.
- Add a public handle-based variant if multiple independent locks are required.
- Introduce indexing only if measured table growth makes linear lookup unsuitable.

These improvements should preserve the defining boundary: LCS decides and signals; the application composes and executes.

---

## 21. License

This module is part of the:

```text
Electronic Lock Project
```

and follows the project's license terms.
