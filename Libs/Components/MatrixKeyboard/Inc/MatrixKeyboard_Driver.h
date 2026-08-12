/**********************************************************************************************************************************
 * @file    MatrixMK_Driver.h
 * @brief   Generic matrix keyboard driver public interface.
 *
 * @details This module provides a generic polling-based matrix keyboard driver capable
 *          of scanning matrix keyboards with an arbitrary number of rows and columns.
 *
 *          The driver performs:
 *           - Matrix scanning through an abstract scan interface.
 *           - Software debouncing.
 *           - Key state management.
 *           - Generation of high-level key actions.
 *
 *          Hardware access is fully abstracted by the scan interface, allowing the
 *          driver to be reused with different GPIO implementations or I/O expanders.
 *
 *          Current version supports:
 *           - Single click detection.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Ago 07, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_COMPONENTS_MATRIXKEYBOARD_INC_MATRIXKEYBOARD_DRIVER_H_
#define LIBS_COMPONENTS_MATRIXKEYBOARD_INC_MATRIXKEYBOARD_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "MatrixKeyboard_ScanInterface.h"
#include "stm32f4xx.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Matrix keyboard semantic type aliases.
 *
 * @details These aliases do not introduce new data types. They are simple semantic
 *          abstractions built on top of fixed-width integer types to improve API
 *          readability and clearly express the intended purpose of each value.
 *
 *          Using dedicated aliases instead of raw integer types makes function
 *          prototypes and data structures easier to understand while preserving
 *          zero runtime overhead.
 *
 *          The underlying representation may be changed in future versions without
 *          affecting the public API semantics.
 **********************************************************************************************************************************/
typedef uint8_t MK_KeyCodeTypeDef;
typedef uint8_t MK_RowsQtyTypeDef;
typedef uint8_t MK_ColsQtyTypeDef;
typedef uint8_t MK_RowTypeDef;
typedef uint8_t MK_ColTypeDef;

/**********************************************************************************************************************************
 * @brief   Matrix keyboard operation status.
 *
 * @details Represents the execution status returned by the public Matrix Keyboard
 *          API functions.
 *
 *          These values indicate only whether the requested operation was
 *          successfully executed. They do not provide any information regarding
 *          key states, events or user actions.
 **********************************************************************************************************************************/
typedef enum
{
    MK_OPERATION_OK,    /* << Operation completed successfully. >> */
    MK_OPERATION_FAIL   /* << Operation could not be completed. >> */

}MK_OpStatusTypeDef;

/**********************************************************************************************************************************
 * @brief   Stable logical level of a keyboard key.
 *
 * @details Represents the debounced logical level associated with a key.
 *
 *          This abstraction is independent of the electrical implementation
 *          (active-low or active-high) and describes only the interpreted
 *          physical state of the key.
 **********************************************************************************************************************************/
typedef enum
{
    MK_KEY_LEVEL_PRESSED,
    MK_KEY_LEVEL_RELEASED,
    MK_KEY_LEVEL_UNKNOWN

}MK_KeyLevelTypeDef;

/**********************************************************************************************************************************
 * @brief   Electrical active level used by the keyboard hardware.
 *
 * @details Defines which electrical level corresponds to a physically pressed key.
 *
 *          This information is used only by the scan adapter to correctly interpret the
 *          hardware signals. The driver itself always operates on normalized logical key
 *          levels and therefore remains independent of whether the hardware is implemented
 *          as active-low or active-high.
 *
 *          Typical examples:
 *
 *          - MK_KEY_ACTIVE_LOW
 *              Pressed   -> Logic 0
 *              Released  -> Logic 1
 *
 *          - MK_KEY_ACTIVE_HIGH
 *              Pressed   -> Logic 1
 *              Released  -> Logic 0
 **********************************************************************************************************************************/
typedef enum
{
    MK_KEY_ACTIVE_LOW = 0U,
    MK_KEY_ACTIVE_HIGH

}MK_KeyActiveLevelTypeDef;

/**********************************************************************************************************************************
 * @brief   Logical user action generated by the keyboard driver.
 *
 * @details Represents high-level actions recognized after the complete processing of the
 *          key state machine.
 *
 *          These actions are generated only after debounce validation and any additional
 *          timing requirements defined by the driver.
 *
 *          This abstraction intentionally hides the underlying electrical transitions,
 *          allowing the application to react only to meaningful user interactions.
 **********************************************************************************************************************************/
typedef enum
{
    MK_KEY_ACTION_NONE,
    MK_KEY_ACTION_CLICK

}MK_KeyActionTypeDef;

/**********************************************************************************************************************************
 * @brief   Stable logical state of a key.
 *
 * @details Represents the debounced state currently assigned to a key.
 *
 *          The state is updated only after the debounce algorithm confirms that the input
 *          remained stable for the configured debounce interval.
 *
 *          This enumeration shall not be confused with the instantaneous electrical level
 *          sampled from the hardware.
 **********************************************************************************************************************************/
typedef enum
{
    MK_KEY_STATE_UNKNOWN,
    MK_KEY_STATE_RELEASED,
    MK_KEY_STATE_PRESSED

}MK_KeyStateTypeDef;

/**********************************************************************************************************************************
 * @brief   State transition event generated from consecutive stable key states.
 *
 * @details Represents the logical transition observed after comparing the current stable
 *          key state with the previous stable state.
 *
 *          Events are consumed internally by the driver state machine and are also used to
 *          generate higher-level user actions such as clicks or long presses.
 *
 *          Only debounced transitions may generate these events.
 **********************************************************************************************************************************/
typedef enum
{
    MK_KEY_EVENT_PRESS,      /* << Transition from released to pressed. >> */
    MK_KEY_EVENT_RELEASE,    /* << Transition from pressed to released. >> */
    MK_KEY_EVENT_NONE,       /* << No state transition detected.        >> */
    MK_KEY_EVENT_UNKNOWN     /* << Undefined transition state.          >> */

}MK_KeyStateEventTypeDef;

/**********************************************************************************************************************************
 * @brief   Debounce processing context of a single key.
 *
 * @details Stores all temporary information required by the debounce algorithm while
 *          validating a candidate key transition.
 *
 *          This structure is used internally by the driver and should never be modified
 *          directly by the application.
 *
 *          Once the candidate level remains stable for the configured debounce interval,
 *          the corresponding stable key state is updated.
 *
 * @warning This structure contains private driver data.Its contents are intended exclusively
 *          for internal use by the Matrix Keyboard driver and shall never be accessed or modified
 *          directly by the application.
 **********************************************************************************************************************************/
typedef struct
{
    /* << Private data. Do not read or modify! >> */
    /* << Indicates whether a debounce sequence is currently active.      >> */ bool               _active;
    /* << Most recent raw electrical level sampled from the hardware.     >> */ MK_KeyLevelTypeDef _key_raw_level;
    /* << Candidate level currently being validated.                      >> */ MK_KeyLevelTypeDef _candidate_level;
    /* << Timestamp captured when the candidate level was first detected. >> */ uint32_t           _candidate_timestamp;


}MK_DebounceContextTypeDef;

/**********************************************************************************************************************************
 * @brief   Runtime information associated with a single keyboard key.
 *
 * @details Stores every internal state required to process one physical key during the
 *          scanning cycle.
 *
 *          Each key owns an independent state machine, debounce context and output
 *          information, allowing all keys to be processed independently.
 *
 *          This structure is intended for exclusive use by the keyboard driver.
 *
 * @warning This structure contains private driver data.Its contents are intended exclusively
 *          for internal use by the Matrix Keyboard driver and shall never be accessed or modified
 *          directly by the application.
 **********************************************************************************************************************************/
typedef struct
{

    /* << Private data. Do not read or modify!                  >> */
    /* << Current stable logical level of the key.              >> */ MK_KeyLevelTypeDef        _current_stable_level;
    /* << Previously validated stable logical level.            >> */ MK_KeyLevelTypeDef        _previous_stable_level;
    /* << Current state of the key state machine.               >> */ MK_KeyStateTypeDef        _state;
    /* << Debounce processing context.                          >> */ MK_DebounceContextTypeDef _debounce_ctx;
    /* << Most recent logical event generated for this key.     >> */ MK_KeyStateEventTypeDef   _output_event;
    /* << Most recent high-level action generated for this key. >> */ MK_KeyActionTypeDef       _output_action;

}MK_KeyTypeDef;

/**********************************************************************************************************************************
 * @brief   Matrix keyboard driver configuration.
 *
 * @details Groups all static configuration parameters required to initialize one
 *          keyboard driver instance.
 *
 *          The contents of this structure are provided by the application during
 *          initialization and shall remain valid for the entire lifetime of the driver.
 *
 *          After a successful initialization, the configuration is treated as read-only
 *          by both the application and the driver.]
 *
 * @warning This structure contains private driver data.Its contents are intended exclusively
 *          for internal use by the Matrix Keyboard driver and shall never be accessed or modified
 *          directly by the application.
 **********************************************************************************************************************************/
typedef struct
{
    /* << Private data. Do not read or modify!                    >> */
    /* << Total number of matrix rows.                            >> */ MK_RowsQtyTypeDef        _rows_number;
    /* << Total number of matrix cols.                            >> */ MK_ColsQtyTypeDef        _cols_number;
    /* << Pointer to the application key map.                     >> */ const MK_KeyCodeTypeDef* _key_map;
    /* << Electrical active level corresponding to a pressed key. >> */ MK_KeyActiveLevelTypeDef _row_active_level;
    /* << Debounce interval, expressed in milliseconds.           >> */ uint32_t                 _debounce_time_ms;

} MK_ConfigTypeDef;

/**********************************************************************************************************************************
 * @brief   Matrix keyboard driver instance.
 *
 * @details Represents a complete runtime instance of the Matrix Keyboard driver.
 *
 *          This structure owns the runtime state required to scan the keyboard,
 *          process key state machines and generate user actions.
 *
 *          Multiple independent keyboard instances may coexist by allocating one
 *          handle per keyboard.
 *
 * @warning This structure contains private driver runtime data.
 *
 *          Except for passing its address to the public API functions, the application
 *          shall neither access nor modify any of its members directly.
 **********************************************************************************************************************************/
typedef struct
{
    /* << Private data. Do not read or modify!                                     >> */
    /* << Pointer to the driver configuration.                                     >> */ const MK_ConfigTypeDef* _config;
    /* << Pointer to the runtime key table. One entry per physical key.            >> */ MK_KeyTypeDef*          _keys;
    /* << Hardware scan interface implementation associated with this instance.    >> */ MK_ScanInterfaceTypeDef _scan_interface;
    /* << Indicates whether the driver instance has been successfully initialized. >> */ bool                    _is_initialized;

} MK_HandleTypeDef;

/**********************************************************************************************************************************
 * @brief   Matrix keyboard driver output.
 *
 * @details Represents the result produced by the keyboard driver after processing one
 *          scan cycle.
 *
 *          When a valid user action is detected, this structure contains both the logical
 *          key identifier associated with the event and the corresponding action
 *          generated by the driver.
 *
 *          If no new action is available, OutputAction is set to
 *          MK_KEY_ACTION_NONE and the value of OutputKey is unspecified.
 **********************************************************************************************************************************/
typedef struct
{
    /* << Logical identifier of the key associated with the generated action. >> */ MK_KeyCodeTypeDef   OutputKey;
    /* << High-level action generated for the corresponding key.              >> */ MK_KeyActionTypeDef OutputAction;

}MK_OutputTypeDef;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
MK_OpStatusTypeDef MK_Init(MK_HandleTypeDef* Device, const MK_ConfigTypeDef* Config, MK_KeyTypeDef* KeysTable);
MK_OpStatusTypeDef MK_Read(MK_HandleTypeDef *Device, MK_OutputTypeDef *Output);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_MATRIXKEYBOARD_INC_MATRIXKEYBOARD_DRIVER_H_ */
