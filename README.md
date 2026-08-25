# STM32F411 Electronic Lock

Layered firmware for an STM32F411CEU6 electronic lock with a 4x4 matrix
keyboard, 16x2 LCD, status LEDs, passive buzzer and GPIO-controlled actuator.

## Contents

1. [Project Overview](#project-overview)
2. [Product Contract](#product-contract)
3. [Hardware Baseline](#hardware-baseline)
4. [Architecture and Documentation](#architecture-and-documentation)
5. [Safety and Security](#safety-and-security)
6. [Build and Integration](#build-and-integration)
7. [Verification](#verification)
8. [Known Constraints](#known-constraints)
9. [Documentation Authority](#documentation-authority)

---

## Project Overview

The project is an engineering prototype that demonstrates a complete local
credential-entry and timed-unlock flow while keeping business policy separate
from STM32 hardware access.

The firmware:

- starts with the actuator in the safe locked command;
- accepts one fixed six-digit numeric credential;
- masks credential progress on the LCD;
- authenticates locally;
- unlocks for a bounded interval after successful authentication;
- provides non-blocking display, LED and sound feedback;
- tracks consecutive failures and enters temporary lockout;
- erases transient credential data on refresh and terminal paths.

> [!IMPORTANT]
> The implemented execution model is synchronous and cooperative. App Core
> creates no FreeRTOS tasks, queues, mutexes or software timers.

> [!IMPORTANT]
> Generated [main.c](Core/Src/main.c) initializes App Core once and then calls
> `App_ReadInput()` and `App_Dispatch()` cooperatively from the main loop.

### Current status

| Area | Current state |
| --- | --- |
| Target and CubeMX project | STM32F411CEU6 configuration present |
| Platform layer | GPIO, I2C, PWM and Time implemented |
| Component layer | Keyboard, LCD, PCF8574, LED, buzzer, lock actuator, door sensor and exit button implemented |
| Service layer | Domain and presentation services implemented |
| Application Core | Implemented as a serialized cooperative orchestrator |
| Main-loop integration | Active through `App_Init()`, `App_ReadInput()` and `App_Dispatch()` |
| Door-mechanism preparation | Lock actuator, door sensor and exit button composed and initialized; Door Control Service pending |
| Low-battery path | LED path exists; sensing and policy pending |
| Verification | Module and target coverage still requires expansion |

The prototype does not provide persistent users, credential updates, access
logs, connectivity, tamper protection, mechanical position feedback or
certified access-control guarantees.

LCS and App Executor implement the local credential-register flow: first entry,
confirmation, staging validation, Flash persistence result and success
feedback. This remains a single-device prototype rather than a multi-user
credential-management system.

---

## Product Contract

### Authoritative parameters

| Parameter | Value | Owner |
| --- | ---: | --- |
| Credential length | 6 digits | Credential Entry, Authentication and Credential Storage |
| Consecutive failure limit | 3 | Lock Control |
| Credential-confirmation mismatch limit | 3 | Lock Control |
| Keyboard debounce | 40 ms | Matrix Keyboard configuration |
| Exit-button debounce | 20 ms | Exit Button configuration |
| Credential-entry inactivity | 5,000 ms | App Core timeout table |
| Authorized unlock | 3,000 ms | App Core timeout table |
| Access-denied feedback | 1,500 ms | App Core timeout table |
| Lockout | 10,000 ms | App Core timeout table |
| Synchronous LCS follow-up limit | 4 actions | App Core |
| LCD | 16 columns x 2 rows | LCD/App Core |
| PCF8574 address | 0x20, 7-bit | App Core |
| Backlight request | 1,500 Hz at 50% | App Core/PWM adapter |

The five durations configured in
[App_Config.h](App/Config/Inc/App_Config.h) and mapped to semantic events in
[App_Core.c](App/Core/Src/App_Core.c) are the authoritative product timeouts.
Older architecture documents do not override them.

### Essential business rules

1. PB8 low is the safe locked command.
2. The first complete key click while locked opens credential entry and is
   consumed as a wake action.
3. Digits `0` through `9` enter the candidate; `#` confirms and `*`
   clears or cancels.
4. Confirming fewer than six digits erases the candidate, preserves the active
   session and restarts the 5-second entry timeout.
5. A non-empty `*` clears the candidate without restarting the current entry
   timeout; an empty `*` cancels the session.
6. The 3-second unlock timeout is established before PB8 may be driven high.
7. Authentication failure starts 1.5 seconds of denial feedback.
8. The third consecutive failure enters 10-second lockout after its denial
   feedback completes.
9. Authentication success and lockout expiry reset the failed-attempt count.
10. App Core owns one mutually exclusive product timeout and emits each
    expiration once.
11. Service updates have no execution timeout; they are called continuously to
    advance non-blocking behavior.
12. Raw credentials are never rendered and transient copies are explicitly
    erased.

Detailed behavior:

- [complete application-layer reference](App/README.md);
- [authoritative Lock Control FSM](Libs/Services/Lock_Control/README.md);
- [native host tests for the LCS FSM](Tests/README.md);
- [timeout lifecycle](App/README.md#9-timeout-model);
- [public API and service update order](App/README.md#5-public-api-and-execution-model);
- [keyboard and physical-input model](App/README.md#8-keyboard-policy);
- [credential ownership and security](App/README.md#12-credential-ownership-and-security);
- [action and presentation coordination](App/README.md#11-action-execution).

---

## Hardware Baseline

- MCU: STM32F411CEU6, Arm Cortex-M4F, UFQFPN48;
- system clock: 100 MHz from HSI through PLL;
- APB1: 50 MHz, with 100 MHz timer clock;
- APB2: 100 MHz;
- HAL millisecond time: SysTick;
- configuration source: [Electronic-Lock.ioc](Electronic-Lock.ioc).

| Resource | Assignment |
| --- | --- |
| PA3, PA2, PA1, PA0 | Keyboard rows 0 through 3; pull-up/rising-and-falling EXTI configuration |
| PA7, PA6, PA5, PA4 | Keyboard columns 0 through 3 |
| PA15 | Active-low lock-status LED |
| PA12 | Active-low low-battery LED |
| PB8 | Lock actuator; low safe, high unlock request |
| PB0 | Door-contact sensor; pull-up input, low active |
| PB10 | Exit button; pull-up, active low, rising/falling EXTI |
| PB6/PB7 | I2C1 SCL/SDA at 100 kHz |
| PB4 | TIM3 channel 1 buzzer PWM |
| PB9 | TIM4 channel 4 LCD-backlight PWM |
| TIM2 | Raw time counter; microsecond resolution/wrap contract requires validation |
| PC13 | On-board diagnostic LED |

CubeMX-generated GPIO initialization requests PB8 low before `App_Init()`. App
Core then binds the Platform descriptor; its redundant explicit safe-state write
is currently disabled and tracked as application safety debt. Firmware state
describes the commanded output, not confirmed mechanical lock position. The
external actuator stage remains responsible for load current, inductive
protection and electrical safety.

App Core also binds PB0 to the Door Sensor Driver and PB10 to the Exit Button
Driver. `App_ConfigHalCallbacks.c` forwards PB10 EXTI activity to
`ExitButton_NotifyInterrupt()` using the application millisecond time base. The
planned Door Control Service will own `DoorSensor_GetState()`,
`ExitButton_Update()` and coordinated lock policy; those runtime operations are
intentionally not performed by App Core yet.

Hardware and Platform details:

- [CubeMX configuration](Electronic-Lock.ioc);
- [Platform interfaces](Platforms/README.md);
- [App configuration and hardware bindings](App/Config/README.md#5-hardware-bindings).

---

## Architecture and Documentation

~~~mermaid
flowchart TB
    MAIN["Execution owner"]
    APP["Application Layer<br/>Config + Core + Executor"]
    DOMAIN["Domain services"]
    UI["Presentation services"]
    COMPONENTS["Components and adapters"]
    PLATFORM["Platform interfaces"]
    HAL["HAL / CMSIS / CubeMX"]

    MAIN --> APP
    APP --> DOMAIN
    APP --> UI
    APP --> COMPONENTS
    APP --> PLATFORM
    UI --> COMPONENTS
    COMPONENTS --> PLATFORM
    PLATFORM --> HAL
~~~

Dependencies point toward hardware capabilities. App Config owns product
bindings and retained objects, App Core initializes and dispatches, and App
Executor applies semantic actions. LCS owns authoritative product state; CES
owns the active candidate; presentation services own UI patterns; components
own device behavior; Platform interfaces isolate HAL and generated handles.

Runtime APIs are serialized and non-reentrant:

~~~c
App_InitStatus_t App_Init      (void);
void             App_ReadInput (void);
void             App_Dispatch  (void);
~~~

`App_ReadInput()` performs one keyboard acquisition and immediately processes
completed input. `App_Dispatch()` polls the active timeout and advances
display, two indication instances and sound once. See the
[public API and execution model](App/README.md#5-public-api-and-execution-model).

### Application and infrastructure

| Module | Responsibility | Detailed documentation |
| --- | --- | --- |
| Application Layer | Product configuration, static composition, input/event orchestration, action execution, timeouts and UI coordination | [README](App/README.md) |
| App Config | Board/product policy and static runtime-object registry | [README](App/Config/README.md) |
| Platform | Project-owned GPIO, I2C, PWM and Time boundary over STM32 | [README](Platforms/README.md) |
| CubeMX Core | Generated startup, clocks, GPIO, I2C and timers | [main.c](Core/Src/main.c), [IOC](Electronic-Lock.ioc) |
| STM32 Drivers | Vendor HAL and CMSIS implementation | [Drivers](Drivers) |

### Domain services

| Service | Responsibility | Detailed documentation |
| --- | --- | --- |
| Lock Control | Authoritative FSM, authentication-purpose routing, registration phases and retry policies | [README](Libs/Services/Lock_Control/README.md) |
| Credential Entry | Active session, normalized candidate and clear/cancel semantics | [README](Libs/Services/Credential_Entry/README.md) |
| Credential Storage | Persistent six-digit credential record, integrity validation and Flash transaction policy | [README](Libs/Services/Credential_Storage/README.md) |
| Authentication | Synchronous comparison of one complete candidate | [README](Libs/Services/Authentication/README.md) |
| Timeout Validation | Rollover-safe evaluation of caller-owned intervals | [README](Libs/Services/Timeout_Validation/README.md) |

### Presentation services

| Service | Responsibility | Detailed documentation |
| --- | --- | --- |
| Display Render | Semantic screens and masked credential progress | [README](Libs/Services/Display_Render/README.md) |
| Status Indication | Non-blocking semantic LED patterns | [README](Libs/Services/Status_Indication/README.md) |
| Sound Generator | Non-blocking semantic buzzer patterns and priority | [README](Libs/Services/Sound_Generator/README.md) |

### Components and adapters

| Component | Responsibility | Detailed documentation |
| --- | --- | --- |
| Matrix Keyboard | Matrix scan, debounce and complete key actions | [README](Libs/Components/MatrixKeyboard/README.md) |
| LCD | HD44780 behavior, PCF8574 bus adapter and PWM backlight adapter | [README](Libs/Components/LCD/README.md) |
| PCF8574 | I2C I/O-expander access and port shadow | [README](Libs/Components/PCF8574/README.md) |
| LED | Active-level-independent digital LED control | [README](Libs/Components/Led/README.md) |
| Buzzer | Passive-buzzer PWM control | [README](Libs/Components/Buzzer/README.md) |
| Lock Actuator | Polarity-independent digital lock and unlock commands | [README](Libs/Components/LockActuator/README.md) |
| Door Sensor | Polarity-independent digital door-contact state | [README](Libs/Components/DoorSensor/README.md) |
| Exit Button | Interrupt-oriented, non-blocking debounced exit-button events | [README](Libs/Components/ExitButton/README.md) |

---

## Safety and Security

### Safety invariants

- PB8 must remain low during startup, denial, cancellation, timeout, lockout and
  fault handling.
- Unlock is forbidden unless its finite deadline is already active.
- Presentation failure must not block the independent actuator-safe request.
- Product timing uses timestamps rather than human-scale blocking delays.
- App Core calls must not overlap or run from an ISR.
- Dispatch must continue while the actuator is unlocked.
- A controlled-reset path cancels timing, erases CES, forces PB8 low, stops
  sound and disables the backlight before reset.

The detailed failure mapping is maintained in the
[application safety and failure policy](App/README.md#13-safety-and-failure-policy).

### Security boundary

CSS owns the persistent credential record. CES stores the active candidate;
App Executor copies it only for one synchronous authentication or registration
operation and explicitly erases that copy. App Config retains the installed
credential in RAM after a successful load or replacement. UI services receive
masked length, never raw digits.

This reduces accidental exposure but does not provide a secure element,
firmware-extraction resistance, tamper response, protected debug or audit
logging. Production deployment still requires a dedicated security review.

---

## Build and Integration

1. Import the repository through **File > Import > Existing Projects into
   Workspace** in STM32CubeIDE.
2. Inspect [Electronic-Lock.ioc](Electronic-Lock.ioc) before regenerating code.
3. Build with the STM32CubeIDE-managed GNU Arm toolchain.
4. Flash the target and verify PB8 safe startup before connecting an actuator
   load.

The current integration is kept inside CubeMX user-code regions:

~~~c
if (App_Init() != APP_INIT_SUCCESSFULLY)
{
    Error_Handler();
}

while (1)
{
    App_ReadInput();
    App_Dispatch();
}
~~~

This example defines call order, not a measured cadence. Do not add a blocking
delay to implement debounce or product timeouts. The execution owner must
measure loop latency against input, presentation and timeout-response
requirements.

After CubeMX regeneration, review the diff and confirm:

- PB8 starts low;
- PA12 and PA15 start high because their LEDs are active-low;
- PB4 remains TIM3 channel 1;
- PB9 remains TIM4 channel 4;
- PB6/PB7 remain I2C1;
- every project-owned source directory remains in the build.

See the [execution model](App/README.md#5-public-api-and-execution-model) and
[initialization graph](App/README.md#6-initialization-and-object-lifetime).

---

## Verification

Before product use, verify at minimum:

- PB8 safe startup and safe return on every failure path;
- complete keyboard map, wake-key consumption and 40 ms debounce;
- PB0 active-low door-contact mapping and electrical open/closed behavior;
- PB10 press/release EXTI delivery and 20 ms exit-button debounce;
- six masked digits, incomplete refresh and clear/cancel behavior;
- exact 5 s, 3 s, 1.5 s and 10 s intervals plus measured dispatch latency;
- failure-count reset and lockout entry after the third denial feedback;
- LCD, backlight, LEDs and every ringtone on target;
- I2C/PWM/time behavior and timestamp rollover;
- reset or induced fault while unlocked;
- UI failure cannot produce or prolong unlock.

Use the [App Config validation checklist](App/Config/README.md#13-validation-checklist)
and each module README for unit-, integration- and hardware-specific cases.

---

## Known Constraints

- Main-loop cadence and maximum timeout-observation latency are not yet
  measured.
- PB8 is still commanded directly through Platform GPIO by App Executor; the
  dedicated driver is initialized but command ownership has not moved to the
  planned Door Control Service, and no redundant local energization deadline
  exists.
- The door-sensor driver is bound to PB0 and initialized, but runtime reads and
  open/closed policy await the planned Door Control Service.
- The exit-button driver is bound to PB10, initialized and notified from EXTI,
  but debounced event consumption and request-to-exit policy await the planned
  Door Control Service.
- `App_InitLockActuator()` currently relies on the CubeMX PB8 startup level
  because its explicit safe-state write is disabled.
- Presentation-service failures are mostly treated as degraded feedback rather
  than global operational faults.
- Low-battery sensing and policy are not implemented.
- Keyboard EXTI is configured, but normal acquisition is polling; its final
  wake-only/unused policy remains open.
- TIM2 microsecond resolution and wrap behavior require correction or
  validation.
- Automated host and hardware-in-the-loop coverage remains incomplete.
- Power management is not integrated.
- The single App Core timeout model does not support overlapping product
  intervals.
- A future RTOS design would require explicit ownership and synchronization;
  current App Core APIs cannot be called concurrently.
- Persistent credentials require a separate security and data-integrity design.
- Mechanical position sensing is absent.

Implementation-specific constraints are tracked in the
[App known-constraints section](App/README.md#17-known-constraints) and the
corresponding module READMEs.

---

## Documentation Authority

When artifacts disagree:

1. [Electronic-Lock.ioc](Electronic-Lock.ioc) defines intended CubeMX hardware
   configuration.
2. Public headers define callable APIs and types.
3. Source files define implemented runtime behavior.
4. Module READMEs explain module behavior and constraints.
5. This README summarizes the current project baseline.
6. [References](References) provide historical input only.

Historical product input:
[Electronic Lock V1 Product Specification](References/Architecture/Electronic-Lock-V1-Product-Specification.pdf).

The project is distributed under the terms of [LICENSE](LICENSE).
