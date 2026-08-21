<h1 align="left">Native Host Tests</h1>

<p align="left">
  <big>
    Reproducible, hardware-independent behavioral validation for project<br>
    services that can execute with a native C compiler.
  </big>
</p>

> [!IMPORTANT]
> The current suite is a **black-box validation of the production Lock Control Service**. It compiles the real `Lock_Control_Service.c`, includes only its public header and observes only the `LCS_Action_t` returned by `LCS_Process()`. It does not copy the transition table, expose private state, mock the FSM or add test-only behavior to production code.

> [!NOTE]
> The test CMake project is intentionally separate from the STM32 firmware build. Configure it with a native host compiler, not with the repository's ARM firmware presets. Each scenario runs in a separate process so the private LCS singleton always starts from its statically initialized boot state.

---

## Table of Contents

* [1. Overview](#1-overview)
* [2. Test Objectives](#2-test-objectives)
* [3. Test Architecture](#3-test-architecture)

  * [3.1 Production Code Under Test](#31-production-code-under-test)
  * [3.2 Process Isolation](#32-process-isolation)
  * [3.3 Behavioral Oracle](#33-behavioral-oracle)
* [4. Directory Structure](#4-directory-structure)
* [5. Requirements](#5-requirements)
* [6. Build Configuration](#6-build-configuration)
* [7. LCS Scenario Catalog](#7-lcs-scenario-catalog)
* [8. Coverage Model](#8-coverage-model)

  * [8.1 FSM Behavior](#81-fsm-behavior)
  * [8.2 Runtime Policy](#82-runtime-policy)
  * [8.3 Invalid Input](#83-invalid-input)
* [9. Reproducible Execution](#9-reproducible-execution)

  * [9.1 Configure](#91-configure)
  * [9.2 Build](#92-build)
  * [9.3 Run All Scenarios](#93-run-all-scenarios)
  * [9.4 Expected Result](#94-expected-result)
* [10. Selecting and Debugging Scenarios](#10-selecting-and-debugging-scenarios)
* [11. Failure Diagnostics](#11-failure-diagnostics)
* [12. Extending the Suite When LCS Changes](#12-extending-the-suite-when-lcs-changes)

  * [12.1 Required Coverage by Change Type](#121-required-coverage-by-change-type)
  * [12.2 Adding a Scenario](#122-adding-a-scenario)
  * [12.3 Scenario Implementation Template](#123-scenario-implementation-template)
  * [12.4 Definition of Done](#124-definition-of-done)
* [13. Continuous Integration](#13-continuous-integration)
* [14. Limitations](#14-limitations)
* [15. Acceptance Criteria](#15-acceptance-criteria)
* [16. License](#16-license)

---

## 1. Overview

The `Tests` directory contains native executables for validating hardware-independent project logic on a development machine or CI runner. It is an independent CMake project and does not require the STM32 target, CubeMX-generated sources, HAL, an RTOS, a debugger probe or the `arm-none-eabi` toolchain.

The first suite validates the table-driven finite-state machine owned by the [Lock Control Service](../Libs/Services/Lock_Control/README.md). Its executable receives one scenario name, dispatches an ordered sequence of public LCS events and verifies the exact semantic action returned after each relevant event.

The suite is intended to serve two purposes:

1. Detect regressions in the current LCS transition and runtime-policy contract.
2. Provide a repeatable maintenance pattern when the FSM gains or changes states, events, guards, effects, actions or transitions.

---

## 2. Test Objectives

The LCS suite shall verify that:

* Boot prevents operational transitions until App Core reports a valid startup result.
* Critical initialization or persistence failures enter the fail-safe fault path.
* Normal credential entry routes successful authentication to bounded unlock.
* Credential-register requests reuse authentication while selecting the registration destination deterministically.
* First-boot registration bypasses authentication only when no credential is installed.
* Registration first entry, confirmation, comparison, persistence and success feedback occur in the required order.
* Cancellation, incomplete-candidate and timeout events select the action appropriate to the active entry phase.
* Authentication failures and registration mismatches use independent bounded counters.
* Guard boundaries select the correct retry, lockout or abortion transition.
* Terminal transitions clear private pending purposes and counters where required.
* Invalid, sentinel, out-of-range and out-of-context events preserve the runtime context.
* Every accepted event returns the public semantic action declared by the LCS contract.

---

## 3. Test Architecture

```mermaid
flowchart LR
    CTEST["CTest<br/>one process per scenario"] --> RUNNER["lcs_fsm_host_test<br/>scenario dispatcher"]
    RUNNER --> EXPECT["Event/action expectations"]
    EXPECT --> API["LCS_Process(event)"]
    API --> PROD["Production LCS<br/>transition table + private <br/>runtime"]
    PROD -->|"LCS_Action_t"| EXPECT
    EXPECT --> RESULT["PASS / FAIL<br/>process status"]
```

### 3.1 Production Code Under Test

`Tests/CMakeLists.txt` builds the production source directly:

```text
Libs/Services/Lock_Control/Src/Lock_Control_Service.c
```

The test includes the production public interface:

```text
Libs/Services/Lock_Control/Inc/Lock_Control_Service.h
```

There is no alternate implementation selected by a test macro. A successful test therefore validates the same state-machine logic compiled into the firmware, subject only to the host compiler and platform-independent C semantics.

### 3.2 Process Isolation

LCS owns a private singleton and exposes no reset function. Running several scenarios sequentially in one process would make later scenarios depend on the final state of earlier scenarios.

CTest solves this without weakening encapsulation:

1. It launches `lcs_fsm_host_test <scenario>`.
2. Static initialization places the singleton in `LCS_STATE_BOOT`.
3. The executable runs exactly one scenario.
4. The process exits and its private state is discarded.
5. CTest launches a fresh process for the next scenario.

Do not add a production `LCS_ResetForTest()` API. If a future LCS architecture intentionally changes singleton ownership, revise this isolation strategy and document the new lifecycle here.

### 3.3 Behavioral Oracle

Every step uses the public relation:

```text
input LCS_Event_t -> LCS_Process() -> expected LCS_Action_t
```

Private state, pending purpose and counters are not inspected. When an internal effect is silent, the test proves it through a later observable decision. Examples:

* After `LCS_EVENT_INIT_OK` returns `LCS_ACTION_NONE`, acceptance of a credential-entry request proves locked idle was reached.
* After authentication success, later failures prove the consecutive-failure counter was reset.
* After lockout timeout, a new first failure proves the counter returned to zero.
* After registration abortion, a fresh first mismatch proves the mismatch counter was reset.
* After rejected registration authorization, a new authentication success selecting unlock proves the pending registration purpose was cleared.

This keeps tests coupled to public behavior rather than private representation.

---

## 4. Directory Structure

```text
Tests/
├── CMakeLists.txt
├── README.md
└── Lock_Control/
    └── Lock_Control_Service_Test.c
```

| Path | Responsibility |
|---|---|
| `Tests/CMakeLists.txt` | Creates the native test target, applies strict compiler warnings, registers each scenario with CTest and supplies suite labels/timeouts. |
| `Tests/Lock_Control/Lock_Control_Service_Test.c` | Implements expectation support, reusable FSM paths, all isolated LCS scenarios and the command-line scenario dispatcher. |
| `Tests/README.md` | Defines objectives, reproducible execution, scenario coverage and the maintenance procedure for future changes. |

Generated files belong under a separate build directory such as `build/host-tests`; they shall not be added to `Tests`.

---

## 5. Requirements

The suite requires:

* CMake **3.22 or newer**.
* CTest from the same CMake installation.
* A native compiler with C11 support, such as GCC, Clang, AppleClang or MSVC.
* A shell or IDE terminal positioned anywhere from which the repository path is accessible.

It does not require:

* STM32CubeIDE.
* `arm-none-eabi-gcc`.
* STM32 HAL or CubeMX-generated target sources.
* Physical hardware or a debug probe.
* Third-party unit-test frameworks.

The configuration intentionally fails when CMake reports cross-compilation. This prevents an ARM executable from being registered as though it could run on the host.

---

## 6. Build Configuration

The standalone CMake project creates two targets:

| Target | Type | Purpose |
|---|---|---|
| `lcs_under_test` | Static library | Compiles the production LCS implementation with its public include directory. |
| `lcs_fsm_host_test` | Executable | Links the production service and provides the isolated scenario runner. |

Both targets require C11. Native warnings are treated as errors:

* GCC/Clang family: `-Wall -Wextra -Wpedantic -Werror`
* MSVC: `/W4 /WX`

Every CTest entry receives these properties:

| Property | Value | Purpose |
|---|---|---|
| Name prefix | `lcs.` | Groups the service's scenarios and gives filters a stable namespace. |
| Labels | `host;lcs;fsm` | Supports execution by environment, service or test class. |
| Timeout | `5` seconds | Detects an unexpected hang in logic that shall be synchronous and bounded. |

The firmware's root `Debug` and `Release` presets are not used because they intentionally select the STM32 ARM toolchain.

---

## 7. LCS Scenario Catalog

The following 14 scenarios are independently executable. Every scenario starts with a fresh process and therefore a fresh boot-state singleton.

| CTest name | Objective | Principal observable acceptance |
|---|---|---|
| `lcs.inactive_gate` | Prove operational events cannot bypass boot and normal initialization activates locked idle. | Representative boot-time events return no action; after `INIT_OK`, entry begins normally. |
| `lcs.boot_failure` | Prove initialization failure selects controlled reset and fault is absorbing. | `INIT_FAIL` returns `REQUEST_CONTROLLED_RESET`; later startup and operational events remain ignored. |
| `lcs.normal_access` | Validate incomplete-entry refresh, authentication request, successful unlock and bounded return to locked idle. | The path returns `REFRESH_CREDENTIAL_ENTRY_SESSION`, `REQUEST_AUTHENTICATION`, `GRANT_ACCESS_UNLOCK` and finally `RETURN_TO_LOCKED`. |
| `lcs.normal_exit_paths` | Validate both user cancellation and inactivity timeout during normal credential entry. | Cancellation ends the entry session; a new session timeout selects the timeout-specific locked return. |
| `lcs.authentication_lockout` | Validate three consecutive failures, the lockout gate and reset after lockout timeout. | First two denials return locked, the third enters lockout, entry is ignored there, and a later first failure remains below the limit. |
| `lcs.authentication_success_resets_counter` | Prove successful authentication clears an existing failure history. | Two failures, one successful unlock and two more failures do not cause premature lockout. |
| `lcs.registration_authorization_failure` | Validate rejection of credential-register authorization and pending-purpose cleanup. | Rejected registration returns to locked; the next normal authentication success selects unlock rather than registration. |
| `lcs.first_boot_registration_success` | Validate direct registration when Credential Storage reports no installed credential. | Authentication is bypassed; matching entries are stored, success feedback completes, and normal entry becomes available. |
| `lcs.authorized_registration_success` | Validate credential replacement after authenticating the installed credential. | Authorization enters first entry, matching stages request storage, storage success starts feedback, and completion returns locked. |
| `lcs.registration_mismatch_limit` | Validate two confirmation retries, abortion on the third mismatch and mismatch-counter reset. | First two mismatches restart confirmation, the third ends it, and a new session receives a fresh retry. |
| `lcs.registration_first_entry_exit_paths` | Validate incomplete entry, cancellation and timeout while collecting the first new credential. | Incomplete input refreshes first entry; cancellation and timeout safely end the registration session. |
| `lcs.registration_confirm_entry_exit_paths` | Validate cancellation, incomplete-entry refresh and timeout during confirmation. | Cancellation ends confirmation; incomplete input refreshes only confirmation; timeout ends and cleans the session. |
| `lcs.registration_storage_failure` | Validate fail-safe handling when persistent credential storage fails. | Storage failure requests controlled reset and later ordinary events cannot escape fault. |
| `lcs.invalid_events_preserve_state` | Validate sentinel, out-of-range and valid-but-out-of-context event rejection. | Invalid events return `LCS_ACTION_NONE`; valid follow-up events prove the previous state and path were preserved. |

The catalog is a behavioral specification. If an intentional LCS change modifies an expected action or terminal path, update the production documentation and this catalog in the same change as the test.

---

## 8. Coverage Model

### 8.1 FSM Behavior

The suite has been reviewed against the current immutable transition table and exercises:

* All **30 declared transition records** through accepted event paths.
* Normal boot, first-registration boot and initialization failure.
* Normal credential entry, authentication, access grant, denial and lockout.
* Credential-register authorization, first entry, confirmation entry, comparison, persistence and success feedback.
* Cancellation, incomplete-candidate and timeout exits for each applicable entry phase.
* Successful and failed persistent-storage outcomes.
* Return to locked idle from every recoverable terminal mode.
* Fault-state preservation after critical failures.

This is behavioral transition coverage established from the transition-table contract. The suite does not currently collect compiler instrumentation such as line, branch or MC/DC coverage.

### 8.2 Runtime Policy

The following private policy is validated indirectly:

| Policy | Boundary or invariant exercised |
|---|---|
| Pending unlock purpose | A normal authentication success selects bounded unlock. |
| Pending registration purpose | A reclassified entry success selects registration first entry. |
| Pending-purpose cleanup | Cancellation, denial and terminal registration paths do not leak routing into the next operation. |
| Authentication failure count | Counts one, two and three select the two retry returns followed by lockout. |
| Failure-count saturation/reset | Lockout blocks entry and timeout restores a fresh failure budget. |
| Success reset | Authentication success clears earlier consecutive failures. |
| Registration mismatch count | Mismatches one and two retry; mismatch three aborts. |
| Mismatch reset | A new registration session receives the full retry budget. |
| Service activation | Only the accepted startup transition enables operational behavior. |

`LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_SAVING_SESSION` is currently reserved and is not selected by the transition table; the suite intentionally does not expect an unreachable action.

### 8.3 Invalid Input

The suite checks three input classes:

1. `LCS_EVENT_NONE` and `LCS_EVENT_COUNT`, which are non-dispatchable sentinels.
2. A value greater than `LCS_EVENT_COUNT`, which validates range rejection.
3. Valid event identifiers sent in the wrong state, which validate sparse-table behavior.

An action of `LCS_ACTION_NONE` alone cannot always prove state preservation because some accepted transitions may also be intentionally silent. Each important invalid-input sequence is therefore followed by a valid state-specific event whose returned action proves the expected state remains active.

---

## 9. Reproducible Execution

The commands below assume the current directory is the repository root. The chosen binary directory is disposable and independent from the STM32 firmware build.

### 9.1 Configure

```sh
cmake -S Tests -B build/host-tests -DCMAKE_BUILD_TYPE=Debug
```

Configuration shall identify a native C compiler and finish with `Build files have been written to .../build/host-tests`.

For a completely separate reproduction without reusing an existing CMake cache, select a new directory:

```sh
cmake -S Tests -B build/host-tests-repro -DCMAKE_BUILD_TYPE=Debug
```

### 9.2 Build

```sh
cmake --build build/host-tests --parallel
```

For a multi-configuration generator such as Visual Studio, select the configuration explicitly:

```sh
cmake --build build/host-tests --config Debug
```

### 9.3 Run All Scenarios

```sh
ctest --test-dir build/host-tests --output-on-failure
```

For a multi-configuration generator:

```sh
ctest --test-dir build/host-tests -C Debug --output-on-failure
```

### 9.4 Expected Result

With the current scenario registry, a successful run ends with:

```text
100% tests passed, 0 tests failed out of 14
```

The exact duration, generator messages, compiler identification and test numbering may vary by host. The required reproducibility criteria are:

* Configuration succeeds with a native compiler.
* Both targets compile with warnings treated as errors.
* CTest discovers 14 scenarios.
* Every scenario returns a successful process status.

---

## 10. Selecting and Debugging Scenarios

List all registered tests without running them:

```sh
ctest --test-dir build/host-tests --show-only
```

Run all LCS tests by label:

```sh
ctest --test-dir build/host-tests --label-regex lcs --output-on-failure
```

Run one scenario with verbose CTest output:

```sh
ctest --test-dir build/host-tests --tests-regex '^lcs.registration_mismatch_limit$' --verbose
```

Invoke the executable directly on Linux or macOS:

```sh
./build/host-tests/lcs_fsm_host_test registration_mismatch_limit
```

For multi-configuration Windows builds, the executable is normally inside the selected configuration directory and has an `.exe` suffix:

```text
build/host-tests/Debug/lcs_fsm_host_test.exe registration_mismatch_limit
```

Running the executable without a scenario prints every accepted scenario name and exits with failure. This is intentional because an empty invocation validates nothing.

---

## 11. Failure Diagnostics

A failed action expectation reports:

```text
line <source-line>: <EVENT> returned action <actual-value>; expected <EXPECTED_ACTION> (<expected-value>)
[FAIL] <scenario-name>
```

Use the information in this order:

1. Open the reported line in `Lock_Control_Service_Test.c`.
2. Read the scenario's Doxygen objective and the nearby phase comment.
3. Compare the event/action pair with the transition table in the LCS README and production source.
4. Determine whether production behavior regressed or the public contract changed intentionally.
5. If intentional, update the LCS code, LCS documentation, scenario expectation and this catalog together.
6. Rebuild before rerunning because CTest does not compile changed sources automatically.

A fixture failure reports the exact expectation inside the fixture. The outer `LCS_TEST_REQUIRE()` does not print a second message, which keeps the first causal failure visible.

---

## 12. Extending the Suite When LCS Changes

### 12.1 Required Coverage by Change Type

| LCS change | Minimum test maintenance required |
|---|---|
| New transition using existing state/event/action | Add or extend a scenario to reach the source state, dispatch the event, verify the action and prove the target state with a follow-up event when necessary. |
| New private state | Test at least one entrance, every supported terminal exit, and representative invalid-event preservation in that state. |
| New public event | Test every state in which it is accepted and at least one relevant state in which it must be ignored. |
| New public action | Verify the exact transition that returns it. Application-side execution of the action belongs in App Core integration tests, not this black-box LCS suite. |
| New guard or changed limit | Exercise the value below the boundary, the boundary itself, and the reset/cleanup route. Test both transitions when guards share one source/event pair. |
| New internal effect | Prove the effect through a later public decision; do not expose private fields solely for the test. |
| Changed pending-operation routing | Exercise every pending purpose and prove cleanup by starting a different operation afterwards. |
| New recoverable terminal path | Verify its action, return to locked idle and acceptance of a new normal entry. |
| New critical failure path | Verify controlled-reset action and that ordinary events cannot resume operation. |
| Removed behavior | Remove obsolete expectations and confirm the old event is either ignored or routed according to the new documented contract. |

When multiple guards share the same source state and event, tests shall cover every mutually exclusive guard result. First-match table ordering is part of deterministic behavior and shall not leave an overlapping case untested.

### 12.2 Adding a Scenario

Use this sequence so a new scenario is independently runnable and documented:

1. Identify the contract change in `Lock_Control_Service.h`, `Lock_Control_Service.c` and the LCS README transition table.
2. Decide whether an existing scenario remains focused after adding the new path. Create a new scenario when the behavior has a distinct objective, boundary or terminal result.
3. Add a `static bool LCS_Test...()` function under **Test Scenarios**.
4. Give the function a Doxygen `@brief`, a behavioral `@details`, and a documented Boolean return.
5. Start from boot and use the shortest public event path that establishes the required precondition.
6. Reuse a fixture only when its documented `@pre` and `@post` conditions exactly match the new scenario.
7. Verify every externally significant returned action with `LCS_TEST_EXPECT_ACTION()`.
8. Prove silent internal effects with a later observable transition.
9. Add the stable snake-case name to `LCS_TestCases` in the C file.
10. Add the same name to `LCS_FSM_TEST_SCENARIOS` in `Tests/CMakeLists.txt`.
11. Add the `lcs.<name>` entry and its objective to the [scenario catalog](#7-lcs-scenario-catalog).
12. Update the coverage and expected test count in this README when applicable.
13. Reconfigure CMake, rebuild and run the complete suite, not only the new scenario.

The scenario name exists in both C and CMake intentionally: the executable owns dispatch, while CMake must register each isolated process at configuration time. A name present in only one registry is a maintenance error.

### 12.3 Scenario Implementation Template

```c
/**
 * @brief Validates <one externally meaningful behavior>.
 *
 * @details Describes the initial route, boundary or terminal condition and how
 *          later actions prove any private effect that cannot be observed directly.
 *
 * @return true when every expected action matches; otherwise false.
 */
static bool LCS_TestNewBehavior(void)
{
    /* Arrange: establish the required state through public events. */
    LCS_TEST_REQUIRE(LCS_TestActivateLocked());

    /* Act and assert: dispatch the changed event and require its public action. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_EXAMPLE, LCS_ACTION_EXAMPLE);

    /* Prove the target state or private cleanup through later behavior. */
    LCS_TEST_EXPECT_ACTION(LCS_EVENT_FOLLOW_UP, LCS_ACTION_EXPECTED_FOLLOW_UP);

    return true;
}
```

Then register the same stable name in the C table:

```c
{"new_behavior", LCS_TestNewBehavior}
```

and in the CMake list:

```cmake
set(LCS_FSM_TEST_SCENARIOS
    # Existing scenarios...
    new_behavior
)
```

The example event and action identifiers are placeholders; they shall not be copied into production unless they are part of the actual LCS contract.

### 12.4 Definition of Done

An LCS test change is complete only when:

* The scenario has one clear behavioral objective.
* Its function and any new fixture are fully documented.
* It starts from the deterministic boot condition and does not depend on scenario ordering.
* Every affected guard branch and boundary is exercised.
* Silent internal effects are proven through public follow-up behavior.
* Invalid or out-of-context handling is covered where the change introduces risk.
* The C registry, CMake registry and README catalog use the same scenario name.
* The documented scenario count matches CTest discovery.
* The host build passes with warnings treated as errors.
* The entire CTest suite passes with `--output-on-failure`.
* Production and LCS README documentation describe the same intentional behavior.

---

## 13. Continuous Integration

A CI job needs only the three reproducible commands:

```sh
cmake -S Tests -B build/host-tests-ci -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host-tests-ci --parallel
ctest --test-dir build/host-tests-ci --output-on-failure
```

The process exit status from the build and CTest commands is sufficient to fail the job. No hardware runner is required.

For larger future test collections, labels can split execution without changing scenario names:

```sh
ctest --test-dir build/host-tests-ci --label-regex fsm --output-on-failure
```

Do not reuse a firmware cross-compilation build directory for host tests. A dedicated CI binary directory also prevents cached toolchain settings from making results environment-dependent.

---

## 14. Limitations

The current suite:

* Validates only hardware-independent LCS behavior, not App Core action execution.
* Does not invoke Credential Entry, Authentication, Credential Register, Credential Storage, Timeout Validation or hardware-facing services.
* Does not validate real-time durations; it dispatches already interpreted timeout events.
* Does not inspect display, sound, LED or lock-actuator side effects.
* Does not perform concurrency or reentrancy testing because the LCS contract requires serialized calls.
* Does not collect structural code-coverage metrics.
* Does not fuzz the complete numeric event domain.
* Relies on a separate process for singleton reset.

These boundaries are intentional. Integration tests shall validate App Core mappings and action execution, while target tests shall validate timing and hardware interaction.

---

## 15. Acceptance Criteria

The native LCS suite is accepted when:

* A fresh native configuration succeeds without STM32 dependencies.
* The production LCS source and host runner compile as C11 with no warnings.
* CTest discovers all names present in the documented scenario catalog.
* Each scenario starts in an independent process.
* All current transition paths, guard boundaries and terminal cleanup policies remain covered.
* Invalid events demonstrably preserve state.
* All 14 current scenarios pass.
* Any future LCS contract change updates code, tests, scenario registry and documentation together.

---

## 16. License

This test suite is part of the:

```text
Electronic Lock Project
```

and follows the project's license terms.
