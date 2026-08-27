cmake_minimum_required(VERSION 3.22)

# Standalone native test project. It is deliberately not included by the
# firmware CMake project because the firmware presets select arm-none-eabi,
# while this suite shall execute directly on the development host.
project(ElectronicLockHostTests LANGUAGES C)

# Accidentally configuring with the embedded toolchain can produce a binary
# that builds successfully but cannot be executed by CTest on the host.
if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "Tests must be configured with a host compiler. "
        "Do not use the STM32 ARM toolchain or the root firmware presets."
    )
endif()

enable_testing()

# Resolve production paths from this file instead of depending on the shell's
# current working directory. This keeps configuration reproducible from the
# repository root, the Tests directory, an IDE or a CI runner.
get_filename_component(ELECTRONIC_LOCK_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Compile the production implementation as the system under test. No copied
# implementation, mock FSM or test-only preprocessor path is used.
add_library(lcs_under_test STATIC
    "${ELECTRONIC_LOCK_ROOT}/Libs/Services/Lock_Control/Src/Lock_Control_Service.c"
)

target_include_directories(lcs_under_test PUBLIC
    "${ELECTRONIC_LOCK_ROOT}/Libs/Services/Lock_Control/Inc"
)

target_compile_features(lcs_under_test PUBLIC c_std_11)

# The executable accepts exactly one scenario name. CTest starts a new process
# for every name so each process receives a freshly initialized LCS singleton.
add_executable(lcs_fsm_host_test
    Lock_Control/Lock_Control_Service_Test.c
)

target_link_libraries(lcs_fsm_host_test PRIVATE
    lcs_under_test
)

# Hold production and test sources to the strict warning level used for native
# validation. Compiler-family branches preserve equivalent behavior on MSVC
# and on GCC/Clang-style command lines.
if(MSVC)
    target_compile_options(lcs_under_test PRIVATE /W4 /WX)
    target_compile_options(lcs_fsm_host_test PRIVATE /W4 /WX)
else()
    target_compile_options(lcs_under_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(lcs_fsm_host_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Scenario names shall match the LCS_TestCases registry in
# Lock_Control/Lock_Control_Service_Test.c and the catalog in README.md.
# When the FSM changes, add the new scenario in all three locations.
set(LCS_FSM_TEST_SCENARIOS
    inactive_gate
    boot_failure
    normal_access
    normal_exit_paths
    authentication_lockout
    authentication_success_resets_counter
    registration_authorization_failure
    first_boot_registration_success
    authorized_registration_success
    registration_mismatch_limit
    registration_first_entry_exit_paths
    registration_confirm_entry_exit_paths
    registration_storage_failure
    invalid_events_preserve_state
    exit_request_access                  
    exit_request_preserves_failure_counter
    relock_not_confirmed_recovery
    unlock_request_failure_recovery
    unlock_recovery_critical_fault
)

# Register each scenario as an independently selectable CTest test. Labels
# support suite-level filters, and the short timeout detects accidental hangs
# even though LCS_Process() is designed to be synchronous and bounded.
foreach(SCENARIO IN LISTS LCS_FSM_TEST_SCENARIOS)
    add_test(
        NAME "lcs.${SCENARIO}"
        COMMAND lcs_fsm_host_test "${SCENARIO}"
    )

    set_tests_properties("lcs.${SCENARIO}" PROPERTIES
        LABELS "host;lcs;fsm"
        TIMEOUT 5
    )
endforeach()
