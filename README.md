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

> [!WARNING]
> App Core is implemented, but generated [main.c](Core/Src/main.c) does not yet
> call `App_Init()`, `App_ReadInput()` or `App_Dispatch()`. Product
> behavior is not active on target until that integration is added in CubeMX
> user-code regions.

### Current status

| Area | Current state |
| --- | --- |
| Target and CubeMX project | STM32F411CEU6 configuration present |
| Platform layer | GPIO, I2C, PWM and Time implemented |
| Component layer | Keyboard, LCD, PCF8574, LED and buzzer implemented |
| Service layer | Domain and presentation services implemented |
| Application Core | Implemented as a serialized cooperative orchestrator |
| Main-loop integration | Pending |
| Actuator abstraction | Direct Platform GPIO; dedicated driver pending |
| Low-battery path | LED path exists; sensing and policy pending |
| Verification | Module and target coverage still requires expansion |

The prototype does not provide persistent users, credential updates, access
logs, connectivity, tamper protection, mechanical position feedback or
certified access-control guarantees.

LCS now models the complete product-level credential-register flow: first
entry, confirmation, validation, persistence result and success feedback. App
Core does not yet execute the required staging, CSS and presentation actions,
so credential updates remain unavailable in the current product integration.

---

## Product Contract

### Authoritative parameters

| Parameter | Value | Owner |
| --- | ---: | --- |
| Credential length | 6 digits | Credential Entry, Authentication and Credential Storage |
| Consecutive failure limit | 3 | Lock Control |
| Credential-confirmation mismatch limit | 3 | Lock Control |
| Keyboard debounce | 40 ms | Matrix Keyboard configuration |
| Credential-entry inactivity | 5,000 ms | App Core timeout table |
| Authorized unlock | 3,000 ms | App Core timeout table |
| Access-denied feedback | 1,500 ms | App Core timeout table |
| Lockout | 10,000 ms | App Core timeout table |
| Synchronous LCS follow-up limit | 4 actions | App Core |
| LCD | 16 columns x 2 rows | LCD/App Core |
| PCF8574 address | 0x20, 7-bit | App Core |
| Backlight request | 1,500 Hz at 50% | App Core/PWM adapter |

The four durations in
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

- [complete App Core reference](App/Core/README.md);
- [FSM diagram and transition table](App/Core/README.md#12-lock-control-state-machine-integration);
- [native host tests for the LCS FSM](Tests/README.md);
- [timeout lifecycle](App/Core/README.md#15-application-timeout-model);
- [service update order](App/Core/README.md#16-service-update-cycle);
- [keyboard and physical-input model](App/Core/README.md#10-keyboard-model);
- [credential-entry outcomes](App/Core/README.md#11-credential-entry-outcomes);
- [presentation policy](App/Core/README.md#17-presentation-policy).

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
| PA3, PA2, PA1, PA0 | Keyboard rows 0 through 3; pull-up/falling EXTI configuration |
| PA7, PA6, PA5, PA4 | Keyboard columns 0 through 3 |
| PA15 | Active-low lock-status LED |
| PA12 | Active-low low-battery LED |
| PB8 | Lock actuator; low safe, high unlock request |
| PB6/PB7 | I2C1 SCL/SDA at 100 kHz |
| PB4 | TIM3 channel 1 buzzer PWM |
| PB9 | TIM4 channel 4 LCD-backlight PWM |
| TIM2 | Raw time counter; microsecond resolution/wrap contract requires validation |
| PC13 | On-board diagnostic LED |

CubeMX-generated GPIO initialization and App Core both request PB8 low before
normal operation. Firmware state describes the commanded output, not confirmed
mechanical lock position. The external actuator stage remains responsible for
load current, inductive protection and electrical safety.

Hardware and Platform details:

- [CubeMX configuration](Electronic-Lock.ioc);
- [Platform interfaces](Platforms/README.md);
- [App Core board composition](App/Core/README.md#7-product-configuration-owned-by-app-core).

---

## Architecture and Documentation

~~~mermaid
flowchart TB
    MAIN["Execution owner"]
    APP["Application Core"]
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

Dependencies point toward hardware capabilities. App Core is the composition
root; LCS owns authoritative product state; CES owns the active candidate;
presentation services own UI patterns; components own device behavior; Platform
interfaces isolate HAL and generated handles.

Runtime APIs are serialized and non-reentrant:

~~~c
App_InitStatus_t App_Init      (void);
void             App_ReadInput (void);
void             App_Dispatch  (void);
~~~

`App_ReadInput()` performs one keyboard acquisition and immediately processes
completed input. `App_Dispatch()` polls the active timeout and advances
display, two indication instances and sound once. See the
[public API and execution model](App/Core/README.md#5-public-api).

### Application and infrastructure

| Module | Responsibility | Detailed documentation |
| --- | --- | --- |
| App Core | Composition, input routing, authentication coordination, actions, timeouts and UI orchestration | [README](App/Core/README.md) |
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
[App Core failure policy](App/Core/README.md#20-failure-policy).

### Security boundary

The credential is fixed in firmware. CES stores the active candidate; App Core
copies it only for authentication and explicitly erases that copy. UI services
receive masked length, never raw digits.

This reduces accidental exposure but does not provide secure storage,
firmware-extraction resistance, a secure element, tamper response, protected
debug, credential rotation or audit logging. Do not reuse the development
credential in a real installation.

---

## Build and Integration

1. Import the repository through **File > Import > Existing Projects into
   Workspace** in STM32CubeIDE.
2. Inspect [Electronic-Lock.ioc](Electronic-Lock.ioc) before regenerating code.
3. Build with the STM32CubeIDE-managed GNU Arm toolchain.
4. Flash the target and verify PB8 safe startup before connecting an actuator
   load.

Pending integration belongs only in CubeMX user-code regions:

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

See the [execution model](App/Core/README.md#6-current-execution-model) and
[initialization graph](App/Core/README.md#9-initialization-graph).

---

## Verification

Before product use, verify at minimum:

- PB8 safe startup and safe return on every failure path;
- complete keyboard map, wake-key consumption and 40 ms debounce;
- six masked digits, incomplete refresh and clear/cancel behavior;
- exact 5 s, 3 s, 1.5 s and 10 s intervals plus measured dispatch latency;
- failure-count reset and lockout entry after the third denial feedback;
- LCD, backlight, LEDs and every ringtone on target;
- I2C/PWM/time behavior and timestamp rollover;
- reset or induced fault while unlocked;
- UI failure cannot produce or prolong unlock.

Use the [App Core verification checklist](App/Core/README.md#26-verification-checklist)
and each module README for unit-, integration- and hardware-specific cases.

---

## Known Constraints

- Generated `main.c` is not yet connected to App Core.
- Main-loop cadence and maximum timeout-observation latency are not yet
  measured.
- PB8 is controlled directly through Platform GPIO; no dedicated actuator
  driver or redundant local energization deadline exists.
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
[App Core deferred-work section](App/Core/README.md#24-known-constraints-and-deferred-work)
and the corresponding module READMEs.

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
