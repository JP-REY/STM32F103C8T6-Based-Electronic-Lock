/**********************************************************************************************************************************
 * @file    ExitButton_Driver.c
 * @brief   Interrupt-oriented, non-blocking exit-button driver implementation.
 *
 * @details Separates the interrupt-facing edge notification from application-context GPIO sampling and debounce evaluation. The
 *          driver reports press and release transitions but owns no request-to-exit policy or actuator command.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    2026-08-24
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "ExitButton_Driver.h"
#include "stddef.h"

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
static bool                  ExitButton_IsInitialized  (const ExitButton_Handle_t* Device);
static ExitButton_OpStatus_t ExitButton_NormalizeLevel (const ExitButton_Handle_t* Device, GPIO_Level_t Level, bool* IsPressed);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Checks whether an exit button driver instance is initialized.
 *
 * @param   Device - Pointer to the exit button handle.
 *
 * @return  true  - Whether Device is non-NULL and initialized.
 * @return  false - Whether Device is NULL or not initialized.
 */
static bool ExitButton_IsInitialized(const ExitButton_Handle_t* Device)
{
    return Device == NULL ? false : Device->_initialized;
}

/**
 * @brief   Converts one electrical GPIO level into a logical button state.
 *
 * @param   Device - Pointer to an initialized exit button handle.
 * @param   Level     - Electrical GPIO level to normalize.
 * @param   IsPressed - Pointer receiving the normalized logical level.
 *
 * @return  EXIT_BUTTON_OPERATION_OK   - Whether Level was normalized.
 * @return  EXIT_BUTTON_OPERATION_FAIL - Whether Level or IsPressed is invalid.
 */
static ExitButton_OpStatus_t ExitButton_NormalizeLevel(const ExitButton_Handle_t* Device, GPIO_Level_t Level, bool* IsPressed)
{
    if(IsPressed == NULL || (Level != GPIO_LEVEL_LOW && Level != GPIO_LEVEL_HIGH))
    {
        return EXIT_BUTTON_OPERATION_FAIL;
    }

    if(Device->_active_level == EXIT_BUTTON_ACTIVE_LEVEL_LOW)
    {
        *IsPressed = Level == GPIO_LEVEL_LOW;
    }
    else
    {
        *IsPressed = Level == GPIO_LEVEL_HIGH;
    }

    return EXIT_BUTTON_OPERATION_OK;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes an interrupt-oriented exit button instance.
 *
 * @details Retains the supplied GPIO Platform handle, active electrical level
 *          and debounce interval. Runtime state and interrupt sequencing are
 *          reset without reading the GPIO or generating an event.
 *
 * @param   Device         - Pointer to the exit button handle to initialize.
 * @param   Context        - Pointer to the caller-owned GPIO Platform handle.
 * @param   Level          - Electrical level representing a pressed button.
 * @param   DebounceTimeMs - Required input stability interval in milliseconds.
 *
 * @return  EXIT_BUTTON_OPERATION_OK   - Whether initialization succeeds.
 * @return  EXIT_BUTTON_OPERATION_FAIL - Whether a parameter is invalid.
 */
ExitButton_OpStatus_t ExitButton_Init(ExitButton_Handle_t* Device, GPIO_Handle_t* Context, ExitButton_ActiveLevel_t Level, uint32_t DebounceTimeMs)
{
    if(Device == NULL || Context == NULL || DebounceTimeMs == 0U ||
      (Level != EXIT_BUTTON_ACTIVE_LEVEL_LOW &&
       Level != EXIT_BUTTON_ACTIVE_LEVEL_HIGH))
    {
        return EXIT_BUTTON_OPERATION_FAIL;
    }

    Device->_gpio                = Context;
    Device->_active_level        = Level;
    Device->_is_pressed          = false;
    Device->_state_is_known      = false;
    Device->_debounce_time_ms    = DebounceTimeMs;
    Device->_interrupt_sequence  = 0U;
    Device->_interrupt_time_ms   = 0U;
    Device->_processed_sequence  = 0U;
    Device->_initialized         = true;

    return EXIT_BUTTON_OPERATION_OK;
}

/**
 * @brief   Records an interrupt edge and restarts the debounce interval.
 *
 * @details Timestamp is written before the sequence increment. The application
 *          processor observes the sequence as the publication marker and
 *          verifies it again around GPIO sampling to avoid consuming an edge
 *          while another interrupt notification is arriving.
 *
 * @param   Device      - Pointer to an initialized exit button handle.
 * @param   TimestampMs - Millisecond timestamp associated with the edge.
 *
 * @return  void
 */
void ExitButton_NotifyInterrupt(ExitButton_Handle_t* Device, uint32_t TimestampMs)
{
    if(!ExitButton_IsInitialized(Device))
    {
        return;
    }

    Device->_interrupt_time_ms = TimestampMs;
    Device->_interrupt_sequence++;
}

/**
 * @brief   Processes pending interrupt activity and validates a stable state.
 *
 * @details The newest interrupt sequence is processed only after the configured
 *          interval has elapsed since its timestamp. If a new interrupt arrives
 *          during processing, the current sample is discarded and the debounce
 *          interval restarts from the newer edge.
 *
 * @param   Device        - Pointer to an initialized exit button handle.
 * @param   CurrentTimeMs - Current monotonic millisecond timestamp.
 * @param   Event         - Pointer receiving the validated transition event.
 *
 * @return  EXIT_BUTTON_OPERATION_OK   - Whether processing completes normally.
 * @return  EXIT_BUTTON_OPERATION_FAIL - Whether validation or GPIO reading fails.
 */
ExitButton_OpStatus_t ExitButton_Update(ExitButton_Handle_t* Device, uint32_t CurrentTimeMs, ExitButton_Event_t* Event)
{
    if(Event == NULL)
    {
        return EXIT_BUTTON_OPERATION_FAIL;
    }

    *Event = EXIT_BUTTON_EVENT_NONE;

    if(!ExitButton_IsInitialized(Device))
    {
        return EXIT_BUTTON_OPERATION_FAIL;
    }

    uint32_t interrupt_sequence = Device->_interrupt_sequence;

    if(interrupt_sequence == Device->_processed_sequence)
    {
        return EXIT_BUTTON_OPERATION_OK;
    }

    uint32_t interrupt_time_ms = Device->_interrupt_time_ms;

    if(interrupt_sequence != Device->_interrupt_sequence)
    {
        return EXIT_BUTTON_OPERATION_OK;
    }

    if((uint32_t)(CurrentTimeMs - interrupt_time_ms) < Device->_debounce_time_ms)
    {
        return EXIT_BUTTON_OPERATION_OK;
    }

    GPIO_Level_t raw_level  = PGPIO_GetLevel(Device->_gpio);
    bool         is_pressed = false;

    if(ExitButton_NormalizeLevel(Device, raw_level, &is_pressed) != EXIT_BUTTON_OPERATION_OK)
    {
        return EXIT_BUTTON_OPERATION_FAIL;
    }

    if(interrupt_sequence != Device->_interrupt_sequence)
    {
        return EXIT_BUTTON_OPERATION_OK;
    }

    Device->_processed_sequence = interrupt_sequence;

    if(Device->_state_is_known && is_pressed == Device->_is_pressed)
    {
        return EXIT_BUTTON_OPERATION_OK;
    }

    Device->_is_pressed     = is_pressed;
    Device->_state_is_known = true;
    
    *Event = is_pressed ? EXIT_BUTTON_EVENT_PRESS : EXIT_BUTTON_EVENT_RELEASE;

    return EXIT_BUTTON_OPERATION_OK;
}
