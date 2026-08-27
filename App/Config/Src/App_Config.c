/**********************************************************************************************************************************
 * @file    App_Config.c
 * @brief   Static product configuration and application runtime-object storage.
 *
 * @details Defines the immutable keyboard policy and owns every Platform descriptor, adapter context, component handle,
 *          instance-based service runtime and credential buffer composed by the App layer. A single immutable registry exposes
 *          borrowed pointers to App_Core.c, App_Executor.c and the HAL callback bridge while preserving static lifetime and
 *          centralized ownership.
 *
 *          This module performs no hardware initialization and executes no business workflow. App_Init() binds and initializes the
 *          object graph; App_ExecuteAction() operates on it in response to semantic Lock Control actions.
 *
 * @note    No dynamic allocation is used. All referenced storage remains valid for the complete firmware lifetime.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    2026-08-25
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "App_Config.h"

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
 * @details App Core binds this descriptor to the CubeMX LOCK_ACTUATOR output before passing it to App_LockActuator. PB8 LOW is the
 *          current product safe locked request; PB8 HIGH requests physical unlock. App Executor still uses the same descriptor
 *          through the Door Control Service after initialization.
 *
 * @note    Only serialized application action paths access this descriptor after initialization.
 */
static GPIO_Handle_t App_LockActuatorGpio = {0};

/**
 * @brief   Lock Actuator Driver instance for the product's physical locking output.
 *
 * @details Borrows App_LockActuatorGpio after App Core initializes both objects. The handle normalizes the active-low locked command
 *          but does not establish an unlock timeout or confirm mechanical bolt position.
 *
 * @note    Owned by App Config for the complete firmware lifetime and borrowed through App_Instances.
 */
static LockActuator_Handle_t App_LockActuator;

/**********************************************************************************************************************************
 Door Sensor Platform Object
 **********************************************************************************************************************************/
/**
 * @brief   Platform GPIO descriptor connected to the physical door-contact sensor.
 *
 * @details App Core binds this descriptor to the CubeMX DOOR_SENSOR input on PA11 and passes it to App_DoorSensor. CubeMX configures
 *          the pin as a pull-up rising-and-falling EXTI input; the driver interprets LOW as the active contact state.
 *
 * @note    The Door Control Service consumes debounced events and assigns product meaning to the active contact.
 */
static GPIO_Handle_t App_DoorSensorGpio = {0};

/**
 * @brief   Door Sensor Driver instance for the product's physical door contact.
 *
 * @details Retains the active-low interpretation, debounce policy and interrupt handoff state. The HAL callback notifies this
 *          instance and the Door Control Service processes pending edges in application context.
 *
 * @note    Owned by App Config for the complete firmware lifetime and borrowed through App_Instances.
 */
static DoorSensor_Handle_t App_DoorSensor;

/**
 * @brief   Application-owned output slot for the latest debounced door-sensor transition.
 *
 * @details DoorSensor_Update() overwrites this slot on every Door Control Service update. Events are consumed synchronously by App
 *          Core and are not retained in a queue.
 */
static DoorSensor_Event_t App_DoorSensorEvent;

/**
 * @brief   Application-owned output slot for synchronous door-sensor status acquisition.
 *
 * @details Receives the instantaneous normalized door-contact status returned by
 *          DCS_GetSensorStatus() during Door Control confirmation processing.
 *
 * @note    Owned by App Config for the complete firmware lifetime and borrowed
 *          through App_Instances.
 */
static DCS_SensorStatus_t App_DoorSensorStatus;

/**********************************************************************************************************************************
 Exit Button Platform Object
 **********************************************************************************************************************************/
/**
 * @brief   Platform GPIO descriptor connected to the physical request-to-exit button.
 *
 * @details App Core binds this descriptor to the CubeMX EXIT_BUTTON input on PA0 and passes it to App_ExitButton. CubeMX configures
 *          the pin with a pull-up and rising/falling EXTI so press and release edges reach the HAL callback bridge.
 *
 * @note    The interrupt callback publishes edge timestamps only; debounced runtime processing belongs to the Door Control Service.
 */
static GPIO_Handle_t App_ExitButtonGpio = {0};

/**
 * @brief   Exit Button Driver instance for the product's request-to-exit input.
 *
 * @details Retains the active-low interpretation, 20 ms debounce policy and interrupt handoff state. The HAL callback notifies this
 *          instance; the Door Control Service calls ExitButton_Update() and interprets validated events.
 *
 * @note    Owned by App Config for the complete firmware lifetime and borrowed through App_Instances.
 */
static ExitButton_Handle_t App_ExitButton;

/**
 * @brief   Runtime event produced by the Exit Button Driver.
 *
 * @details Stores the latest debounced event reported by App_ExitButton during
 *          runtime processing. The Door Control Service uses this event to
 *          determine whether a validated request-to-exit action occurred.
 *
 * @note    Owned by App Config for the complete firmware lifetime and borrowed
 *          through App_Instances.
 */
static ExitButton_Event_t App_ExitButtonEvent;

/**********************************************************************************************************************************
 Credential and Timeout Runtime Objects
 **********************************************************************************************************************************/
/**
 * @brief   Caller-owned transient copy of a complete credential candidate.
 *
 * @details Receives bounded copies from CES_GetCandidate() for authentication, first-entry staging and confirmation validation.
 *          Each synchronous consumer finishes before App_ClearRuntime_Candidate() explicitly erases its credential digit array.
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
 * @brief   Immutable registry of application-owned runtime objects.
 *
 * @details Connects each registry role to its static storage above. The registry itself is read-only so consumers cannot replace
 *          bindings after composition; referenced handles remain mutable and are initialized by App_Init().
 *
 * @warning Runtime_Candidate and Runtime_Credential reference secret-bearing buffers. Consumers must follow the explicit erasure
 *          rules implemented by App Executor.
 */
const App_RuntimeInstances_t App_Instances = 
{
    .Keyboard_KeyMap                = App_KeyboardKeyMap,
    .Keyboard_Config                = &App_KeyboardConfig,
    .Keyboard_Rows                  = App_KeyboardRowGpios,
    .Keyboard_Cols                  = App_KeyboardColumnGpios,
    .Keyboard_Gpio_ScanAdapter      = &App_KeyboardGpioScanAdapter,
    .Keyboard_Keys                  = App_KeyboardKeys,
    .Keyboard                       = &App_Keyboard,
    .Lcd_BacklightPwm               = &App_LcdBacklightPwm,
    .Lcd_IoExpander                 = &App_LcdIoExpander,
    .Lcd                            = &App_Lcd,
    .Buzzer_Pwm                     = &App_BuzzerPwm,
    .Buzzer                         = &App_Buzzer,
    .LowBattery_Status_Led_Gpio     = &App_LowBatteryStatusLedGpio,
    .LowBattery_Status_Led          = &App_LowBatteryStatusLed,
    .LowBattery_Status_Indication   = &App_LowBatteryStatusIndication,
    .Lock_Status_Led_Gpio           = &App_LockStatusLedGpio,
    .Lock_Status_Led                = &App_LockStatusLed,
    .Lock_Status_Indication         = &App_LockStatusIndication,
    .Lock_Actuator_Gpio             = &App_LockActuatorGpio,
    .Lock_Actuator                  = &App_LockActuator,
    .Door_Sensor_Gpio               = &App_DoorSensorGpio,
    .Door_Sensor                    = &App_DoorSensor,
    .Door_Sensor_Event              = &App_DoorSensorEvent,
    .Door_Sensor_Status             = &App_DoorSensorStatus,
    .Exit_Button_Gpio               = &App_ExitButtonGpio,
    .Exit_Button                    = &App_ExitButton,
    .Exit_Button_Event              = &App_ExitButtonEvent,
    .Runtime_Candidate              = &App_RuntimeCandidate,
    .Runtime_Credential             = App_RuntimeCredential,
};

/**********************************************************************************************************************************
 Private Function Prototypes
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**
 * @brief   Returns the application runtime-object registry.
 *
 * @details Supplies the composition root with a stable, immutable view of all application-owned objects. The function has no
 *          initialization side effects and always returns the same address.
 *
 * @return  Borrowed pointer to the static App_Instances registry.
 */
const App_RuntimeInstances_t* App_GetRuntimeInstances(void)
{
    return (&App_Instances);
}
