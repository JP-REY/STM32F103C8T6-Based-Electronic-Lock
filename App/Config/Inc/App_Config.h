/**********************************************************************************************************************************
 * @file    App_Config.h
 * @brief   Product configuration and application-owned runtime-object registry.
 *
 * @details Centralizes the board bindings, product policy constants, timeout data model and statically allocated object graph used
 *          by the electronic-lock application. App_Config.c owns the concrete storage; App_Core.c initializes the objects and
 *          coordinates input and timing; App_Executor.c consumes the same registry while executing semantic Lock Control actions;
 *          App_ConfigHalCallbacks.c borrows the exit-button instance only to publish PB10 interrupt activity.
 *
 *          This header is internal to the App layer. It intentionally exposes component, service, Platform and CubeMX types to the
 *          application implementation modules, but none of those dependencies cross the public App_Core.h boundary.
 *
 * @note    App_GetRuntimeInstances() returns borrowed pointers with static storage duration. Callers must never free or replace
 *          the pointed-to objects.
 *
 * @author  Joao Pedro Rey
 * @version 1.1.0
 * @date    2026-08-25
 **********************************************************************************************************************************/

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifdef __cplusplus
static "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
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
#include "LockActuator_Driver.h"
#include "DoorSensor_Driver.h"
#include "ExitButton_Driver.h"

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
 Defines
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

/** @brief STM32 GPIO port connected to the physical lock actuator control input. */
#define APP_LOCK_ACTUATOR_GPIO_PORT             (GPIOB)

/** @brief Zero-based STM32 GPIO pin number connected to the physical lock actuator control input. */
#define APP_LOCK_ACTUATOR_PIN_NUMBER            (8U)

/** @brief Electrical level interpreted by the Lock Actuator Driver as the locked command. */
#define APP_LOCK_ACTUATOR_ACTIVE_LEVEL          (LOCK_ACTUATOR_ACTIVE_LEVEL_LOW)

/** @brief STM32 GPIO port connected to the physical door sensor input. */
#define APP_DOOR_SENSOR_GPIO_PORT               (GPIOB)

/** @brief Zero-based STM32 GPIO pin number connected to the physical door sensor input. */
#define APP_DOOR_SENSOR_PIN_NUMBER              (0U)

/** @brief Electrical level interpreted by the Door Sensor Driver as its active contact state. */
#define APP_DOOR_SENSOR_ACTIVE_LEVEL            (DOOR_SENSOR_ACTIVE_LEVEL_LOW)

/** @brief STM32 GPIO port connected to the physical exit button input. */
#define APP_EXIT_BUTTON_GPIO_PORT               (GPIOB)

/** @brief Zero-based STM32 GPIO pin number connected to the physical exit button input. */
#define APP_EXIT_BUTTON_PIN_NUMBER              (10U)

/** @brief Electrical level interpreted by the Exit Button Driver as a pressed button. */
#define APP_EXIT_BUTTON_ACTIVE_LEVEL            (EXIT_BUTTON_ACTIVE_LEVEL_LOW)

/** @brief Required quiet interval after the most recent exit-button edge, in milliseconds. */
#define APP_EXIT_BUTTON_DEBOUNCE_TIME_MS        (20U)

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

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
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
 Types
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

/**
 * @brief   Registry of every stateful object owned by the App configuration module.
 *
 * @details Groups borrowed references to the immutable keyboard policy, Platform descriptors, concrete adapters, component
 *          handles, instance-based service runtimes and credential buffers required by App Core and App Executor. The registry
 *          separates storage ownership from initialization and orchestration without using dynamic allocation.
 *
 * @note    The registry does not indicate initialization state. App_Init() remains responsible for initializing the dependency
 *          graph before any runtime entry point or action execution is allowed.
 * @warning Credential pointers expose application-internal secret storage and must never be logged, displayed or exported through
 *          the public application API.
 */
typedef struct
{
    const MK_KeyCode_t*    Keyboard_KeyMap;              /*< Immutable row-major logical key map.                         */
    const MK_Config_t*     Keyboard_Config;              /*< Immutable keyboard dimensions, level and debounce policy.   */
    GPIO_Handle_t*         Keyboard_Rows;                /*< Array of APP_KEYBOARD_ROW_COUNT row GPIO descriptors.        */
    GPIO_Handle_t*         Keyboard_Cols;                /*< Array of APP_KEYBOARD_COLUMN_COUNT column GPIO descriptors.  */
    MK_GPIO_ScanAdapter_t* Keyboard_Gpio_ScanAdapter;    /*< Concrete GPIO scan-adapter context.                          */
    MK_Key_t*              Keyboard_Keys;                /*< Per-key debounce and action runtime array.                    */
    MK_Handle_t*           Keyboard;                     /*< Matrix Keyboard Driver instance.                              */
    PWM_Handle_t*          Lcd_BacklightPwm;             /*< Platform PWM descriptor for the LCD backlight.                */
    PCF8574_Handle_t*      Lcd_IoExpander;               /*< PCF8574 instance that implements the LCD data bus.             */
    LCD_Handle_t*          Lcd;                          /*< Product character-LCD instance.                               */
    PWM_Handle_t*          Buzzer_Pwm;                   /*< Platform PWM descriptor for the passive buzzer.               */
    Buzzer_Handle_t*       Buzzer;                       /*< Buzzer Driver instance borrowed by Sound Generator.           */
    GPIO_Handle_t*         LowBattery_Status_Led_Gpio;   /*< Platform GPIO descriptor for the low-battery LED.             */
    LED_Handle_t*          LowBattery_Status_Led;        /*< LED Driver instance for low-battery indication.               */
    SIS_Handle_t*          LowBattery_Status_Indication; /*< Independent low-battery Status Indication runtime.            */
    GPIO_Handle_t*         Lock_Status_Led_Gpio;         /*< Platform GPIO descriptor for the lock-status LED.             */
    LED_Handle_t*          Lock_Status_Led;              /*< LED Driver instance for lock-state indication.                */
    SIS_Handle_t*          Lock_Status_Indication;       /*< Independent lock-state Status Indication runtime.             */
    GPIO_Handle_t*         Lock_Actuator_Gpio;           /*< Platform GPIO descriptor for the physical lock actuator.      */
    LockActuator_Handle_t* Lock_Actuator;                /*< Polarity-independent lock-actuator component instance.        */
    GPIO_Handle_t*         Door_Sensor_Gpio;             /*< Platform GPIO descriptor for the physical door contact.       */
    DoorSensor_Handle_t*   Door_Sensor;                  /*< Polarity-independent door-sensor component instance.          */
    GPIO_Handle_t*         Exit_Button_Gpio;             /*< Platform GPIO descriptor for the physical exit button.        */
    ExitButton_Handle_t*   Exit_Button;                  /*< Interrupt-oriented debounced exit-button component instance.  */
    CES_Candidate_t*       Runtime_Candidate;            /*< Short-lived credential candidate transfer buffer.             */
    uint8_t*               Runtime_Credential;           /*< Installed credential retained for authentication.             */

}App_RuntimeInstances_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**
 * @brief   Active view of the application-owned runtime registry.
 *
 * @details App_Init() assigns this pointer from App_GetRuntimeInstances() before initializing the dependency graph. App Core and
 *          App Executor then use it to share the same statically allocated instances without exposing those objects publicly.
 *
 * @note    NULL before App_Init() binds the registry. Runtime application functions require successful initialization first.
 */
extern const App_RuntimeInstances_t* App_Instance;

/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
const App_RuntimeInstances_t* App_GetRuntimeInstances(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
