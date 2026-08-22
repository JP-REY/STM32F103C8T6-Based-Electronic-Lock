/**********************************************************************************************************************************
 * @file    App_Core.c
 * @brief   Electronic-lock application composition root.
 *
 * @details Implements the electronic-lock composition root. This module owns the static-duration Platform handles, adapter
 *          contexts, component instances and instance-based service runtimes required by the product. It also binds these objects
 *          to the CubeMX-generated peripheral handles during App_Init().
 *
 *          Initialization is fail-fast: each private initializer validates every stage and immediately reports failure to
 *          App_Init(). Stateless services and services that own an internal singleton runtime do not require an additional
 *          application-side handle.
 *
 *          App_ReadInput() acquires and translates physical input, while App_Dispatch() evaluates application-owned timeouts and
 *          advances presentation services. Both paths may synchronously execute semantic Lock Control actions without exposing
 *          concrete hardware dependencies through App_Core.h.
 *
 * @note    All objects in this translation unit have static storage duration because drivers, adapters and services retain
 *          borrowed pointers to their dependencies after initialization.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    Aug 22, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "App_Core.h"
#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"

/**
 * @brief Domain-service dependencies.
 */
#include "Timeout_Validation_Service.h"
#include "Credential_Entry_Service.h"
#include "Credential_Register_Service.h"
#include "Credential_Storage_Service.h"
#include "Authentication_Service.h"
#include "Lock_Control_Service.h"

/**
 * @brief User-interface service dependencies.
 */
#include "Status_Indication_Service.h"
#include "Sound_Generator_Service.h"
#include "Display_Render_Service.h"

/**
 * @brief Component-driver and concrete-adapter dependencies.
 */
#include "LCD_Driver.h"
#include "LCD_PCF8574_BusAdapter.h"
#include "LCD_PWM_BacklightAdapter.h"
#include "PCF8574_Driver.h"
#include "MatrixKeyboard_Driver.h"
#include "MatrixKeyboard_GPIO_ScanAdapter.h"
#include "Buzzer_Driver.h"
#include "Led_Driver.h"

/**
 * @brief Platform interfaces used directly by the composition root.
 */
#include "GPIO_Platform_Interface.h"
#include "PWM_Platform_Interface.h"
#include "Time_Platform_Interface.h"

/**
 * @brief CubeMX-generated hardware bindings consumed during initialization.
 */
#include "main.h"
#include "i2c.h"
#include "tim.h"

/**********************************************************************************************************************************
 Private Macros
 **********************************************************************************************************************************/
/** @brief Number of visible columns provided by the product character LCD. */
#define APP_LCD_COLUMN_COUNT                    (16U)

/** @brief I2C device address assigned to the PCF8574 LCD I/O expander. */
#define APP_LCD_IO_EXPANDER_ADDRESS             (0x20U)

/** @brief CubeMX I2C peripheral handle used by the LCD I/O expander. */
#define APP_LCD_IO_EXPANDER_I2C_CONTEXT         (&hi2c1)

/** @brief CubeMX timer handle used to generate the LCD-backlight PWM signal. */
#define APP_LCD_BACKLIGHT_PWM_CONTEXT           (&htim4)

/** @brief Platform PWM channel associated with the LCD-backlight output. */
#define APP_LCD_BACKLIGHT_PWM_CHANNEL           (PWM_CHANNEL_4)

/** @brief LCD-backlight PWM frequency, in hertz. */
#define APP_LCD_BACKLIGHT_PWM_FREQUENCY         (1500U)

/** @brief Initial LCD-backlight brightness, expressed as a percentage. */
#define APP_LCD_BACKLIGHT_BRIGHTNESS            (50U)

/** @brief Number of rows in the physical matrix keyboard. */
#define APP_KEYBOARD_ROW_COUNT                  (4U)

/** @brief Number of columns in the physical matrix keyboard. */
#define APP_KEYBOARD_COLUMN_COUNT               (4U)

/** @brief Number of physical keys derived from the keyboard dimensions. */
#define APP_KEYBOARD_KEY_COUNT                  (APP_KEYBOARD_ROW_COUNT * APP_KEYBOARD_COLUMN_COUNT)

/** @brief Required stable interval before a keyboard transition is accepted, in milliseconds. */
#define APP_KEYBOARD_DEBOUNCE_MS                (40U)

/** @brief STM32 GPIO port connected to matrix-keyboard row zero. */
#define APP_KEYBOARD_ROW_0_GPIO_PORT            (GPIOA)

/** @brief Zero-based STM32 GPIO pin number connected to matrix-keyboard row zero. */
#define APP_KEYBOARD_ROW_0_PIN_NUMBER           (3U)

/** @brief STM32 GPIO port connected to matrix-keyboard row one. */
#define APP_KEYBOARD_ROW_1_GPIO_PORT            (GPIOA)

/** @brief Zero-based STM32 GPIO pin number connected to matrix-keyboard row one. */
#define APP_KEYBOARD_ROW_1_PIN_NUMBER           (2U)

/** @brief STM32 GPIO port connected to matrix-keyboard row two. */
#define APP_KEYBOARD_ROW_2_GPIO_PORT            (GPIOA)

/** @brief Zero-based STM32 GPIO pin number connected to matrix-keyboard row two. */
#define APP_KEYBOARD_ROW_2_PIN_NUMBER           (1U)

/** @brief STM32 GPIO port connected to matrix-keyboard row three. */
#define APP_KEYBOARD_ROW_3_GPIO_PORT            (GPIOA)

/** @brief Zero-based STM32 GPIO pin number connected to matrix-keyboard row three. */
#define APP_KEYBOARD_ROW_3_PIN_NUMBER           (0U)

/** @brief STM32 GPIO port connected to matrix-keyboard column zero. */
#define APP_KEYBOARD_COL_0_GPIO_PORT            (GPIOA)

/** @brief Zero-based STM32 GPIO pin number connected to matrix-keyboard column zero. */
#define APP_KEYBOARD_COL_0_PIN_NUMBER           (7U)

/** @brief STM32 GPIO port connected to matrix-keyboard column one. */
#define APP_KEYBOARD_COL_1_GPIO_PORT            (GPIOA)

/** @brief Zero-based STM32 GPIO pin number connected to matrix-keyboard column one. */
#define APP_KEYBOARD_COL_1_PIN_NUMBER           (6U)

/** @brief STM32 GPIO port connected to matrix-keyboard column two. */
#define APP_KEYBOARD_COL_2_GPIO_PORT            (GPIOA)

/** @brief Zero-based STM32 GPIO pin number connected to matrix-keyboard column two. */
#define APP_KEYBOARD_COL_2_PIN_NUMBER           (5U)

/** @brief STM32 GPIO port connected to matrix-keyboard column three. */
#define APP_KEYBOARD_COL_3_GPIO_PORT            (GPIOA)

/** @brief Zero-based STM32 GPIO pin number connected to matrix-keyboard column three. */
#define APP_KEYBOARD_COL_3_PIN_NUMBER           (4U)

/** @brief CubeMX timer handle used to generate the passive-buzzer PWM signal. */
#define APP_BUZZER_PWM_CONTEXT                  (&htim3)

/** @brief Platform PWM channel selected for the passive-buzzer output. */
#define APP_BUZZER_PWM_CHANNEL                  (PWM_CHANNEL_1)

/** @brief STM32 GPIO port connected to the lock-status LED. */
#define APP_LOCK_STATUS_LED_GPIO_PORT           (GPIOA)

/** @brief Zero-based STM32 GPIO pin number connected to the lock-status LED. */
#define APP_LOCK_STATUS_LED_PIN_NUMBER          (15U)

/** @brief Electrical level that turns the lock-status LED on. */
#define APP_LOCK_STATUS_LED_ACTIVE_LEVEL        (LED_ACTIVE_LOW)

/** @brief STM32 GPIO port connected to the low-battery status LED. */
#define APP_LOW_BATTERY_STATUS_LED_GPIO_PORT    (GPIOA)

/** @brief Zero-based STM32 GPIO pin number connected to the low-battery status LED. */
#define APP_LOW_BATTERY_STATUS_LED_PIN_NUMBER   (12U)

/** @brief Electrical level that turns the low-battery status LED on. */
#define APP_LOW_BATTERY_STATUS_LED_ACTIVE_LEVEL (LED_ACTIVE_LOW)

/** @brief Credential Entry Service numeric value associated with keyboard key '0'. */
#define APP_CREDENTIAL_ENTRY_DIGIT_0            (0U)

/** @brief Credential Entry Service numeric value associated with keyboard key '1'. */
#define APP_CREDENTIAL_ENTRY_DIGIT_1            (1U)

/** @brief Credential Entry Service numeric value associated with keyboard key '2'. */
#define APP_CREDENTIAL_ENTRY_DIGIT_2            (2U)

/** @brief Credential Entry Service numeric value associated with keyboard key '3'. */
#define APP_CREDENTIAL_ENTRY_DIGIT_3            (3U)

/** @brief Credential Entry Service numeric value associated with keyboard key '4'. */
#define APP_CREDENTIAL_ENTRY_DIGIT_4            (4U)

/** @brief Credential Entry Service numeric value associated with keyboard key '5'. */
#define APP_CREDENTIAL_ENTRY_DIGIT_5            (5U)

/** @brief Credential Entry Service numeric value associated with keyboard key '6'. */
#define APP_CREDENTIAL_ENTRY_DIGIT_6            (6U)

/** @brief Credential Entry Service numeric value associated with keyboard key '7'. */
#define APP_CREDENTIAL_ENTRY_DIGIT_7            (7U)

/** @brief Credential Entry Service numeric value associated with keyboard key '8'. */
#define APP_CREDENTIAL_ENTRY_DIGIT_8            (8U)

/** @brief Credential Entry Service numeric value associated with keyboard key '9'. */
#define APP_CREDENTIAL_ENTRY_DIGIT_9            (9U)

/** @brief Sentinel stored when a CES command carries no numeric digit. */
#define APP_CREDENTIAL_ENTRY_DIGIT_INVALID      (0xFFU)

/** @brief Maximum credential-entry inactivity interval, in milliseconds. */
#define APP_CREDENTIAL_ENTRY_TIMEOUT_MS         (5000U)

/** @brief Maximum authorized unlock interval, in milliseconds. */
#define APP_UNLOCK_TIMEOUT_MS                   (3000U)

/** @brief Duration of access-denied feedback before the FSM advances, in milliseconds. */
#define APP_ACCESS_DENIED_TIMEOUT_MS            (1500U)

/** @brief Duration for which credential entry remains blocked after the attempt limit, in milliseconds. */
#define APP_LOCKOUT_TIMEOUT_MS                  (10000U)

/** @brief Duration of credential-register save feedback before the FSM advances, in milliseconds. */
#define APP_CRS_SAVED_TIMEOUT_MS                (1500U)

/** @brief Maximum number of synchronous LCS follow-up events processed in one dispatch chain. */
#define APP_MAX_LCS_DISPATCH_DEPTH              (4U)

/** @brief STM32 GPIO port connected to the physical lock actuator control input. */
#define APP_LOCK_ACTUATOR_GPIO_PORT             (GPIOB)

/** @brief Zero-based STM32 GPIO pin number connected to the physical lock actuator control input. */
#define APP_LOCK_ACTUATOR_PIN_NUMBER            (8U)

/**********************************************************************************************************************************
 Private Types
 **********************************************************************************************************************************/
/**
 * @brief   Application-owned timeout identifiers.
 *
 * @details Identifies each mutually exclusive bounded interval associated with one timed Lock Control FSM state. The identifier
 *          indexes App_TimeoutDefinitions and is retained by App_ActiveTimeout while the corresponding interval is active.
 *
 * @note    APP_TIMEOUT_NONE is a runtime sentinel and shall never index App_TimeoutDefinitions.
 */
typedef enum
{
    APP_TIMEOUT_CREDENTIAL_ENTRY = 0U,    /*< Credential-entry inactivity interval.        */

    APP_TIMEOUT_UNLOCK,                   /*< Authorized physical-unlock interval.         */

    APP_TIMEOUT_ACCESS_DENIED,            /*< Access-denied feedback interval.             */

    APP_TIMEOUT_LOCKOUT,                  /*< Temporary credential-entry lockout interval. */

    APP_TIMEOUT_CRS_SAVED,                /*< Credential-register saved feedback interval. */

    APP_TIMEOUT_COUNT,                    /*< Number of concrete timeout definitions.      */

    APP_TIMEOUT_NONE = APP_TIMEOUT_COUNT  /*< Sentinel indicating no active timeout.       */

}App_TimeoutId_t;

/**
 * @brief   Immutable policy associated with one application timeout.
 *
 * @details Associates a nonzero timeout duration with the semantic Lock Control event that shall be dispatched exactly once when
 *          that interval elapses. Definition objects are compile-time configuration and contain no mutable lifecycle state.
 */
typedef struct
{
    TVS_DurationMs_t DurationMs;   /*< Configured interval duration in milliseconds; must be nonzero. */
    LCS_Event_t      ElapsedEvent; /*< Semantic event produced once when the interval expires.       */

}App_TimeoutDefinition_t;

/**
 * @brief   Mutable lifecycle state of the currently active application timeout.
 *
 * @details Retains the selected definition identifier and its monotonic start timestamp. The application requires only one
 *          runtime because the timed Lock Control FSM states are mutually exclusive.
 *
 * @note    Active is authoritative. Id is APP_TIMEOUT_NONE and StartedAtMs is zero while no interval is active.
 */
typedef struct
{
    App_TimeoutId_t   Id;          /*< Concrete definition currently selected, or APP_TIMEOUT_NONE.   */
    TVS_TimestampMs_t StartedAtMs; /*< Monotonic Platform timestamp captured when the interval began. */
    bool              Active;      /*< True only while the selected interval is pending.              */

}App_TimeoutRuntime_t;

/**********************************************************************************************************************************
 Private Constants
 **********************************************************************************************************************************/
/**
 * @brief   Matrix-keyboard logical key map in physical row-major order.
 *
 * @details Associates every physical key position with the device-level code returned by the Matrix Keyboard Driver. Its static
 *          storage duration satisfies the lifetime required by App_KeyboardConfig and App_Keyboard.
 */
static const MK_KeyCode_t App_KeyboardKeyMap[APP_KEYBOARD_KEY_COUNT] =
{
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D'
};

/**
 * @brief   Immutable Matrix Keyboard Driver configuration.
 *
 * @details Defines the 4x4 matrix dimensions, key map, active-low electrical interpretation and debounce policy. The driver
 *          borrows this object after MK_Init(), so it remains owned by the composition root for the complete firmware lifetime.
 */
static const MK_Config_t App_KeyboardConfig =
{
    ._rows_number      = APP_KEYBOARD_ROW_COUNT,
    ._cols_number      = APP_KEYBOARD_COLUMN_COUNT,
    ._key_map          = App_KeyboardKeyMap,
    ._row_active_level = MK_KEY_ACTIVE_LOW,
    ._debounce_time_ms = APP_KEYBOARD_DEBOUNCE_MS
};

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

/**
 * @brief   Verifies that credential size contracts remain compatible across the participating services.
 *
 * @details Authentication, registration staging, persistent storage and Display Render all operate on the credential collected
 *          by CES. Compile-time failures prevent silent truncation if any participating service changes its length independently.
 */
_Static_assert(CES_CREDENTIAL_LENGTH == AS_CREDENTIAL_LENGTH,
               "Credential Entry and Authentication lengths must match");

_Static_assert(CES_CREDENTIAL_LENGTH == CRS_CREDENTIAL_LENGTH,
               "Credential Entry and Credential Register lengths must match");


_Static_assert(CES_CREDENTIAL_LENGTH == CSS_CREDENTIAL_LENGTH,
               "Credential Entry and Credential Storage lengths must match");

_Static_assert(CES_CREDENTIAL_LENGTH == DRS_ENTRY_DIGIT_CAPACITY,
               "Credential Entry and Display Render capacities must match");

/**********************************************************************************************************************************
 Private Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Matrix Keyboard Driver Objects
 **********************************************************************************************************************************/
/**
 * @brief   Platform GPIO descriptors associated with the matrix-keyboard rows.
 *
 * @details Each descriptor will be bound by PGPIO_Init() to its corresponding CubeMX MKB_ROWx pin. The array is subsequently
 *          borrowed by App_KeyboardGpioScanAdapter and therefore has static storage duration.
 */
static GPIO_Handle_t App_KeyboardRowGpios[APP_KEYBOARD_ROW_COUNT] = {0};

/**
 * @brief   Platform GPIO descriptors associated with the matrix-keyboard columns.
 *
 * @details Each descriptor will be bound by PGPIO_Init() to its corresponding CubeMX MKB_COLx pin. The array is subsequently
 *          borrowed by App_KeyboardGpioScanAdapter and therefore has static storage duration.
 */
static GPIO_Handle_t App_KeyboardColumnGpios[APP_KEYBOARD_COLUMN_COUNT] = {0};

/**
 * @brief   Concrete GPIO scan-adapter context used by the Matrix Keyboard Driver.
 *
 * @details Retains the application-owned row and column arrays and the active level driven while selecting a matrix column.
 *          MK_GPIO_ScanAdapterInit() binds this context to the scan interface embedded in App_Keyboard.
 */
static MK_GPIO_ScanAdapter_t App_KeyboardGpioScanAdapter =
{
    .Columns     = App_KeyboardColumnGpios,
    .ColumnCount = APP_KEYBOARD_COLUMN_COUNT,
    .Rows        = App_KeyboardRowGpios,
    .RowCount    = APP_KEYBOARD_ROW_COUNT,
    .ActiveLevel = GPIO_LEVEL_LOW
};

/**
 * @brief   Per-key runtime storage borrowed and managed by the Matrix Keyboard Driver.
 *
 * @details Provides one independent state and debounce context for every physical key. Application code must not inspect or
 *          modify these elements after they are supplied to MK_Init().
 */
static MK_Key_t App_KeyboardKeys[APP_KEYBOARD_KEY_COUNT] = {0};

/**
 * @brief   Matrix Keyboard Driver instance owned by the application.
 *
 * @details Retains the scan interface, immutable configuration and per-key runtime references. 
 *          App_ReadInput() performs one non-blocking acquisition from this instance per call.
 */
static MK_Handle_t App_Keyboard = {0};

/**********************************************************************************************************************************
 LCD Driver and Display Render Service Objects
 **********************************************************************************************************************************/
/**
 * @brief   Platform PWM descriptor for the LCD backlight.
 *
 * @details Will be created from the CubeMX TIM4 channel 4 binding and borrowed by the LCD PWM Backlight Adapter for the complete
 *          LCD lifetime.
 */
static PWM_Handle_t App_LcdBacklightPwm = {0};

/**
 * @brief   PCF8574 Driver instance used as the LCD parallel-bus backend.
 *
 * @details Will retain the CubeMX I2C1 context, device address and output-port shadow. The LCD PCF8574 Bus Adapter borrows this
 *          object after initialization.
 */
static PCF8574_Handle_t App_LcdIoExpander = {0};

/**
 * @brief   Product 16x2 character-LCD instance.
 *
 * @details Owns the bus and backlight interfaces populated by the concrete adapters. After LCD_Init(), the Display Render Service
 *          singleton will borrow this handle through DRS_Init().
 */
static LCD_Handle_t App_Lcd =
{
    ._rows           = LCD_2LINE,
    ._cols           = APP_LCD_COLUMN_COUNT,
    ._interface_mode = LCD_4BIT_MODE,
    ._font_dot_size  = LCD_5X8_FONT,
    ._initialized    = false
};

/**********************************************************************************************************************************
 Buzzer Driver and Sound Generator Service Objects
 **********************************************************************************************************************************/
/**
 * @brief   Platform PWM descriptor for the passive buzzer.
 *
 * @details Will be created from the CubeMX TIM3 context and APP_BUZZER_PWM_CHANNEL selection. Frequency and duty-cycle operations
 *          requested by the buzzer component are routed through this application-owned descriptor.
 */
static PWM_Handle_t App_BuzzerPwm = {0};

/**
 * @brief   Buzzer Driver instance used by the Sound Generator Service.
 *
 * @details Borrows App_BuzzerPwm after Buzzer_Init(). The Sound Generator Service singleton subsequently retains this driver
 *          reference through SGS_Init().
 */
static Buzzer_Handle_t App_Buzzer = {0};

/**********************************************************************************************************************************
 Low-Battery Status Indication Objects
 **********************************************************************************************************************************/
/**
 * @brief   Platform GPIO descriptor associated with the low-battery status LED.
 *
 * @details Retains the Platform representation of the configured low-battery LED pin and is borrowed by
 *          App_LowBatteryStatusLed after initialization.
 */
static GPIO_Handle_t App_LowBatteryStatusLedGpio = {0};

/**
 * @brief   LED Driver instance associated with low-battery indication.
 *
 * @details Borrows App_LowBatteryStatusLedGpio and provides non-blocking electrical LED operations to
 *          App_LowBatteryStatusIndication.
 */
static LED_Handle_t App_LowBatteryStatusLed = {0};

/**
 * @brief   Status Indication Service runtime associated with the low-battery LED.
 *
 * @details Retains the independent semantic-pattern, phase and timing state required to drive App_LowBatteryStatusLed.
 */
static SIS_Handle_t App_LowBatteryStatusIndication = {0};

/**********************************************************************************************************************************
 Lock Status Indication Objects
 **********************************************************************************************************************************/
/**
 * @brief   Platform GPIO descriptor associated with the lock-status LED.
 *
 * @details Retains the Platform representation of the configured lock-status LED pin and is borrowed by App_LockStatusLed after
 *          initialization.
 */
static GPIO_Handle_t App_LockStatusLedGpio = {0};

/**
 * @brief   LED Driver instance associated with lock-state indication.
 *
 * @details Borrows App_LockStatusLedGpio and provides non-blocking electrical LED operations to App_LockStatusIndication.
 */
static LED_Handle_t App_LockStatusLed = {0};

/**
 * @brief   Status Indication Service runtime associated with the lock-status LED.
 *
 * @details Retains the independent semantic-pattern, phase and timing state required to drive App_LockStatusLed.
 */
static SIS_Handle_t App_LockStatusIndication = {0};

/**********************************************************************************************************************************
 Lock Actuator Platform Object
 **********************************************************************************************************************************/
/**
 * @brief   Platform GPIO descriptor controlling the physical lock actuator.
 *
 * @details Is bound to the CubeMX LOCKER_PIN output and temporarily realizes the actuator boundary until a dedicated Lock Actuator
 *          Driver is introduced. GPIO reset is the safe locked state; GPIO set requests physical unlock.
 *
 * @note    Only serialized application action paths access this descriptor after initialization.
 */
static GPIO_Handle_t App_LockActuatorGpio = {0};

/**********************************************************************************************************************************
 Credential and Timeout Runtime Objects
 **********************************************************************************************************************************/
/**
 * @brief   Caller-owned transient copy of a complete credential candidate.
 *
 * @details Receives bounded copies from CES_GetCandidate() for authentication, first-entry staging and confirmation validation.
 *          Each synchronous consumer finishes before App_ClearRuntimeCandidate() explicitly erases the complete object.
 *
 * @warning Candidate bytes must never be logged, displayed or retained after their synchronous consumer completes.
 */
static CES_Candidate_t App_RuntimeCandidate = {0};

/**
 * @brief   Application-owned reference to the currently installed credential.
 *
 * @details Is loaded lazily from Credential Storage before the first authentication and is replaced only after
 *          CSS_SaveCredential() reports verified persistence of a newly registered credential. Authentication borrows the array
 *          synchronously and never owns it.
 *
 * @warning This longer-lived secret is explicitly erased before a controlled reset. It must never be logged, displayed or exposed
 *          through the public App Core interface.
 */
static uint8_t App_RuntimeCredential[AS_CREDENTIAL_LENGTH] = {0};

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
 Credential Entry and Authentication
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Processes one semantic input through the Credential Entry Service. */
static CES_Event_t App_ProcessCredentialInput(const CES_Input_t* Input);

/** @brief Copies and erases the CES candidate, authenticates it and returns the corresponding LCS event. */
static LCS_Event_t App_ProcessAuthentication(void);

/** @brief Explicitly erases the application-owned transient credential candidate. */
static void App_ClearRuntimeCandidate(void);

/*---------------------------------------------------------------------------------------------------------------------------------
 Application Timeout Management
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Starts or replaces the single active application timeout with the selected definition. */
static bool App_StartTimeout(App_TimeoutId_t Timeout);

/** @brief Cancels the active application timeout and restores its inactive sentinel state. */
static void App_CancelTimeout(void);

/** @brief Polls the active timeout and returns its semantic LCS event exactly once after expiration. */
static LCS_Event_t App_PollTimeout(void);

/*---------------------------------------------------------------------------------------------------------------------------------
 Lock Actuator Control
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Forces the lock-actuator GPIO into its safe locked state. */
static bool App_ForceLock(void);

/** @brief Requests actuator unlock after a finite unlock timeout has been established. */
static bool App_RequestUnlock(void);

/*---------------------------------------------------------------------------------------------------------------------------------
 Presentation Coordination
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Restores the locked-idle display, backlight and lock-status indication policy. */
static void App_SetLockedPresentation(void);

/** @brief Presents a newly opened normal Credential Entry Service session. */
static void App_SetCESPresentation(void);

/** @brief Presents installed-credential authorization for a credential-replacement request. */
static void App_SetCRSAuthPresentation(void);

/** @brief Presents the first-entry phase of credential registration. */
static void App_SetCRSFirstEntryPresentation(void);

/** @brief Presents the confirmation-entry phase while CRS retains the staged credential. */
static void App_SetCRSConfirmEntryPresentation(void);

/** @brief Presents successful credential persistence during the bounded saved-feedback interval. */
static void App_SetCRSSavedPresentation(void);

/*---------------------------------------------------------------------------------------------------------------------------------
 Lock Control Action and Event Orchestration
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Executes one semantic LCS action and returns any synchronous follow-up event it produces. */
static LCS_Event_t App_ExecuteAction(LCS_Action_t Action);

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

/*---------------------------------------------------------------------------------------------------------------------------------
 Fail-Safe Reset Endpoint
 ---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief Disables interrupts and requests the target system reset after fail-safe cleanup is complete. */
static void App_RequestControlledReset(void);

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
    if(PCF8574_Init(&App_LcdIoExpander,
                     APP_LCD_IO_EXPANDER_ADDRESS,
                     APP_LCD_IO_EXPANDER_I2C_CONTEXT)
                     != PCF8574_OPERATION_OK)
    {
        return false;
    }

    if(LCD_PCF8574_BusAdapterInit(&App_Lcd._bus,
                                  &App_LcdIoExpander)
                                  != LCD_BUS_OPERATION_OK)
    {
        return false;
    }

    if(PPWM_Create(&App_LcdBacklightPwm,
                    APP_LCD_BACKLIGHT_PWM_CONTEXT,
                    APP_LCD_BACKLIGHT_PWM_CHANNEL)
                    != PWM_OPERATION_OK)
    {
        return false;
    }

    if(PPWM_Init(&App_LcdBacklightPwm)
                  != PWM_OPERATION_OK)
    {
        return false;
    }

    if(PPWM_SetFrequency(&App_LcdBacklightPwm,
                          APP_LCD_BACKLIGHT_PWM_FREQUENCY)
                          != PWM_OPERATION_OK)
    {
        return false;
    }

    if(LCD_PWM_BacklightAdapterInit(&App_Lcd._backlight,
                                    &App_LcdBacklightPwm)
                                     != LCD_BACKLIGHT_OP_OK)
    {
        return false;
    }

    if(LCD_Init(&App_Lcd)
                 != LCD_OPERATION_OK)
    {
        return false;
    }

    if(LCD_SetBrightness(&App_Lcd,
                          APP_LCD_BACKLIGHT_BRIGHTNESS)
                          != LCD_OPERATION_OK)
    {
        return false;
    }

    if(DRS_Init(&App_Lcd) != DRS_OPERATION_OK)
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
    if(PGPIO_Init(&App_KeyboardRowGpios[0],
                   APP_KEYBOARD_ROW_0_GPIO_PORT,
                   APP_KEYBOARD_ROW_0_PIN_NUMBER)
                   != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&App_KeyboardRowGpios[1],
                   APP_KEYBOARD_ROW_1_GPIO_PORT,
                   APP_KEYBOARD_ROW_1_PIN_NUMBER)
                   != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&App_KeyboardRowGpios[2],
                   APP_KEYBOARD_ROW_2_GPIO_PORT,
                   APP_KEYBOARD_ROW_2_PIN_NUMBER)
                   != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&App_KeyboardRowGpios[3],
                   APP_KEYBOARD_ROW_3_GPIO_PORT,
                   APP_KEYBOARD_ROW_3_PIN_NUMBER)
                   != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&App_KeyboardColumnGpios[0],
                   APP_KEYBOARD_COL_0_GPIO_PORT,
                   APP_KEYBOARD_COL_0_PIN_NUMBER)
                   != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&App_KeyboardColumnGpios[1],
                   APP_KEYBOARD_COL_1_GPIO_PORT,
                   APP_KEYBOARD_COL_1_PIN_NUMBER)
                   != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&App_KeyboardColumnGpios[2],
                   APP_KEYBOARD_COL_2_GPIO_PORT,
                   APP_KEYBOARD_COL_2_PIN_NUMBER)
                   != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(PGPIO_Init(&App_KeyboardColumnGpios[3],
                   APP_KEYBOARD_COL_3_GPIO_PORT,
                   APP_KEYBOARD_COL_3_PIN_NUMBER)
                   != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(MK_Init(&App_Keyboard,
               &App_KeyboardConfig,
                App_KeyboardKeys)
                != MK_OPERATION_OK)
    {
        return false;
    }

    if(MK_GPIO_ScanAdapterInit(&App_Keyboard._scan_interface,
                               &App_KeyboardGpioScanAdapter)
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
    if(PPWM_Create(&App_BuzzerPwm,
                    APP_BUZZER_PWM_CONTEXT,
                    APP_BUZZER_PWM_CHANNEL)
                    != PWM_OPERATION_OK)
    {
        return false;
    }

    if(Buzzer_Init(&App_Buzzer,
                   &App_BuzzerPwm)
                    != BUZZER_OPERATION_OK)
    {
        return false;
    }

    if(SGS_Init(&App_Buzzer)
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
 * @note    The service instance owns only indication state; the GPIO and LED handles remain owned by the composition root.
 * @note    Initialization is fail-fast and does not roll back stages that completed before a later failure.
 *
 * @return  true  - When the GPIO, LED Driver and Status Indication Service are initialized successfully.
 * @return  false - When any required initialization operation fails.
 */
static bool App_InitLockStatusIndication(void)
{
    if(PGPIO_Init(&App_LockStatusLedGpio,
                   APP_LOCK_STATUS_LED_GPIO_PORT,
                   APP_LOCK_STATUS_LED_PIN_NUMBER)
                   != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(LED_Init(&App_LockStatusLed,
                &App_LockStatusLedGpio,
                 APP_LOCK_STATUS_LED_ACTIVE_LEVEL)
                 != LED_OPERATION_OK)
    {
        return false;
    }

    if(SIS_Init(&App_LockStatusIndication,
                &App_LockStatusLed)
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
 * @note    The service instance owns only indication state; the GPIO and LED handles remain owned by the composition root.
 * @note    Initialization is fail-fast and does not roll back stages that completed before a later failure.
 *
 * @return  true  - When the GPIO, LED Driver and Status Indication Service are initialized successfully.
 * @return  false - When any required initialization operation fails.
 */
static bool App_InitLowBatteryStatusIndication(void)
{
    if(PGPIO_Init(&App_LowBatteryStatusLedGpio,
                   APP_LOW_BATTERY_STATUS_LED_GPIO_PORT,
                   APP_LOW_BATTERY_STATUS_LED_PIN_NUMBER)
                   != GPIO_OPERATION_OK)
    {
        return false;
    }

    if(LED_Init(&App_LowBatteryStatusLed,
                &App_LowBatteryStatusLedGpio,
                 APP_LOW_BATTERY_STATUS_LED_ACTIVE_LEVEL)
                 != LED_OPERATION_OK)
    {
        return false;
    }

    if(SIS_Init(&App_LowBatteryStatusIndication,
                &App_LowBatteryStatusLed)
                 != SIS_OPERATION_OK)
    {
        return false;
    }

    return true;
}

/**
 * @brief   Initializes the temporary GPIO-based lock-actuator boundary in its safe state.
 *
 * @details Binds App_LockActuatorGpio to the CubeMX-configured LOCKER_PIN and immediately drives the output low. The low level is
 *          the product's current safe locked state and matches the startup level configured by CubeMX.
 *
 * @note    A dedicated Lock Actuator Driver shall eventually replace this direct Platform binding while preserving the same
 *          force-safe behavior and bounded-unlock contract.
 *
 * @return  true  - When the GPIO descriptor is initialized and the actuator is forced locked.
 * @return  false - When GPIO initialization or the safe-state write fails.
 */
static bool App_InitLockActuator(void)
{
    if(PGPIO_Init(&App_LockActuatorGpio,
                   APP_LOCK_ACTUATOR_GPIO_PORT,
                   APP_LOCK_ACTUATOR_PIN_NUMBER) != GPIO_OPERATION_OK)
    {
        return false;
    }

    return App_ForceLock();
}

/**
 * @brief   Performs one complete Matrix Keyboard Driver acquisition cycle.
 *
 * @details Delegates matrix scanning, debounce processing and pending-action retrieval to MK_Read() while retaining ownership of
 *          the keyboard instance inside the App Core composition root.
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

    return MK_Read(&App_Keyboard, Output);
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
 * @brief   Explicitly erases the application-owned candidate credential copy.
 *
 * @details Writes zero through a volatile byte pointer over the complete CES_Candidate_t object. Volatile access prevents the
 *          compiler from removing the erasure merely because the object is not read afterward.
 *
 * @warning This helper erases only App_RuntimeCandidate. CES_EndSession() remains responsible for erasing CES internal storage.
 */
static void App_ClearRuntimeCandidate(void)
{
    volatile uint8_t* candidate_bytes = (volatile uint8_t*)&App_RuntimeCandidate;

    for(size_t index = 0U; index < sizeof(App_RuntimeCandidate); index++)
    {
        candidate_bytes[index] = 0U;
    }
}

/**
 * @brief   Completes candidate transfer, authentication and credential erasure.
 *
 * @details Copies the complete candidate from CES, immediately ends the session to erase service-owned candidate storage,
 *          authenticates it against App_RuntimeCredential, and then explicitly erases the application candidate copy. The AS
 *          result is mapped into the semantic Lock Control event expected by the authoritative FSM.
 *
 * @note    App_ExecuteAction() ensures App_RuntimeCredential is valid before calling this helper. Candidate-copy or session-ending
 *          failures conservatively produce LCS_EVENT_AUTH_FAILURE; the installed runtime credential remains available for later
 *          authentication attempts.
 *
 * @return  LCS_EVENT_AUTH_SUCCESS - When AS authenticates the complete candidate.
 * @return  LCS_EVENT_AUTH_FAILURE - When candidate transfer, session cleanup or authentication does not succeed.
 */
static LCS_Event_t App_ProcessAuthentication(void)
{
    App_ClearRuntimeCandidate();

    if(CES_GetCandidate(&App_RuntimeCandidate) != CES_OPERATION_OK)
    {
        (void)CES_EndSession();
        App_ClearRuntimeCandidate();

        return LCS_EVENT_AUTH_FAILURE;
    }

    if(CES_EndSession() != CES_OPERATION_OK)
    {
        App_ClearRuntimeCandidate();

        return LCS_EVENT_AUTH_FAILURE;
    }

    AS_Result_t authentication_result = AS_Authenticate(App_RuntimeCandidate.Digits, App_RuntimeCredential);

    App_ClearRuntimeCandidate();

    return (authentication_result == AS_RESULT_AUTHENTICATED) ?
                                     LCS_EVENT_AUTH_SUCCESS   :
                                     LCS_EVENT_AUTH_FAILURE   ;
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
static bool App_StartTimeout(App_TimeoutId_t Timeout)
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
static void App_CancelTimeout(void)
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
 * @brief   Forces the lock-actuator output to its safe locked state.
 *
 * @details Drives the application-owned actuator GPIO low through the Platform interface. The operation is intentionally available
 *          independently from presentation services so display, LED or sound failures cannot prevent the safe output request.
 *
 * @return  true when the safe GPIO write succeeds; otherwise false.
 */
static bool App_ForceLock(void)
{
    return (PGPIO_Reset(&App_LockActuatorGpio) == GPIO_OPERATION_OK);
}

/**
 * @brief   Requests physical unlock through the temporary actuator GPIO boundary.
 *
 * @details Drives the actuator GPIO high only after the action dispatcher has established a finite unlock timeout. A future Lock
 *          Actuator Driver will replace this direct operation and enforce its own redundant maximum-energization deadline.
 *
 * @return  true when the unlock GPIO write succeeds; otherwise false.
 */
static bool App_RequestUnlock(void)
{
    return (PGPIO_Set(&App_LockActuatorGpio) == GPIO_OPERATION_OK);
}

/**
 * @brief   Restores the normal locked-idle presentation policy.
 *
 * @details Requests the blank locked-idle view, clears retained mask progress, synchronizes the LCD while its backlight is still
 *          available, turns the backlight off and selects the locked LED indication.
 *
 * @note    Presentation failures are treated as degraded UI behavior and cannot prevent the separate actuator safe-state request.
 * @note    This helper does not stop sound, allowing action-specific feedback to finish after the display returns to idle.
 */
static void App_SetLockedPresentation(void)
{
    (void)DRS_SetScreen(DRS_SCREEN_IDLE);
    (void)DRS_SetEnteredDigits(0U);
    (void)DRS_Update();
    (void)LCD_BacklightOff(&App_Lcd);
    (void)SIS_SetIndication(&App_LockStatusIndication, SIS_INDICATION_LOCKED);
}

/**
 * @brief   Presents a newly opened credential-entry session.
 *
 * @details Enables the LCD backlight, requests the empty masked-password screen, selects the normal locked LED baseline and emits a
 *          short keypress acknowledgement for the wake key. The initiating key is not passed to CES.
 */
static void App_SetCESPresentation(void)
{
    uint32_t current_time_ms = Platform_GetMillis();

    (void)SGS_Ring(SGS_RINGTONE_KEYPRESS, current_time_ms);
    (void)LCD_BacklightOn(&App_Lcd);
    (void)DRS_SetScreen(DRS_SCREEN_PASSWORD_ENTRY);
    (void)DRS_SetEnteredDigits(0U);
    (void)DRS_Update();
    (void)SIS_SetIndication(&App_LockStatusIndication, SIS_INDICATION_LOCKED);
}

/**
 * @brief   Presents installed-credential authorization for a replacement request.
 *
 * @details Acknowledges the registration command, enables the backlight, requests the fixed Access PIN prompt and clears retained
 *          mask progress before the authorization candidate is collected.
 */
static void App_SetCRSAuthPresentation(void)
{
    uint32_t current_time_ms = Platform_GetMillis();

    (void)SGS_Ring(SGS_RINGTONE_KEYPRESS, current_time_ms);
    (void)LCD_BacklightOn(&App_Lcd);
    (void)DRS_SetScreen(DRS_SCREEN_CREDENTIAL_REGISTER_AUTH);
    (void)DRS_SetEnteredDigits(0U);
    (void)DRS_Update();
}

/**
 * @brief   Presents a newly opened credential-register first-entry session.
 *
 * @details Acknowledges the phase transition, enables the backlight, requests the fixed Update PIN prompt and clears retained mask
 *          progress before the proposed credential is collected.
 */
static void App_SetCRSFirstEntryPresentation(void)
{
    uint32_t current_time_ms = Platform_GetMillis();

    (void)SGS_Ring(SGS_RINGTONE_KEYPRESS, current_time_ms);
    (void)LCD_BacklightOn(&App_Lcd);
    (void)DRS_SetScreen(DRS_SCREEN_CREDENTIAL_REGISTER_FIRST_ENTRY);
    (void)DRS_SetEnteredDigits(0U);
    (void)DRS_Update();
}

/**
 * @brief   Presents a newly opened credential-register confirmation-entry session.
 *
 * @details Acknowledges the phase transition, enables the backlight, requests the fixed Confirm PIN prompt and clears retained mask
 *          progress while CRS preserves the staged first entry.
 */
static void App_SetCRSConfirmEntryPresentation(void)
{
    uint32_t current_time_ms = Platform_GetMillis();

    (void)SGS_Ring(SGS_RINGTONE_KEYPRESS, current_time_ms);
    (void)LCD_BacklightOn(&App_Lcd);
    (void)DRS_SetScreen(DRS_SCREEN_CREDENTIAL_REGISTER_CONFIRM_ENTRY);
    (void)DRS_SetEnteredDigits(0U);
    (void)DRS_Update();
}

/**
 * @brief   Presents successful credential-persistence feedback.
 *
 * @details Enables the backlight, requests the fixed PIN-updated screen and starts the access-granted sound pattern. The action
 *          executor separately owns the bounded APP_TIMEOUT_CRS_SAVED interval.
 */
static void App_SetCRSSavedPresentation(void)
{
    uint32_t current_time_ms = Platform_GetMillis();

    (void)SGS_Ring(SGS_RINGTONE_ACCESS_GRANTED, current_time_ms);
    (void)LCD_BacklightOn(&App_Lcd);
    (void)DRS_SetScreen(DRS_SCREEN_CREDENTIAL_REGISTER_SAVED);
    (void)DRS_Update();
}

/**
 * @brief   Executes one semantic Lock Control action and optionally produces a synchronous follow-up event.
 *
 * @details Coordinates CES sessions, CRS staging and validation, CSS loading and persistence, authentication, timeout ownership,
 *          actuator safety and presentation according to the action selected by LCS. Synchronous outcomes are returned as
 *          follow-up events so they are dispatched only after the current LCS_Process() call has returned.
 *
 * @note    The function-local runtime_credential_valid flag prevents repeated Flash reads after the installed credential has been
 *          loaded or successfully replaced. The controlled-reset path erases the runtime reference and clears that flag.
 *
 * @param   Action - Semantic action returned by LCS_Process().
 *
 * @return  A synchronous semantic follow-up event, or LCS_EVENT_NONE when the action is complete.
 */
static LCS_Event_t App_ExecuteAction(LCS_Action_t Action)
{
    static bool runtime_credential_valid = false;

    uint32_t current_time_ms = Platform_GetMillis();

    switch(Action)
    {
        case LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION:

            (void)CRS_ClearStaging();
            App_ClearRuntimeCandidate();

            if(CES_BeginSession() != CES_OPERATION_OK ||
               !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                (void)CES_EndSession();
                (void)CRS_ClearStaging();

                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            App_SetCRSFirstEntryPresentation();

        break;

        case LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION:

            if(CES_RefreshSession() != CES_OPERATION_OK ||
               !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            (void)SGS_Ring(SGS_RINGTONE_ENTRY_INCOMPLETE, current_time_ms);

            App_SetCRSFirstEntryPresentation();

        break;

        case LCS_ACTION_END_CREDENTIAL_REGISTER_FIRST_ENTRY_SESSION:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)CRS_ClearStaging();

            App_ClearRuntimeCandidate();

            (void)App_ForceLock();

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_FIRST_TO_CONFIRM_ENTRY_SESSION:
        {
            App_CancelTimeout();
            App_ClearRuntimeCandidate();

            if(CES_GetCandidate(&App_RuntimeCandidate) != CES_OPERATION_OK)
            {
                goto controlled_reset;
            }

            CRS_OpStatus_t staging_status = CRS_StageCredential(App_RuntimeCandidate.Digits);

            App_ClearRuntimeCandidate();

            if(staging_status != CRS_OPERATION_OK)
            {
                goto controlled_reset;
            }

            if(CES_RefreshSession() != CES_OPERATION_OK ||
               !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                (void)CES_EndSession();
                (void)CRS_ClearStaging();

                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            App_SetCRSConfirmEntryPresentation();
        }
        break;

        case LCS_ACTION_REFRESH_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION:

            if(CES_RefreshSession() != CES_OPERATION_OK ||
               !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            (void)SGS_Ring(SGS_RINGTONE_ENTRY_INCOMPLETE, current_time_ms);

            App_SetCRSConfirmEntryPresentation();

        break;

        case LCS_ACTION_END_CREDENTIAL_REGISTER_CONFIRM_ENTRY_SESSION:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)CRS_ClearStaging();

            App_ClearRuntimeCandidate();

            (void)App_ForceLock();

            (void)SGS_Ring(SGS_RINGTONE_ERROR, current_time_ms);

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_BEGIN_CREDENTIAL_REGISTER_SAVING_SESSION:

            App_CancelTimeout();

        break;

        case LCS_ACTION_END_CREDENTIAL_REGISTER_SAVING_SESSION:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)CRS_ClearStaging();

            App_ClearRuntimeCandidate();

            (void)App_ForceLock();

            (void)SIS_SetIndication(&App_LockStatusIndication, SIS_INDICATION_LOCKED);

            App_SetCRSSavedPresentation();

            (void)SGS_Ring(SGS_RINGTONE_ACCESS_GRANTED, current_time_ms);

            if(!App_StartTimeout(APP_TIMEOUT_CRS_SAVED))
            {
                return LCS_EVENT_CREDENTIAL_REGISTER_DONE;
            }

        break;

        case LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_TO_REGISTER_SESSION:

            if(CES_RefreshSession() != CES_OPERATION_OK ||
               !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                (void)CES_EndSession();

                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            App_SetCRSAuthPresentation();

        break;

        case LCS_ACTION_BEGIN_CREDENTIAL_ENTRY_SESSION:

            if(CES_BeginSession() != CES_OPERATION_OK ||
               !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                (void)CES_EndSession();

                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            App_SetCESPresentation();

        break;

        case LCS_ACTION_REFRESH_CREDENTIAL_ENTRY_SESSION:

            if(CES_RefreshSession() != CES_OPERATION_OK ||
               !App_StartTimeout(APP_TIMEOUT_CREDENTIAL_ENTRY))
            {
                App_CancelTimeout();

                return LCS_EVENT_CREDENTIAL_CANCELLED;
            }

            (void)SGS_Ring(SGS_RINGTONE_ENTRY_INCOMPLETE, current_time_ms);

            App_SetCESPresentation();

        break;

        case LCS_ACTION_END_CREDENTIAL_ENTRY_SESSION:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)App_ForceLock();
            (void)SIS_SetIndication(&App_LockStatusIndication, SIS_INDICATION_LOCKED);

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_REQUEST_AUTHENTICATION:

            App_CancelTimeout();

            if(!runtime_credential_valid)
            {
                if(CSS_GetCredential(App_RuntimeCredential) != CSS_OPERATION_OK)
                {
                    goto controlled_reset;
                }

                runtime_credential_valid = true;
            }

            return App_ProcessAuthentication();

        case LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STAGES_VALIDATION:
        {
            App_CancelTimeout();
            App_ClearRuntimeCandidate();

            if(CES_GetCandidate(&App_RuntimeCandidate) != CES_OPERATION_OK)
            {
                goto controlled_reset;
            }

            CRS_ValidationResult_t validation_result =
                CRS_ValidateConfirmation(App_RuntimeCandidate.Digits);

            App_ClearRuntimeCandidate();

            if(validation_result == CRS_VALIDATION_MATCH)
            {
                return LCS_EVENT_STAGING_VALIDATION_SUCCESS;
            }

            if(validation_result == CRS_VALIDATION_MISMATCH)
            {
                return LCS_EVENT_STAGING_VALIDATION_FAILURE;
            }

            goto controlled_reset;
        }

        case LCS_ACTION_REQUEST_CREDENTIAL_REGISTER_STORAGE:
        {
            uint8_t temporary_credential[CRS_CREDENTIAL_LENGTH] = {0U};
            LCS_Event_t storage_event = LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_FAILURE;

            App_CancelTimeout();

            if(CRS_GetValidatedCredential(temporary_credential) == CRS_OPERATION_OK &&
               CSS_SaveCredential(temporary_credential) == CSS_OPERATION_OK)
            {
                for(size_t index = 0U; index < sizeof(App_RuntimeCredential); index++)
                {
                    App_RuntimeCredential[index] = temporary_credential[index];
                }

                runtime_credential_valid = true;
                storage_event = LCS_EVENT_CREDENTIAL_REGISTER_STORAGE_SUCCESS;
            }

            (void)CRS_ClearStaging();

            volatile uint8_t* temporary_bytes = (volatile uint8_t*)temporary_credential;

            for(size_t index = 0U; index < sizeof(temporary_credential); index++)
            {
                temporary_bytes[index] = 0U;
            }

            return storage_event;
        }

        case LCS_ACTION_GRANT_ACCESS_UNLOCK:

            if(!App_StartTimeout(APP_TIMEOUT_UNLOCK))
            {
                (void)App_ForceLock();

                return LCS_EVENT_UNLOCK_TIMEOUT;
            }

            if(!App_RequestUnlock())
            {
                App_CancelTimeout();
                (void)App_ForceLock();

                return LCS_EVENT_UNLOCK_TIMEOUT;
            }

            (void)LCD_BacklightOn(&App_Lcd);
            (void)DRS_SetScreen(DRS_SCREEN_ACCESS_GRANTED);
            (void)DRS_Update();
            (void)SIS_SetIndication(&App_LockStatusIndication, SIS_INDICATION_ACCESS_GRANTED);
            (void)SGS_Ring(SGS_RINGTONE_ACCESS_GRANTED, current_time_ms);

        break;

        case LCS_ACTION_DENY_ACCESS:

            (void)App_ForceLock();

            if(!App_StartTimeout(APP_TIMEOUT_ACCESS_DENIED))
            {
                return LCS_EVENT_DENIED_ACCESS_TIMEOUT;
            }

            (void)LCD_BacklightOn(&App_Lcd);
            (void)DRS_SetScreen(DRS_SCREEN_ACCESS_DENIED);
            (void)DRS_Update();
            (void)SIS_SetIndication(&App_LockStatusIndication, SIS_INDICATION_ACCESS_DENIED);
            (void)SGS_Ring(SGS_RINGTONE_ERROR, current_time_ms);

        break;

        case LCS_ACTION_ENTER_LOCKOUT:

            (void)App_ForceLock();

            if(!App_StartTimeout(APP_TIMEOUT_LOCKOUT))
            {
                return LCS_EVENT_LOCKOUT_TIMEOUT;
            }

            (void)LCD_BacklightOn(&App_Lcd);
            (void)DRS_SetScreen(DRS_SCREEN_LOCKOUT);
            (void)DRS_Update();
            (void)SIS_SetIndication(&App_LockStatusIndication, SIS_INDICATION_LOCKOUT_ENTRY);
            (void)SGS_Ring(SGS_RINGTONE_LOCKOUT, current_time_ms);

        break;

        case LCS_ACTION_RETURN_TO_LOCKED_FROM_ENTRY_TIMEOUT:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)App_ForceLock();
            (void)SGS_Ring(SGS_RINGTONE_ENTRY_TIMEOUT, current_time_ms);

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_RETURN_TO_LOCKED:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)App_ForceLock();

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_RETURN_TO_LOCKED_FROM_CREDENTIAL_REGISTER_SESSION:

            App_CancelTimeout();

            (void)CES_EndSession();
            (void)CRS_ClearStaging();
            App_ClearRuntimeCandidate();
            (void)App_ForceLock();

            App_SetLockedPresentation();

        break;

        case LCS_ACTION_REQUEST_CONTROLLED_RESET:

            goto controlled_reset;

        case LCS_ACTION_NONE:
        default:

        break;
    }

    return LCS_EVENT_NONE;

controlled_reset:

    /* CRS integration faults share the same fail-safe cleanup as an LCS-requested reset. */
    runtime_credential_valid = false;

    App_CancelTimeout();

    (void)CES_EndSession();
    (void)CRS_ClearStaging();
    App_ClearRuntimeCandidate();

    volatile uint8_t* runtime_credential_bytes = (volatile uint8_t*)App_RuntimeCredential;

    for(size_t index = 0U; index < sizeof(App_RuntimeCredential); index++)
    {
        runtime_credential_bytes[index] = 0U;
    }

    (void)App_ForceLock();
    (void)SGS_Stop();
    (void)LCD_BacklightOff(&App_Lcd);

    App_RequestControlledReset();

    return LCS_EVENT_NONE;
}

/**
 * @brief   Serially dispatches one event and every bounded synchronous follow-up through Lock Control.
 *
 * @details Calls LCS_Process(), executes its returned action, and repeats while authentication, registration validation, storage or
 *          fail-safe execution produces a synchronous follow-up event. A small fixed bound prevents an accidental action cycle
 *          from monopolizing the caller. Exceeding that bound cancels timing, ends CES, forces the actuator safe and requests a
 *          controlled reset.
 *
 * @param   Event - First semantic event to dispatch; sentinels and out-of-range values are ignored.
 *
 * @note    All event processing and synchronous follow-up actions complete in the serialized context that initiated dispatch,
 *          whether App_ReadInput() or App_Dispatch().
 * @warning The dispatch-depth fallback requests reset directly and therefore does not execute the full credential erasure owned
 *          by LCS_ACTION_REQUEST_CONTROLLED_RESET before the hardware reset takes effect.
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
        (void)App_ForceLock();
        (void)SGS_Stop();
        (void)LCD_BacklightOff(&App_Lcd);

        App_RequestControlledReset();
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
    (void)SIS_Update(&App_LockStatusIndication);
    (void)SIS_Update(&App_LowBatteryStatusIndication);
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
    (void)LCD_BacklightOn(&App_Lcd);
    (void)DRS_SetScreen(DRS_SCREEN_ACCESS_DENIED);
    (void)DRS_Update();
    (void)SGS_Ring(SGS_RINGTONE_ERROR, current_time_ms);
}

/**
 * @brief   Executes the target-controlled reset endpoint after outputs are safe.
 *
 * @details Disables interrupt activity and invokes the CMSIS system-reset request. Callers must force the actuator safe, erase
 *          credentials, stop sound and disable the LCD backlight before entering this endpoint.
 *
 * @note    This function is not expected to return on the STM32 target.
 */
static void App_RequestControlledReset(void)
{
    __disable_irq();
    NVIC_SystemReset();

    for(;;)
    {
        /* Wait for the reset request to take effect. */
    }
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Initializes the complete electronic-lock application object graph.
 *
 * @details Initializes the actuator safe path first, followed by the LCD/render path, keyboard, buzzer/sound path and both
 *          indication instances. After every dependency succeeds, CSS availability selects either the first-registration route
 *          through LCS_EVENT_CREDENTIAL_NOT_REGISTERED or normal activation through LCS_EVENT_INIT_OK and locked-idle presentation.
 *
 * @note    CSS_HasCredential() provides only availability. The installed credential is copied into App_RuntimeCredential lazily
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
    bool initialized = App_InitLockActuator()               &&
                       App_InitLcd()                        &&
                       App_InitKeyboard()                   &&
                       App_InitBuzzer()                     &&
                       App_InitLockStatusIndication()       &&
                       App_InitLowBatteryStatusIndication() ;

    if(!initialized)
    {
        (void)App_ForceLock();

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

        App_SetLockedPresentation();
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
