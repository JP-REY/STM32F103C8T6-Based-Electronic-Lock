/**********************************************************************************************************************************
 * @file    MatrixKeyboard_Driver.c
 * @brief   Matrix keyboard driver implementation.
 *
 * @details Implements the complete matrix keyboard processing pipeline,
 *          including matrix scanning, debounce filtering, key level
 *          tracking, state machine processing and generation of user
 *          actions.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Ago 07, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "MatrixKeyboard_Driver.h"
#include "Time_Platform_Interface.h"

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
static MK_ScanOpStatusTypeDef MK_Scan                       (MK_HandleTypeDef* Device);
static MK_OpStatusTypeDef     MK_FilterKeysDebounce         (MK_HandleTypeDef* Device);
static MK_OpStatusTypeDef     MK_UpdateKeysEvent            (MK_HandleTypeDef* Device);
static MK_OpStatusTypeDef     MK_ProcessKeysEvent           (MK_HandleTypeDef* Device);
static MK_OpStatusTypeDef     MK_Process                    (MK_HandleTypeDef* Device);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Checks whether the matrix keyboard driver has been initialized.
 *
 * @details Returns the initialization state stored in the driver instance.
 *          A NULL device pointer is treated as an uninitialized instance.
 *
 * @param   Device Pointer to the matrix keyboard instance.
 *
 * @return  true  The driver has been successfully initialized.
 * @return  false The driver is not initialized or the device pointer is NULL.
 **********************************************************************************************************************************/
static inline bool MK_IsInit(MK_HandleTypeDef* Device)
{
    return Device == NULL ? false : Device->_is_initialized;
}

/**********************************************************************************************************************************
 * @brief   Scans the keyboard matrix and updates the raw level of every key.
 *
 * @details Sequentially selects each matrix column through the configured
 *          scan interface and samples all keyboard rows.
 *
 *          The scan adapter returns a normalized row bitmask, where each
 *          set bit represents an active row regardless of the underlying
 *          electrical polarity. The resulting state of every key is stored
 *          as its raw level and becomes the input for the debounce stage.
 *
 * @param   Device - Pointer to the matrix keyboard instance.
 *
 * @note    This function performs only matrix scanning. It does not apply
 *          debounce filtering or generate key events.
 *
 * @return  MK_SCAN_OP_OK   - Matrix successfully scanned.
 * @return  MK_SCAN_OP_FAIL - Invalid device or scan operation failed.
 **********************************************************************************************************************************/
static MK_ScanOpStatusTypeDef MK_Scan(MK_HandleTypeDef *Device)
{
    if (Device == NULL || !MK_IsInit(Device))
    {
        return MK_SCAN_OP_FAIL;
    }

    const MK_RowsQtyTypeDef rows = Device->_config->_rows_number;
    const MK_ColsQtyTypeDef cols = Device->_config->_cols_number;

    for(MK_ColTypeDef col = 0; col < cols; col++)
    {
        uint32_t rows_mask = 0x00;

        if(Device->_scan_interface.SelectColumn(Device->_scan_interface.Context, col) != MK_SCAN_OP_OK)

            return MK_SCAN_OP_FAIL;

        if(Device->_scan_interface.ReadRows(Device->_scan_interface.Context, &rows_mask) != MK_SCAN_OP_OK)

            return MK_SCAN_OP_FAIL;

        for(MK_RowTypeDef row = 0; row < rows; row++)
        {
            const uint16_t key_index = row * cols + col;

            bool active = ((rows_mask >> row) & 1U) != 0U;

            Device->_keys[key_index]._debounce_ctx._key_raw_level = active ? MK_KEY_LEVEL_PRESSED : MK_KEY_LEVEL_RELEASED;
        }
    }

    return MK_SCAN_OP_OK;
}

/**********************************************************************************************************************************
 * @brief   Applies debounce filtering to all keys.
 *
 * @details Processes the raw level of every key and validates level
 *          transitions using the configured debounce interval.
 *
 *          Whenever a raw level change is detected, a debounce cycle is
 *          started. The new level is accepted only if it remains unchanged
 *          for the entire debounce period. Once validated, the key stable
 *          level is updated accordingly.
 *
 *          This function converts instantaneous key samples into stable
 *          logical levels suitable for event generation.
 *
 * @param   Device - Pointer to the matrix keyboard instance.
 *
 * @note    This function does not generate key events or user actions.
 *          It only updates the stable level of each key after the debounce
 *          interval has elapsed.
 *
 * @return  MK_OPERATION_OK   - Debounce processing completed successfully.
 * @return  MK_OPERATION_FAIL - Invalid device or processing failed.
 **********************************************************************************************************************************/
static MK_OpStatusTypeDef MK_FilterKeysDebounce(MK_HandleTypeDef* Device)
{
    if (Device == NULL || !MK_IsInit(Device))
    {
        return MK_OPERATION_FAIL;
    }

    const MK_RowsQtyTypeDef rows = Device->_config->_rows_number;
    const MK_ColsQtyTypeDef cols = Device->_config->_cols_number;

    const uint16_t total_keys = rows * cols;

    MK_KeyTypeDef* key_data = Device->_keys;

    for(uint16_t key = 0; key < total_keys; key++)
    {
        MK_KeyLevelTypeDef raw    = key_data[key]._debounce_ctx._key_raw_level;
        MK_KeyLevelTypeDef stable = key_data[key]._current_stable_level;

        MK_DebounceContextTypeDef* debounce = &Device->_keys[key]._debounce_ctx;

        if(!(debounce->_active))
        {
            if(raw != stable)
            {
                debounce->_active = true;

                debounce->_candidate_level = raw;

                key_data[key]._debounce_ctx._candidate_timestamp = Platform_GetMillis();
            }
        }

        else
        {
            if(raw == stable)
            {
                debounce->_active = false;
            }

            else if(raw != debounce->_candidate_level)
            {
                debounce->_candidate_level = raw;

                key_data[key]._debounce_ctx._candidate_timestamp = Platform_GetMillis();
            }

            else
            {
                uint32_t now = Platform_GetMillis();

                uint32_t elapsed = now - key_data[key]._debounce_ctx._candidate_timestamp;

                if(elapsed >= Device->_config->_debounce_time_ms)
                {
                    debounce->_active = false;

                    Device->_keys[key]._current_stable_level = debounce->_candidate_level;
                }
            }
        }
    }

    return MK_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Updates the output event of every key.
 *
 * @details Compares the current stable level of each key with its previous
 *          stable level to detect validated state transitions.
 *
 *          Whenever a stable level change is detected, the corresponding
 *          key event is generated and stored. Keys whose stable level
 *          remains unchanged produce no event.
 *
 *          This function converts stable key levels into discrete press
 *          and release events for subsequent state machine processing.
 *
 * @param   Device - Pointer to the matrix keyboard instance.
 *
 * @note    This function does not generate user actions. It only reports
 *          validated level transitions as key events.
 *
 * @return  MK_OPERATION_OK   - Event generation completed successfully.
 * @return  MK_OPERATION_FAIL - Invalid device or inconsistent key state.
 **********************************************************************************************************************************/
static MK_OpStatusTypeDef MK_UpdateKeysEvent(MK_HandleTypeDef* Device)
{
    if (Device == NULL || !MK_IsInit(Device))
    {
        return MK_OPERATION_FAIL;
    }

    const MK_RowsQtyTypeDef rows = Device->_config->_rows_number;
    const MK_ColsQtyTypeDef cols = Device->_config->_cols_number;

    const uint16_t total_keys = rows * cols;

    MK_KeyTypeDef* key_data = Device->_keys;

    for(uint16_t key = 0; key < total_keys; key++)
    {
        MK_KeyLevelTypeDef current  = key_data[key]._current_stable_level;
        MK_KeyLevelTypeDef previous = key_data[key]._previous_stable_level;

        if((current != MK_KEY_LEVEL_PRESSED && current != MK_KEY_LEVEL_RELEASED) ||
           (previous != MK_KEY_LEVEL_PRESSED && previous != MK_KEY_LEVEL_RELEASED))
        {
            return MK_OPERATION_FAIL;
        }

        MK_KeyStateEventTypeDef event = MK_KEY_EVENT_NONE;

        if(current != previous)
        {
            event = current == MK_KEY_LEVEL_PRESSED ?
                               MK_KEY_EVENT_PRESS   :
                               MK_KEY_EVENT_RELEASE ;

            key_data[key]._previous_stable_level = key_data[key]._current_stable_level;
        }

        key_data[key]._output_event = event;
      }

    return MK_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Processes the outputed event of every key.
 *
 * @details Evaluates the current state and outputed event of each key to
 *          execute the corresponding state transition and generate user
 *          actions.
 *
 *          Validated key events produced by the previous processing stage
 *          are consumed by the state machine and translated into logical
 *          actions, such as key clicks. Once processed, pending events are
 *          cleared to prevent repeated handling.
 *
 * @param   Device - Pointer to the matrix keyboard instance.
 *
 * @note    This function is responsible for updating the key state machine
 *          and generating output actions. It assumes that debounce
 *          filtering and event generation have already been completed.
 *
 * @return  MK_OPERATION_OK   - State machine processed successfully.
 * @return  MK_OPERATION_FAIL - Invalid device or inconsistent key state.
 **********************************************************************************************************************************/
static MK_OpStatusTypeDef MK_ProcessKeysEvent(MK_HandleTypeDef* Device)
{
    if (Device == NULL || !MK_IsInit(Device))
    {
        return MK_OPERATION_FAIL;
    }

    const MK_RowsQtyTypeDef rows = Device->_config->_rows_number;
    const MK_ColsQtyTypeDef cols = Device->_config->_cols_number;

    const uint16_t total_keys = rows * cols;

    for(uint16_t key = 0; key < total_keys; key++)
    {
        MK_KeyTypeDef* key_data = Device->_keys;

        MK_KeyStateTypeDef key_state = key_data[key]._state;

        MK_KeyStateEventTypeDef key_event = key_data[key]._output_event;

        key_data[key]._output_event = MK_KEY_EVENT_NONE;

        switch(key_state)
        {
            case MK_KEY_STATE_RELEASED:
            {
               if(key_event == MK_KEY_EVENT_PRESS)
               {
                   key_data[key]._state = MK_KEY_STATE_PRESSED;
               }
            }

            break;

            case MK_KEY_STATE_PRESSED:
            {
                if(key_event != MK_KEY_EVENT_RELEASE) break;

                key_data[key]._output_action = MK_KEY_ACTION_CLICK;

                key_data[key]._state = MK_KEY_STATE_RELEASED;
            }

            break;

            default:
            return MK_OPERATION_FAIL;
        }
    }

    return MK_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Executes the complete key processing pipeline.
 *
 * @details Performs all processing stages required to transform the raw
 *          key levels obtained during the matrix scan into user actions.
 *
 *          The processing pipeline is executed in the following order:
 *              - Debounce filtering.
 *              - Key event generation.
 *              - Key event processing.
 *
 *          If any processing stage fails, the pipeline is immediately
 *          aborted and the corresponding failure status is returned.
 *
 * @param   Device - Pointer to the matrix keyboard instance.
 *
 * @note    This function assumes that the keyboard matrix has already been
 *          scanned and the raw key levels have been updated.
 *
 * @return  MK_OPERATION_OK   - All processing stages completed successfully.
 * @return  MK_OPERATION_FAIL - Invalid device or a processing stage failed.
 **********************************************************************************************************************************/
static MK_OpStatusTypeDef MK_Process(MK_HandleTypeDef* Device)
{
    if (Device == NULL || !MK_IsInit(Device))
    {
        return MK_OPERATION_FAIL;
    }

    if(MK_FilterKeysDebounce(Device) != MK_OPERATION_OK)

        return MK_OPERATION_FAIL;

    if(MK_UpdateKeysEvent(Device) != MK_OPERATION_OK)

        return MK_OPERATION_FAIL;

    if(MK_ProcessKeysEvent(Device) != MK_OPERATION_OK)

        return MK_OPERATION_FAIL;

    return MK_OPERATION_OK;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Initializes a matrix keyboard driver instance.
 *
 * @details Associates the driver instance with the supplied configuration
 *          and key storage, initializes the internal state of every key
 *          and prepares the driver for operation.
 *
 *          All keys are initialized to the released state, debounce
 *          contexts are reset and any pending events or output actions
 *          are cleared to ensure a known initial condition.
 *
 *          If the driver instance has already been initialized, this
 *          function returns successfully without reinitializing it.
 *
 * @param   Device    - Pointer to the matrix keyboard instance.
 * @param   Config    - Pointer to the driver configuration.
 * @param   KeysTable - Pointer to the key map table managed by the driver.
 *
 * @note    The configuration and key table must remain valid for the
 *          entire lifetime of the driver instance.
 *
 * @return  MK_OPERATION_OK   - Driver successfully initialized.
 * @return  MK_OPERATION_FAIL - Invalid parameters or initialization failed.
 **********************************************************************************************************************************/
MK_OpStatusTypeDef MK_Init(MK_HandleTypeDef* Device, const MK_ConfigTypeDef* Config, MK_KeyTypeDef* KeysTable)
{
    if (Device == NULL || Config == NULL)
    {
        return MK_OPERATION_FAIL;
    }

    if(MK_IsInit(Device))
    {
        return MK_OPERATION_OK;
    }

    Device->_config = Config;

    Device->_keys = KeysTable;

    const MK_RowsQtyTypeDef rows = Device->_config->_rows_number;
    const MK_ColsQtyTypeDef cols = Device->_config->_cols_number;

    const uint16_t total_keys = rows * cols;

    MK_KeyTypeDef* key_data = Device->_keys;

    for(uint16_t key = 0; key < total_keys; key++)
    {
        key_data[key]._state                             = MK_KEY_STATE_RELEASED;
        key_data[key]._debounce_ctx._key_raw_level       = MK_KEY_LEVEL_RELEASED;
        key_data[key]._debounce_ctx._candidate_level     = MK_KEY_LEVEL_RELEASED;
        key_data[key]._output_event                      = MK_KEY_EVENT_NONE;
        key_data[key]._current_stable_level              = MK_KEY_LEVEL_RELEASED;
        key_data[key]._previous_stable_level             = MK_KEY_LEVEL_RELEASED;
        key_data[key]._output_action                     = MK_KEY_ACTION_NONE;
        key_data[key]._debounce_ctx._active              = false;
        key_data[key]._debounce_ctx._candidate_timestamp = 0;
    }

    Device->_is_initialized = true;

    return MK_OPERATION_OK;
}

/**********************************************************************************************************************************
 * @brief   Reads the current state of the matrix keyboard.
 *
 * @details Executes a complete keyboard acquisition cycle by scanning the
 *          matrix, processing the sampled key states and retrieving the
 *          next pending user action, if available.
 *
 *          If a key action is available, the corresponding key identifier
 *          and action type are written to the output structure. Once
 *          reported, the action is cleared to prevent it from being
 *          returned again in subsequent calls.
 *
 * @param   Device - Pointer to the matrix keyboard instance.
 * @param   Output - Pointer to the structure that receives the detected key
 *                   and associated action.
 *
 * @note    This function should be called periodically to ensure proper
 *          keyboard scanning and event processing.
 *
 * @note    If multiple key actions are pending, only the first pending
 *          action found during the scan order is returned. Remaining
 *          pending actions are preserved for subsequent calls.
 *
 * @return  MK_OPERATION_OK   - Keyboard successfully processed.
 * @return  MK_OPERATION_FAIL - Invalid parameters or an internal processing
 *                              stage failed.
 **********************************************************************************************************************************/
MK_OpStatusTypeDef MK_Read(MK_HandleTypeDef* Device, MK_OutputTypeDef* Output)
{
    if(Device == NULL || Output == NULL || !MK_IsInit(Device))
    {
        return MK_OPERATION_FAIL;
    }

    if(MK_Scan(Device) != MK_SCAN_OP_OK)
    {
        return MK_OPERATION_FAIL;
    }

    if(MK_Process(Device) != MK_OPERATION_OK)
    {
        return MK_OPERATION_FAIL;
    }

    const MK_RowsQtyTypeDef rows = Device->_config->_rows_number;
    const MK_ColsQtyTypeDef cols = Device->_config->_cols_number;

    const uint16_t total_keys = rows * cols;

    MK_KeyTypeDef* key_data = Device->_keys;

    for(uint16_t key = 0; key < total_keys; key++)
    {
        if(Device->_keys[key]._output_action != MK_KEY_ACTION_NONE)
        {
            Output->OutputKey = Device->_config->_key_map[key];

            Output->OutputAction = key_data[key]._output_action;

            key_data[key]._output_action = MK_KEY_ACTION_NONE;

            break;
        }
    }

    return MK_OPERATION_OK;
}


























