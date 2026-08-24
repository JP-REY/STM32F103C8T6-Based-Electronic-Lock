/**********************************************************************************************************************************
 * @file    App_Core.c
 * @brief   Electronic-lock application initialization and event-orchestration core.
 *
 * @details Initializes the electronic-lock object graph supplied by App Config, binds it to CubeMX-generated peripheral resources
 *          and orchestrates keyboard input, application timeouts and bounded Lock Control event/action chains. App Config owns the
 *          static runtime storage, while App Executor owns the concrete side effects selected by the Lock Control state machine.
 *
 *          Initialization is fail-fast: each private initializer validates every stage and immediately reports failure to
 *          App_Init(). Stateless services and services that own an internal singleton runtime do not require an additional
 *          application-side handle.
 *
 *          App_ReadInput() acquires and translates physical input, while App_Dispatch() evaluates application-owned timeouts and
 *          advances presentation services. Both paths may synchronously execute semantic Lock Control actions without exposing
 *          concrete hardware dependencies through App_Core.h.
 *
 * @note    The timeout runtime and registry pointer in this translation unit have static storage duration. Concrete component and
 *          service instances live in App_Config.c and are borrowed through App_Instance.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    Aug 22, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "App_Core.h"
#include "App_Core_Internal.h"
#include "App_Config.h"
#include "App_Executor.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**
 * @brief   Data-driven timeout policy used by the application dispatcher.
 *
 * @details Uses App_TimeoutId_t values as designated indexes. Adding another mutually exclusive timed FSM state requires one new
 *          identifier and one definition record without changing App_PollTimeout().
 *
 * @note    Every event in this table describes an already elapsed interval and is consumed only by LCS_Process().
 */
static const App_TimeoutDefinition_t App_TimeoutDefinitions[APP_TIMEOUT_COUNT] =
{
    [APP_TIMEOUT_CREDENTIAL_ENTRY] =
    {
        .DurationMs   = APP_CREDENTIAL_ENTRY_TIMEOUT_MS,
        .ElapsedEvent = LCS_EVENT_ENTRY_TIMEOUT
    },

    [APP_TIMEOUT_UNLOCK] =
    {
        .DurationMs   = APP_UNLOCK_TIMEOUT_MS,
        .ElapsedEvent = LCS_EVENT_UNLOCK_TIMEOUT
    },

    [APP_TIMEOUT_ACCESS_DENIED] =
    {
        .DurationMs   = APP_ACCESS_DENIED_TIMEOUT_MS,
        .ElapsedEvent = LCS_EVENT_DENIED_ACCESS_TIMEOUT
    },

    [APP_TIMEOUT_LOCKOUT] =
    {
        .DurationMs   = APP_LOCKOUT_TIMEOUT_MS,
        .ElapsedEvent = LCS_EVENT_LOCKOUT_TIMEOUT
    },

    [APP_TIMEOUT_CRS_SAVED] =
    {
        .DurationMs   = APP_CRS_SAVED_TIMEOUT_MS,
        .ElapsedEvent = LCS_EVENT_CREDENTIAL_REGISTER_DONE
    }
};

/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/**
 * @brief   Single active application-timeout runtime.
 *
 * @details Owns only timeout lifecycle data. Elapsed-time arithmetic remains delegated to the stateless Timeout Validation Service,
 *          and semantic timeout events remain consumed by the Lock Control Service.
 *
 * @note    Serialized action chains initiated by App_ReadInput() or App_Dispatch() are the sole runtime readers and writers.
 */
static App_TimeoutRuntime_t App_ActiveTimeout =
{
    .Id          = APP_TIMEOUT_NONE,
    .StartedAtMs = 0U,
    .Active      = false
};

/**
 * @brief   Bound view of the runtime-object registry owned by App Config.
 *
 * @details App_Init() assigns the immutable registry address before invoking any dependency initializer. App Core and App Executor
 *          use the pointer to operate on the same statically allocated handles and credential buffers.
 *
 * @note    Remains NULL until App_Init() begins. Public runtime entry points require successful initialization before use.
 */
const App_RuntimeInstances_t* App_Instance = NULL;

/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
/*---------------------------------------------------------------------------------------------------------------------------------
 Application Dependency Initialization
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Initializes the LCD hardware, adapters, backlight and Display Render Service path. */
static bool App_InitLcd(void);

/** @brief Initializes the matrix-keyboard GPIO, scan adapter and driver path. */
static bool App_InitKeyboard(void);

/** @brief Initializes the PWM, Buzzer Driver and Sound Generator Service path. */
static bool App_InitBuzzer(void);

/** @brief Initializes the lock-status LED and its Status Indication Service instance. */
static bool App_InitLockStatusIndication(void);

/** @brief Initializes the low-battery LED and its Status Indication Service instance. */
static bool App_InitLowBatteryStatusIndication(void);

/** @brief Initializes the temporary GPIO-based lock actuator and immediately forces its safe state. */
static bool App_InitLockActuator(void);

/*---------------------------------------------------------------------------------------------------------------------------------
 Keyboard Acquisition and Translation
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Acquires one non-blocking output from the Matrix Keyboard Driver. */
static MK_OpStatus_t App_ReadKeyboard(MK_Output_t* Output);

/** @brief Translates a physical keyboard output into a semantic Credential Entry Service input. */
static CES_Input_t App_TranslateKeyToInputKind(const MK_Output_t* KeyboardOutput);

/*---------------------------------------------------------------------------------------------------------------------------------
 Credential Entry
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Processes one semantic input through the Credential Entry Service. */
static CES_Event_t App_ProcessCredentialInput(const CES_Input_t* Input);

/** @brief Polls the active timeout and returns its semantic LCS event exactly once after expiration. */
static LCS_Event_t App_PollTimeout(void);

/*---------------------------------------------------------------------------------------------------------------------------------
 Lock Control Event Orchestration
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Dispatches one LCS event and its bounded synchronous action/event follow-up chain. */
static void App_DispatchLcsEvent(LCS_Event_t Event);

/*---------------------------------------------------------------------------------------------------------------------------------
 Input and Credential Event Handling
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Applies wake-key, registration-command and CES-routing policy to one keyboard event. */
static void App_HandleKeyboardEvent(const MK_Output_t* Event);

/** @brief Maps a CES outcome into presentation, timeout management or a semantic LCS event. */
static void App_HandleCredentialEvent(CES_Event_t Event);

/** @brief Safely abandons an untrustworthy keyboard-input sequence and requests error feedback. */
static void App_HandleInputFault(void);

/*---------------------------------------------------------------------------------------------------------------------------------
 Periodic Service Coordination
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Polls application timing and advances display, indication and sound services once. */
static void App_UpdateServices(void);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes the LCD hardware path and Display Render Service.
 *
 * @details Initializes the PCF8574 over the CubeMX I2C context, binds it to the LCD bus interface, creates and initializes the
 *          backlight PWM, configures its frequency, binds the backlight adapter, initializes the LCD and applies the initial
 *          brightness. Finally, injects the LCD into the Display Render Service singleton.
 *
 * @note    Initialization is fail-fast and does not roll back stages that completed before a later failure.
 *
 * @return  true  - When every LCD, adapter, Platform and display-service stage succeeds.
 * @return  false - When any required initialization or configuration operation fails.
 */
static bool App_InitLcd(void)
{
    if(PCF8574_Init(App_Instance->Lcd_IoExpander,
                    APP_LCD_IO_EXPANDER_ADDRESS,
                    APP_LCD_IO_EXPANDER_I2C_CONTEXT)
                    != PCF8574_OPERATION_OK)
    {
        return false;
    }

    if(LCD_PCF8574_BusAdapterInit(&(App_Instance->Lcd->_bus),
                                    App_Instance->Lcd_IoExpander)
                                    != LCD_BUS_OPERATION_OK)
    {
        return false;
    }

    if(PPWM_Create(App_Instance->Lcd_BacklightPwm,
                   APP_LCD_BACKLIGHT_PWM_CONTEXT,
                   APP_LCD_BACKLIGHT_PWM_CHANNEL)
                   != PWM_OPERATION_OK)
    {
        return false;
    }

    if(PPWM_Init(App_Instance->Lcd_BacklightPwm)
                 != PWM_OPERATION_OK)
    {
        return false;
    }

    if(PPWM_SetFrequency(App_Instance->Lcd_BacklightPwm,
                         APP_LCD_BACKLIGHT_PWM_FREQUENCY)
                         != PWM_OPERATION_OK)
    {
        return false;
    }

    if(LCD_PWM_BacklightAdapterInit(&(App_Instance->Lcd->_backlight),
                                      App_Instance->Lcd_BacklightPwm)
                                      != LCD_BACKLIGHT_OP_OK)
    {
        return false;
    }

    if(LCD_Init(App_Instance->Lcd)
                != LCD_OPERATION_OK)
    {
        return false;
    }

    if(LCD_SetBrightness(App_Instance->Lcd,
                         APP_LCD_BACKLIGHT_BRIGHTNESS)
                         != LCD_OPERATION_OK)
    {
        return false;
    }

    if(DRS_Init(App_Instance->Lcd) != DRS_OPERATION_OK)
    {
        return false;
    }

    return true;
}

/**
 * @brief   Initializes the complete matrix-keyboard acquisition path.
 *
 * @details Creates Platform GPIO descriptors for all four rows and columns, initializes the Matrix Keyboard Driver with its
 *          immutable key map and per-key runtime table, and binds the GPIO scan adapter to the interface embedded in the keyboard
 *          handle.
 *
 * @note    GPIO pin macros provide zero-based pin numbers because PGPIO_Init() converts them to STM32 GPIO bit masks.
 * @note    Initialization is fail-fast and does not roll back descriptors initialized before a later failure.
 *
 * @return  true  - When every GPIO, keyboard-driver and scan-adapter stage succeeds.
 * @return  false - When any required initialization operation fails.
 */
static bool App_InitKeyboard(void)
{
    if(PGPIO_Init(&(App_Instance->Keyboard_Rows[0]),
                    APP_KEYBOARD_ROW_0_GPIO_PORT,
                    APP_KEYBOARD_ROW_0_PIN_NUMBER)
                    != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&(App_Instance->Keyboard_Rows[1]),
                    APP_KEYBOARD_ROW_1_GPIO_PORT,
                    APP_KEYBOARD_ROW_1_PIN_NUMBER)
                    != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&(App_Instance->Keyboard_Rows[2]),
                    APP_KEYBOARD_ROW_2_GPIO_PORT,
                    APP_KEYBOARD_ROW_2_PIN_NUMBER)
                    != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&(App_Instance->Keyboard_Rows[3]),
                    APP_KEYBOARD_ROW_3_GPIO_PORT,
                    APP_KEYBOARD_ROW_3_PIN_NUMBER)
                    != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&(App_Instance->Keyboard_Cols[0]),
                    APP_KEYBOARD_COL_0_GPIO_PORT,
                    APP_KEYBOARD_COL_0_PIN_NUMBER)
                    != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&(App_Instance->Keyboard_Cols[1]),
                    APP_KEYBOARD_COL_1_GPIO_PORT,
                    APP_KEYBOARD_COL_1_PIN_NUMBER)
                    != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&(App_Instance->Keyboard_Cols[2]),
                    APP_KEYBOARD_COL_2_GPIO_PORT,
                    APP_KEYBOARD_COL_2_PIN_NUMBER)
                    != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&(App_Instance->Keyboard_Cols[3]),
                    APP_KEYBOARD_COL_3_GPIO_PORT,
                    APP_KEYBOARD_COL_3_PIN_NUMBER)
                    != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(MK_Init(App_Instance->Keyboard,
               App_Instance->Keyboard_Config,
               App_Instance->Keyboard_Keys)
               != MK_OPERATION_OK)
    {
        return false;
    }

    if(MK_GPIO_ScanAdapterInit((&App_Instance->Keyboard->_scan_interface),
                                 App_Instance->Keyboard_Gpio_ScanAdapter)
                                 != MK_SCAN_OP_OK)
    {
        return false;
    }

    return true;
}

/**
 * @brief   Initializes the passive-buzzer path and Sound Generator Service.
 *
 * @details Creates the Platform PWM descriptor from the CubeMX timer context, injects it into the Buzzer Driver and then injects
 *          the initialized buzzer into the Sound Generator Service singleton.
 *
 * @note    Buzzer_Init() performs the underlying PWM initialization and establishes the driver's default duty cycle.
 * @note    Initialization is fail-fast and does not roll back stages that completed before a later failure.
 *
 * @return  true  - When the PWM descriptor, buzzer and sound service are initialized successfully.
 * @return  false - When any required initialization operation fails.
 */
static bool App_InitBuzzer(void)
{
    if(PPWM_Create(App_Instance->Buzzer_Pwm,
                   APP_BUZZER_PWM_CONTEXT,
                   APP_BUZZER_PWM_CHANNEL)
                   != PWM_OPERATION_OK)
    {
        return false;
    }

    if(Buzzer_Init(App_Instance->Buzzer,
                   App_Instance->Buzzer_Pwm)
                   != BUZZER_OPERATION_OK)
    {
        return false;
    }

    if(SGS_Init(App_Instance->Buzzer)
                != SGS_OPERATION_OK)
    {
        return false;
    }

    return true;
}

/**
 * @brief   Initializes lock-state visual indication.
 *
 * @details Creates the lock-status Platform GPIO descriptor, injects it and the configured active level into the LED Driver, and
 *          injects that LED instance into its dedicated Status Indication Service runtime.
 *
 * @note    The service instance owns only indication state; App Config owns the GPIO, LED and SIS storage.
 * @note    Initialization is fail-fast and does not roll back stages that completed before a later failure.
 *
 * @return  true  - When the GPIO, LED Driver and Status Indication Service are initialized successfully.
 * @return  false - When any required initialization operation fails.
 */
static bool App_InitLockStatusIndication(void)
{
    if(PGPIO_Init(App_Instance->Lock_Status_Led_Gpio,
                  APP_LOCK_STATUS_LED_GPIO_PORT,
                  APP_LOCK_STATUS_LED_PIN_NUMBER)
                  != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(LED_Init(App_Instance->Lock_Status_Led,
                App_Instance->Lock_Status_Led_Gpio,
                APP_LOCK_STATUS_LED_ACTIVE_LEVEL)
                != LED_OPERATION_OK)
    {
        return false;
    }

    if(SIS_Init(App_Instance->Lock_Status_Indication,
                App_Instance->Lock_Status_Led)
                != SIS_OPERATION_OK)
    {
        return false;
    }

    return true;
}

/**
 * @brief   Initializes low-battery visual indication.
 *
 * @details Creates the low-battery Platform GPIO descriptor, injects it and the configured active level into the LED Driver, and
 *          injects that LED instance into its dedicated Status Indication Service runtime.
 *
 * @note    The service instance owns only indication state; App Config owns the GPIO, LED and SIS storage.
 * @note    Initialization is fail-fast and does not roll back stages that completed before a later failure.
 *
 * @return  true  - When the GPIO, LED Driver and Status Indication Service are initialized successfully.
 * @return  false - When any required initialization operation fails.
 */
static bool App_InitLowBatteryStatusIndication(void)
{
    if(PGPIO_Init(App_Instance->LowBattery_Status_Led_Gpio,
                  APP_LOW_BATTERY_STATUS_LED_GPIO_PORT,
                  APP_LOW_BATTERY_STATUS_LED_PIN_NUMBER)
                  != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(LED_Init(App_Instance->LowBattery_Status_Led,
                App_Instance->LowBattery_Status_Led_Gpio,
                APP_LOW_BATTERY_STATUS_LED_ACTIVE_LEVEL)
                != LED_OPERATION_OK)
    {
        return false;
    }

    if(SIS_Init(App_Instance->LowBattery_Status_Indication,
                App_Instance->LowBattery_Status_Led)
                != SIS_OPERATION_OK)
    {
        return false;
    }

    return true;
}

/**
 * @brief   Initializes the temporary GPIO-based lock-actuator descriptor.
 *
 * @details Binds the configured lock-actuator GPIO descriptor to the CubeMX-configured LOCK_ACTUATOR pin. CubeMX-generated GPIO
 *          initialization has already driven that output low, which is the product's current safe locked request. The explicit
 *          Platform safe-state write in this initializer is currently disabled.
 *
 * @note    A dedicated Lock Actuator Driver shall eventually replace this direct Platform binding while preserving the same
 *          force-safe behavior and bounded-unlock contract.
 *
 * @return  true  - When the GPIO descriptor is initialized successfully.
 * @return  false - When GPIO descriptor initialization fails.
 */
static bool App_InitLockActuator(void)
{
    if(PGPIO_Init(App_Instance->Lock_Actuator_Gpio,
                  APP_LOCK_ACTUATOR_GPIO_PORT,
                  APP_LOCK_ACTUATOR_PIN_NUMBER)
                  != GPIO_OPERATION_OK)
    {
        return false;
    }

    // return App_ForceLock();
    return true;
}

/**
 * @brief   Performs one complete Matrix Keyboard Driver acquisition cycle.
 *
 * @details Delegates matrix scanning, debounce processing and pending-action retrieval to MK_Read(). The keyboard instance remains
 *          owned by App Config and is borrowed through the runtime registry.
 *
 * @param   Output - Caller-owned value object that receives the current key code and key action; must not be NULL.
 *
 * @return  MK_OPERATION_OK   - When keyboard acquisition and processing complete successfully.
 * @return  MK_OPERATION_FAIL - When Output is invalid or the Matrix Keyboard Driver reports failure.
 */
static MK_OpStatus_t App_ReadKeyboard(MK_Output_t* Output)
{
    if(Output == NULL)
    {
        return MK_OPERATION_FAIL;
    }

    return MK_Read(App_Instance->Keyboard, Output);
}

/**
 * @brief   Translates a Matrix Keyboard Driver output into a Credential Entry Service input kind.
 *
 * @details Maps numeric key codes to CES digit inputs, '#' to confirmation and '*' to clear/cancel. Unsupported key codes produce
 *          CES_INPUT_KIND_NONE. Command and unsupported inputs carry APP_CREDENTIAL_ENTRY_DIGIT_INVALID because their semantic
 *          meaning does not include a numeric digit.
 *
 * @param   KeyboardOutput - Pointer to the keyboard output whose key code shall be translated.
 *
 * @note    A NULL input is treated as an unsupported key and produces CES_INPUT_KIND_NONE.
 *
 * @return  A fully initialized CES_Input_t value representing the supplied key code, or CES_INPUT_KIND_NONE when no supported
 *          semantic credential command can be produced.
 */
static CES_Input_t App_TranslateKeyToInputKind(const MK_Output_t* KeyboardOutput)
{
    CES_Input_t ces_input =
    {
        .Kind  = CES_INPUT_KIND_NONE,
        .Digit = APP_CREDENTIAL_ENTRY_DIGIT_INVALID
    };

    if(KeyboardOutput == NULL)
    {
        return ces_input;
    }

    MK_KeyCode_t key = KeyboardOutput->Key;

    switch(key)
    {
        case '0':
            ces_input.Kind  = CES_INPUT_KIND_DIGIT;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_0;
        break;

        case '1':
            ces_input.Kind  = CES_INPUT_KIND_DIGIT;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_1;
        break;

        case '2':
            ces_input.Kind  = CES_INPUT_KIND_DIGIT;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_2;
        break;

        case '3':
            ces_input.Kind  = CES_INPUT_KIND_DIGIT;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_3;
        break;

        case '4':
            ces_input.Kind  = CES_INPUT_KIND_DIGIT;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_4;
        break;

        case '5':
            ces_input.Kind  = CES_INPUT_KIND_DIGIT;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_5;
        break;

        case '6':
            ces_input.Kind  = CES_INPUT_KIND_DIGIT;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_6;
        break;

        case '7':
            ces_input.Kind  = CES_INPUT_KIND_DIGIT;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_7;
        break;

        case '8':
            ces_input.Kind  = CES_INPUT_KIND_DIGIT;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_8;
        break;

        case '9':
            ces_input.Kind  = CES_INPUT_KIND_DIGIT;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_9;
        break;

        case '#':
            ces_input.Kind  = CES_INPUT_KIND_CONFIRM;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_INVALID;
        break;

        case '*':
            ces_input.Kind  = CES_INPUT_KIND_CLEAR_CANCEL;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_INVALID;
        break;

        default:
            ces_input.Kind  = CES_INPUT_KIND_NONE;
            ces_input.Digit = APP_CREDENTIAL_ENTRY_DIGIT_INVALID;
        break;
    }

    return ces_input;
}

/**
 * @brief   Processes one semantic credential command through the Credential Entry Service.
 *
 * @details Provides the application input boundary so physical keyboard types never cross into CES. The
 *          service remains authoritative for candidate mutation, command validation and credential-entry event production.
 *
 * @param   Input - Semantic input command to process; must not be NULL.
 *
 * @return  The CES_Event_t produced by the Credential Entry Service, or CES_EVENT_NONE when Input is NULL or no active session
 *          accepts the command.
 */
static CES_Event_t App_ProcessCredentialInput(const CES_Input_t* Input)
{
    if(Input == NULL)
    {
        return CES_EVENT_NONE;
    }

    return CES_ProcessInput(Input);
}

/**
 * @brief   Starts or restarts one application-owned timeout.
 *
 * @details Validates the identifier and its immutable definition, captures the current monotonic Platform timestamp and replaces
 *          the previous timeout runtime. Replacement supports credential-entry restarts and transitions among every mutually
 *          exclusive timed LCS phase.
 *
 * @param   Timeout - Concrete timeout definition to activate; must be below APP_TIMEOUT_COUNT.
 *
 * @return  true  - When a valid nonzero timeout definition is activated.
 * @return  false - When the identifier, duration or elapsed-event definition is invalid.
 */
bool App_StartTimeout(App_TimeoutId_t Timeout)
{
    if(Timeout >= APP_TIMEOUT_COUNT)
    {
        return false;
    }

    const App_TimeoutDefinition_t* definition = &App_TimeoutDefinitions[Timeout];

    if(definition->DurationMs == 0U ||
       definition->ElapsedEvent <= LCS_EVENT_NONE ||
       definition->ElapsedEvent >= LCS_EVENT_COUNT)
    {
        return false;
    }

    App_ActiveTimeout.Id          = Timeout;
    App_ActiveTimeout.StartedAtMs = Platform_GetMillis();
    App_ActiveTimeout.Active      = true;

    return true;
}

/**
 * @brief   Cancels the currently active application timeout.
 *
 * @details Restores the complete timeout runtime to its inactive sentinel representation. Cancellation produces no LCS event and
 *          does not access the time source.
 */
void App_CancelTimeout(void)
{
    App_ActiveTimeout.Id          = APP_TIMEOUT_NONE;
    App_ActiveTimeout.StartedAtMs = 0U;
    App_ActiveTimeout.Active      = false;
}

/**
 * @brief   Converts an elapsed application timeout into one semantic LCS event.
 *
 * @details Reads the selected immutable definition, samples Platform time, and delegates rollover-safe interval validation to
 *          TVS_HasElapsed(). When the interval expires, the runtime is cancelled before its event is returned so repeated dispatch
 *          cycles cannot emit the same timeout more than once.
 *
 * @note    This function owns no timer peripheral and performs no blocking delay. App_Dispatch() calls it once per execution cycle.
 *
 * @return  The elapsed-event value associated with the active definition, or LCS_EVENT_NONE while no timeout has elapsed.
 */
static LCS_Event_t App_PollTimeout(void)
{
    if(!App_ActiveTimeout.Active || App_ActiveTimeout.Id >= APP_TIMEOUT_COUNT)
    {
        return LCS_EVENT_NONE;
    }

    const App_TimeoutDefinition_t* definition = &App_TimeoutDefinitions[App_ActiveTimeout.Id];

    TVS_TimestampMs_t current_time_ms = Platform_GetMillis();

    if(!TVS_HasElapsed(App_ActiveTimeout.StartedAtMs,
                       current_time_ms,
                       definition->DurationMs))
    {
        return LCS_EVENT_NONE;
    }

    LCS_Event_t elapsed_event = definition->ElapsedEvent;

    App_CancelTimeout();

    return elapsed_event;
}

/**
 * @brief   Serially dispatches one event and every bounded synchronous follow-up through Lock Control.
 *
 * @details Calls LCS_Process(), executes its returned action, and repeats while authentication, registration validation, storage or
 *          fail-safe execution produces a synchronous follow-up event. A small fixed bound prevents an accidental action cycle
 *          from monopolizing the caller. The current overflow fallback cancels timing, ends CES, stops sound and disables the LCD
 *          backlight.
 *
 * @param   Event - First semantic event to dispatch; sentinels and out-of-range values are ignored.
 *
 * @note    All event processing and synchronous follow-up actions complete in the serialized context that initiated dispatch,
 *          whether App_ReadInput() or App_Dispatch().
 * @warning Direct actuator-safe and controlled-reset calls are currently disabled in the overflow fallback. Until this path is
 *          consolidated with App Executor's controlled-reset cleanup, it does not provide the complete fail-safe behavior.
 */
static void App_DispatchLcsEvent(LCS_Event_t Event)
{
    if(Event <= LCS_EVENT_NONE || Event >= LCS_EVENT_COUNT)
    {
        return;
    }

    LCS_Event_t pending_event = Event;

    for(uint8_t depth = 0U;
        depth < APP_MAX_LCS_DISPATCH_DEPTH && pending_event > LCS_EVENT_NONE && pending_event < LCS_EVENT_COUNT;
        depth++)
    {
        LCS_Event_t trigger_event = pending_event;
        LCS_Action_t action = LCS_Process(trigger_event);

        pending_event = App_ExecuteAction(action);
    }

    if(pending_event != LCS_EVENT_NONE)
    {
        App_CancelTimeout();

        (void)CES_EndSession();
        (void)SGS_Stop();
        (void)LCD_BacklightOff(App_Instance->Lcd);

        App_ExecuteAction(LCS_ACTION_REQUEST_CONTROLLED_RESET);
    }
}

/**
 * @brief   Applies one Credential Entry Service event to presentation, timing or Lock Control.
 *
 * @details Accepted digits update the masked length and restart entry inactivity timing. Clear updates the masked presentation
 *          locally; incomplete, ready and cancelled outcomes are translated into their semantic Lock Control events so LCS can
 *          select the state-specific refresh, validation, authentication or cleanup action.
 *
 * @param[in] Event - Credential Entry Service outcome produced by CES_ProcessInput().
 */
static void App_HandleCredentialEvent(CES_Event_t Event)
{
    uint32_t current_time_ms = Platform_GetMillis();

    switch(Event)
    {
        case CES_EVENT_INPUT_ACCEPTED:
        {
            CES_Length_t entered_digits = CES_GetCurrentLength();

            if(!App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                App_DispatchLcsEvent(LCS_EVENT_ENTRY_TIMEOUT);
                break;
            }

            (void)SGS_Ring(SGS_RINGTONE_KEYPRESS, current_time_ms);

            (void)DRS_SetScreen(DRS_SCREEN_PASSWORD_ENTRY);

            if(entered_digits <= DRS_ENTRY_DIGIT_CAPACITY)
            {
                (void)DRS_SetEnteredDigits(entered_digits);
            }

            (void)DRS_Update();
        }
        break;

        case CES_EVENT_INCOMPLETE:

            App_DispatchLcsEvent(LCS_EVENT_CANDIDATE_INCOMPLETE);

        break;

        case CES_EVENT_CLEARED:

            (void)DRS_SetScreen(DRS_SCREEN_PASSWORD_ENTRY);
            (void)DRS_SetEnteredDigits(0U);
            (void)DRS_Update();
            (void)SGS_Ring(SGS_RINGTONE_KEYPRESS, current_time_ms);

        break;

        case CES_EVENT_READY:

            App_DispatchLcsEvent(LCS_EVENT_CANDIDATE_READY);

        break;

        case CES_EVENT_CANCELLED:

            App_DispatchLcsEvent(LCS_EVENT_CREDENTIAL_CANCELLED);

        break;

        case CES_EVENT_NONE:
        default:
            break;
    }
}

/**
 * @brief   Routes one acquired keyboard action through wake semantics or credential entry.
 *
 * @details First offers every debounced click to LCS as a credential-entry request. When that event opens a new session, the wake
 *          key is consumed. During an already active session, key 'C' becomes a credential-register request and is likewise
 *          consumed; every other supported key is translated and offered to CES.
 *
 * @param   Event - Complete keyboard value acquired during the current dispatch cycle; must not be NULL.
 */
static void App_HandleKeyboardEvent(const MK_Output_t* Event)
{
    if(Event == NULL || Event->Action != MK_KEY_ACTION_CLICK)
    {
        return;
    }

    LCS_Action_t wake_action = LCS_Process(LCS_EVENT_CREDENTIAL_ENTRY_REQUESTED);

    if(wake_action == LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION)
    {
        LCS_Event_t follow_up = App_ExecuteAction(wake_action);

        if(follow_up != LCS_EVENT_NONE)
        {
            App_DispatchLcsEvent(follow_up);
        }

        return;
    }

    LCS_Event_t unexpected_follow_up = App_ExecuteAction(wake_action);

    if(unexpected_follow_up != LCS_EVENT_NONE)
    {
        App_DispatchLcsEvent(unexpected_follow_up);

        return;
    }

    if(Event->Key == 'C')
    {
        App_DispatchLcsEvent(LCS_EVENT_CREDENTIAL_REGISTER_REQUESTED);

        return;
    }

    CES_Input_t input = App_TranslateKeyToInputKind(Event);

    if(input.Kind != CES_INPUT_KIND_NONE)
    {
        App_HandleCredentialEvent(App_ProcessCredentialInput(&input));
    }
}

/**
 * @brief   Advances timeout, display, LED and sound state during one application-dispatch cycle.
 *
 * @details Polls the single application-owned timeout first and serially dispatches any elapsed event. It then advances every
 *          non-blocking presentation service. UI update failures remain observable through their return values during future fault
 *          integration but do not bypass the independent lock-actuator safe path.
 */
static void App_UpdateServices(void)
{
    LCS_Event_t timeout_event = App_PollTimeout();

    if(timeout_event != LCS_EVENT_NONE)
    {
        App_DispatchLcsEvent(timeout_event);
    }

    uint32_t current_time_ms = Platform_GetMillis();

    (void)DRS_Update();
    (void)SIS_Update(App_Instance->Lock_Status_Indication);
    (void)SIS_Update(App_Instance->LowBattery_Status_Indication);
    (void)SGS_Update(current_time_ms);
}

/**
 * @brief   Abandons an untrustworthy credential-input sequence safely.
 *
 * @details Dispatches credential cancellation so an active session is erased and returned to locked state. It then requests error
 *          feedback. If the FSM is in another state, the cancellation event is harmlessly ignored and its active timeout remains
 *          authoritative.
 */
static void App_HandleInputFault(void)
{
    uint32_t current_time_ms = Platform_GetMillis();

    App_DispatchLcsEvent(LCS_EVENT_CREDENTIAL_CANCELLED);
    (void)LCD_BacklightOn(App_Instance->Lcd);
    (void)DRS_SetScreen(DRS_SCREEN_ACCESS_DENIED);
    (void)DRS_Update();
    (void)SGS_Ring(SGS_RINGTONE_ERROR, current_time_ms);
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes the complete electronic-lock application object graph.
 *
 * @details Initializes the actuator descriptor first, followed by the LCD/render path, keyboard, buzzer/sound path and both
 *          indication instances. After every dependency succeeds, CSS availability selects either the first-registration route
 *          through LCS_EVENT_CREDENTIAL_NOT_REGISTERED or normal activation through LCS_EVENT_INIT_OK and locked-idle presentation.
 *
 * @note    CSS_HasCredential() provides only availability. The installed credential is copied into the configured runtime buffer lazily
 *          when the first authentication action executes.
 * @note    This operation shall run after CubeMX peripheral initialization and before the first App_ReadInput() or App_Dispatch().
 * @note    A failure forces the actuator low when possible and reports LCS_EVENT_INIT_FAIL while the FSM remains in boot. The
 *          resulting controlled-reset action does not return on the STM32 target; APP_INIT_FAILED is retained as a defensive
 *          fallback contract. Already initialized static objects are not deinitialized before the reset request.
 *
 * @return  APP_INIT_SUCCESSFULLY - When every required application dependency is initialized.
 * @return  APP_INIT_FAILED       - When any Platform, adapter, component or service initialization fails.
 */
App_InitStatus_t App_Init(void)
{
    App_Instance = App_GetRuntimeInstances();

    bool initialized = App_InitLockActuator()               &&
                       App_InitLcd()                        &&
                       App_InitKeyboard()                   &&
                       App_InitBuzzer()                     &&
                       App_InitLockStatusIndication()       &&
                       App_InitLowBatteryStatusIndication() ;

    if(!initialized)
    {
        // (void)App_ForceLock();

        LCS_Action_t failure_action = LCS_Process(LCS_EVENT_INIT_FAIL);

        (void)App_ExecuteAction(failure_action);

        return APP_INIT_FAILED;
    }

    if(!CSS_HasCredential())
    {
        App_DispatchLcsEvent(LCS_EVENT_CREDENTIAL_NOT_REGISTERED);
    }

    else
    {
        (void)LCS_Process(LCS_EVENT_INIT_OK);
        (void)DRS_SetScreen(DRS_SCREEN_IDLE);
        (void)DRS_SetEnteredDigits(0U);
        (void)DRS_Update();
        (void)LCD_BacklightOff(App_Instance->Lcd);
        (void)SIS_SetIndication(App_Instance->Lock_Status_Indication, SIS_INDICATION_LOCKED);
    }

    return APP_INIT_SUCCESSFULLY;
}

/**
 * @brief   Acquires and processes one non-blocking keyboard input sample.
 *
 * @details Reads the matrix keyboard once and routes any completed key action
 *          through the credential-entry flow. When no key action is available,
 *          the function returns without modifying the application state.
 *
 *          A keyboard acquisition failure invokes the application input-fault
 *          policy, which safely abandons any active credential-entry sequence
 *          and requests error feedback.
 *
 * @note    App_Init() shall complete successfully before the first call.
 * 
 * @note    This function shall be called continuously at a cadence compatible
 *          with the matrix-keyboard debounce requirements.
 * 
 * @note    This function does not update presentation services or poll the
 *          application-owned timeout; those operations belong to App_Dispatch().
 */
void App_ReadInput(void)
{
    MK_Output_t keyboard_output = {0};

    if(App_ReadKeyboard(&keyboard_output) == MK_OPERATION_OK)
    {
        if(keyboard_output.Action != MK_KEY_ACTION_NONE)
        {
            App_HandleKeyboardEvent(&keyboard_output);
        }
    }
    else
    {
        App_HandleInputFault();
    }
}

/**
 * @brief   Executes one synchronous and non-blocking service-dispatch cycle.
 *
 * @details Polls the active application-owned timeout and dispatches its
 *          semantic event to the Lock Control Service when the interval has
 *          elapsed. It then advances the display, status-indication and sound
 *          services exactly once.
 *
 *          The function performs no deliberate delay and never waits for a
 *          timeout or presentation phase to complete.
 *
 * @note    App_Init() shall complete successfully before the first call.
 * 
 * @note    This function does not acquire or process keyboard input; input
 *          acquisition is performed separately by App_ReadInput().
 * 
 * @note    The execution owner shall call this function periodically with a
 *          cadence compatible with timeout-response and non-blocking
 *          presentation-progression requirements.
 */
void App_Dispatch(void)
{
    App_UpdateServices();
}
