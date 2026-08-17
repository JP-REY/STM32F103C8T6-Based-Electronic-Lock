<h1 align="left">LCD Driver</h1>

<p align="left">
  <big>
    Hardware-independent driver for HD44780-compatible character LCD modules,<br>
    designed for portable embedded systems.
  </big>
</p>

---
## Table of Contents

* [1. Overview](#1-overview)
* [2. Features](#2-features)
* [3. Architecture](#3-architecture)

  * [3.1 Bus Interface](#31-bus-interface)
  * [3.2 PCF8574 Bus Adapter](#32-pcf8574-bus-adapter)
  * [3.3 Backlight Interface](#33-backlight-interface)
  * [3.4 PWM Backlight Adapter](#34-pwm-backlight-adapter)
* [4. Directory Structure](#4-directory-structure)
* [5. Driver Responsibilities](#5-driver-responsibilities)
* [6. Dependencies](#6-dependencies)
* [7. Data Structures](#7-data-structures)

  * [7.1 LCD Handle](#71-LCD-handle)
  * [7.2 Operation Status](#72-operation-status)
  * [7.3 Interface Mode](#73-interface-mode)
  * [7.4 Character Font](#74-character-font)
  * [7.5 Display Line Configuration](#75-display-line-configuration)
  * [7.6 Bus Interface](#76-bus-interface)
  * [7.7 Backlight Interface](#77-backlight-interface)
* [8. API Reference](#8-api-reference)

  * [8.1 LCD_Init](#81-LCD_init)
  * [8.2 LCD_Clear](#82-LCD_clear)
  * [8.3 LCD_Home](#83-LCD_home)
  * [8.4 Display Control](#84-display-control)
  * [8.5 Cursor and Display Shift Control](#85-cursor-and-display-shift-control)
  * [8.6 LCD_SetCursor](#86-LCD_setcursor)
  * [8.7 LCD_WriteChar](#87-LCD_writechar)
  * [8.8 LCD_WriteString](#88-LCD_writestring)
  * [8.9 LCD_PrintLine](#89-LCD_printline)
  * [8.10 LCD_ClearLine](#810-LCD_clearline)
  * [8.11 LCD_CreateChar](#811-LCD_createchar)
  * [8.12 LCD_WriteCustomChar](#812-LCD_writecustomchar)
  * [8.13 LCD_BacklightOn](#813-LCD_backlighton)
  * [8.14 LCD_BacklightOff](#814-LCD_backlightoff)
  * [8.15 LCD_SetBrightness](#815-LCD_setbrightness)
* [9. Operation Flow](#9-operation-flow)

  * [9.1 Initialization Flow](#91-initialization-flow)
* [10. Usage Example](#10-usage-example)

  * [10.1 Driver Objects](#101-driver-objects)
  * [10.2 Initialization](#102-initialization)
  * [10.3 Write Text](#103-write-text)
  * [10.4 Write Character](#104-write-character)
  * [10.5 Clear Display](#105-clear-display)
  * [10.6 Print Text on Specific Line](#106-print-text-on-specific-line)
  * [10.7 Clear Specific Line](#107-clear-specific-line)
  * [10.8 Backlight Control](#108-backlight-control)
  * [10.9 Custom Characters](#109-custom-characters)
* [11. Design Decisions](#11-design-decisions)

  * [11.1 Separation Between Driver and Hardware](#111-separation-between-driver-and-hardware)
  * [11.2 Nibble-Based Bus Interface](#112-nibble-based-bus-interface)
  * [11.3 Internal State Management](#113-internal-state-management)
  * [11.4 Custom Character Tracking](#114-custom-character-tracking)
  * [11.5 Delay-Based Synchronization](#115-delay-based-synchronization)
  * [11.6 Timing Abstraction](#116-timing-abstraction)
* [12. Error Handling](#12-error-handling)

  * [12.1 Operation Failure Conditions](#121-operation-failure-conditions)
  * [12.2 Bus Errors](#122-bus-errors)
  * [12.3 Backlight Errors](#123-backlight-errors)
  * [12.4 Error Information](#124-error-information)
* [13. Usage Constraints](#13-usage-constraints)

  * [13.1 Initialization](#131-initialization)
  * [13.2 Interface Mode](#132-interface-mode)
  * [13.3 Timing Dependency](#133-timing-dependency)
  * [13.4 Display Geometry](#134-display-geometry)
  * [13.5 String Handling](#135-string-handling)
  * [13.6 Custom Characters](#136-custom-characters)
  * [13.7 Backlight Availability](#137-backlight-availability)
  * [13.8 Multi-Instance Usage](#138-multi-instance-usage)
  * [13.9 Blocking Behavior](#139-blocking-behavior)
* [14. Applications](#14-applications)
* [15. Limitations](#15-limitations)

  * [15.1 Interface Mode](#151-interface-mode)
  * [15.2 Busy Flag](#152-busy-flag)
  * [15.3 Display Shift API](#153-display-shift-api)
  * [15.4 Multi-Instance Support](#154-multi-instance-support)
  * [15.5 Available Adapters](#155-available-adapters)
* [16. Future Improvements](#16-future-improvements)
* [17. License](#17-license)

---
## 1. Overview

- The driver implements the LCD controller communication protocol, abstracting bus communication and backlight control through configurable interfaces. This allows reuse with different hardware implementations such as direct GPIO, I2C expanders, PWM-controlled backlights, and other peripherals.

---

## 2. Features

- Complete LCD controller initialization.
- 4-bit communication mode support.
- Command and data transmission.
- Display control:
  - Display ON/OFF.
  - Cursor visibility.
  - Cursor blinking.
- Cursor direction configuration:
  - Increment mode.
  - Decrement mode.
- Automatic display shift configuration.
- Cursor positioning using row and column coordinates.
- Character writing.
- String writing.
- Line-specific text printing.
- Line clearing.
- Custom character creation using CGRAM.
- Backlight control:
  - Enable/disable.
  - Brightness adjustment.
- Hardware-independent architecture:
  - Bus Interface abstraction.
  - Backlight Interface abstraction.
- Provided adapters:
  - PCF8574 I2C bus adapter.
  - PWM timer backlight adapter.

---

## 3. Architecture

The driver follows a layered architecture where hardware dependencies are isolated through abstraction interfaces.

The LCD driver never accesses hardware peripherals directly. All communication with the LCD controller is performed through the Bus Interface, while backlight control is performed through the Backlight Interface.

This design allows the driver to be reused across different microcontrollers and hardware configurations.

```mermaid
flowchart TD

    APP["Application"]

    DRIVER["LCD Driver"]

    BUS_IF["Bus Interface"]
    BL_IF["Backlight Interface"]

    BUS_ADAPTER["PCF8574 Bus Adapter"]
    BL_ADAPTER["PWM Backlight Adapter"]

    PCF8574["PCF8574 Driver"]
    PWM["PWM Driver"]

    I2C["I2C Peripheral"]
    TIMER["Timer Peripheral"]

    LCD["LCD LCD"]

    APP --> DRIVER

    DRIVER --> BUS_IF
    DRIVER --> BL_IF

    BUS_IF --> BUS_ADAPTER
    BL_IF --> BL_ADAPTER

    BUS_ADAPTER --> PCF8574
    BL_ADAPTER --> PWM

    PCF8574 --> I2C
    PWM --> TIMER

    I2C --> LCD
    TIMER --> LCD
```
### 3.1 Bus Interface

The communication between the LCD driver and the physical bus is abstracted through:

```c
LCD_BusInterface_t
```

Definition:

```c
typedef struct
{
    LCD_BusOpStatus_t (*TransferNibble)(
        void* Context,
        uint8_t Nibble,
        LCD_RegisterSelect_t Rs
    );

    void* Context;

} LCD_BusInterface_t;
```

The driver communicates only through the `TransferNibble()` callback.

The concrete adapter is responsible for:

- Mapping data bits.
- Controlling RS signal.
- Generating EN pulse.
- Handling the physical communication method.

The driver sends data as nibbles because the LCD operates internally with 4-bit transfers when configured in 4-bit mode.

### 3.2 PCF8574 Bus Adapter

The PCF8574 adapter implements the Bus Interface using an I2C GPIO expander.

Responsibilities:

- Convert LCD signals into PCF8574 bit operations.
- Generate the enable pulse.
- Control RS.
- Send high and low nibbles.
- Handle I2C communication through the PCF8574 driver.

Initialization:

```c
LCD_PCF8574_BusAdapterInit(
    LCD_BusInterface_t* Bus,
    PCF8574_Handle_t* PCF8574_Instance
);
```

The adapter stores the PCF8574 instance as context and exposes the required bus operations to the LCD driver.

### 3.3 Backlight Interface

Backlight control is abstracted through the:

```c
LCD_BacklightInterface_t
```

interface.

Definition:

```c
typedef struct
{
    LCD_BacklightOpStatus_t (*TurnOn)(void* Context);

    LCD_BacklightOpStatus_t (*TurnOff)(void* Context);

    LCD_BacklightOpStatus_t (*SetBrightness)(
        void* Context,
        uint16_t Level
    );

    uint16_t (*GetBrightness)(void* Context);

    void* Context;

} LCD_BacklightInterface_t;
```

The LCD driver does not know how the backlight is physically controlled.

The concrete adapter is responsible for implementing:

- Backlight activation.
- Backlight deactivation.
- Brightness control.
- Hardware-specific conversion.

Possible implementations:

- GPIO controlled backlight.
- PWM controlled backlight.

### 3.4 PWM Backlight Adapter

The provided PWM adapter controls the LCD backlight intensity using a PWM peripheral.

The adapter converts a brightness level into a PWM duty cycle.

Initialization:

```c
LCD_BacklightOpStatus_t LCD_PWM_BacklightAdapterInit(
    LCD_BacklightInterface_t* Backlight,
    void* Context
);
```

The context must point to a PWM instance:

```c
PWM_Handle_t
```

Example:

```c
PWM_Handle_t LCD_BacklightAdapter;

LCD_PWM_BacklightAdapterInit(
    &LCD._backlight,
    &LCD_BacklightAdapter
);
```

The adapter is independent from the timer implementation. Any PWM driver implementing the required interface can be used.

---

## 4. Directory Structure

Example project organization:

```text
LCD/
|
├── Inc/
│   ├── LCD_Driver.h
│   ├── LCD_BusInterface.h
│   ├── LCD_BacklightInterface.h
│   ├── LCD_PCF8574_BusAdapter.h
│   └── LCD_PWM_BacklightAdapter.h
|
└── Src/
    ├── LCD_Driver.c
    ├── LCD_PCF8574_BusAdapter.c
    └── LCD_PWM_BacklightAdapter.c
```

---

## 5. Driver Responsibilities

The LCD driver is responsible for:

- Implementing the LCD communication protocol.
- Managing the LCD initialization sequence.
- Sending commands and data.
- Managing internal display configuration state.
- Controlling cursor position.
- Writing characters and strings.
- Managing CGRAM custom characters.
- Controlling the backlight through an abstract interface.

The driver is **not responsible** for:

- Direct GPIO access.
- Direct I2C communication.
- Timer configuration.
- Interrupt handling.
- Character buffering.
- Display queue management.
- Low-level peripheral initialization.

---

## 6. Dependencies

Required dependencies:

```text
Time Platform Interface
```

Provides timing functions required by the LCD specification:

```c
Platform_DelayUs()
Platform_DelayMs()
```

Optional dependencies:

```text
PCF8574 Driver
PWM Platform Interface
```

These are required only when using the provided adapters.

---

## 7. Data Structures

### 7.1 LCD Handle

The driver uses `LCD_Handle_t` to represent an LCD LCD device instance.

```c
typedef struct
{
    LCD_BusInterface_t       _bus;
    LCD_BacklightInterface_t _backlight;
    LCD_LineNumber_t         _rows;
    uint8_t                  _cols;
    LCD_InterfaceMode_t      _interface_mode;
    LCD_CharacterFont_t      _font_dot_size;
    bool                     _initialized;

} LCD_Handle_t;
```

| Member            | Description                                                    |
| ----------------- | -------------------------------------------------------------- |
| `_bus`            | Bus interface used to communicate with the LCD controller. |
| `_backlight`      | Backlight control interface associated with the LCD module.    |
| `_rows`           | Configured number of display lines.                            |
| `_cols`           | Configured number of display columns.                          |
| `_interface_mode` | Selected LCD data interface mode.                          |
| `_font_dot_size`  | Selected character font configuration.                         |
| `_initialized`    | Internal initialization state of the driver instance.          |

The members of `LCD_Handle_t` are considered private driver data and shall not be accessed or modified directly by the application after initialization.

The application shall interact with the LCD exclusively through the public driver API.

### 7.2 Operation Status

The driver uses `LCD_OpStatus_t` to report the result of driver operations.

```c
typedef enum
{
    LCD_OPERATION_OK,
    LCD_OPERATION_FAIL

} LCD_OpStatus_t;
```

| Status                   | Description                       |
| ------------------------ | --------------------------------- |
| `LCD_OPERATION_OK`   | Operation completed successfully. |
| `LCD_OPERATION_FAIL` | Operation could not be completed. |


### 7.3 Interface Mode

The `LCD_InterfaceMode_t` enumeration defines the controller data interface mode.

```c
typedef enum
{
    LCD_8BIT_MODE,
    LCD_4BIT_MODE

} LCD_InterfaceMode_t;
```

| Mode                | Description                            |
| ------------------- | -------------------------------------- |
| `LCD_4BIT_MODE` | Four-bit parallel communication mode.  |
| `LCD_8BIT_MODE` | Eight-bit parallel communication mode. |

Only `LCD_4BIT_MODE` is currently implemented by the driver.

### 7.4 Character Font

The `LCD_CharacterFont_t` enumeration defines the character font configuration.

```c
typedef enum
{
    LCD_5X10_FONT,
    LCD_5X8_FONT

} LCD_CharacterFont_t;
```

| Font                | Description                                              |
| ------------------- | -------------------------------------------------------- |
| `LCD_5X8_FONT`  | 5×8 character font. Supports up to 8 custom characters.  |
| `LCD_5X10_FONT` | 5×10 character font. Supports up to 4 custom characters. |

The 5×10 font is supported only with single-line display configuration.

### 7.5 Display Line Configuration

The `LCD_LineNumber_t` enumeration defines the number of display lines.

```c
typedef enum
{
    LCD_2LINE = 0x01,
    LCD_1LINE = 0x00

} LCD_LineNumber_t;
```

| Configuration   | Description                        |
| --------------- | ---------------------------------- |
| `LCD_1LINE` | Single-line display configuration. |
| `LCD_2LINE` | Two-line display configuration.    |


### 7.6 Bus Interface

The LCD driver communicates with the physical display through `LCD_BusInterface_t`.

```c
typedef struct
{
    LCD_BusOpStatus_t (*TransferNibble)(
        void* Context,
        uint8_t Nibble,
        LCD_RegisterSelect_t Rs
    );

    void* Context;

} LCD_BusInterface_t;
```

| Member           | Description                                                      |
| ---------------- | ---------------------------------------------------------------- |
| `TransferNibble` | Callback used to transfer a 4-bit value to the LCD data bus. |
| `Context`        | Adapter-specific context passed to the callback.                 |

The interface isolates the driver from the physical communication implementation.

The concrete adapter is responsible for:

* Mapping the LCD data signals.
* Controlling the RS signal.
* Generating the enable pulse.
* Implementing the underlying communication mechanism.

The current project provides a PCF8574-based I2C bus adapter.

### 7.7 Backlight Interface

The LCD driver controls the LCD backlight through `LCD_BacklightInterface_t`.

```c
typedef struct
{
    LCD_BacklightOpStatus_t (*TurnOn)(void* Context);

    LCD_BacklightOpStatus_t (*TurnOff)(void* Context);

    LCD_BacklightOpStatus_t (*SetBrightness)(
        void* Context,
        uint16_t Level
    );

    uint16_t (*GetBrightness)(void* Context);

    void* Context;

} LCD_BacklightInterface_t;
```

| Member          | Description                                             |
| --------------- | ------------------------------------------------------- |
| `TurnOn`        | Enables the LCD backlight.                              |
| `TurnOff`       | Disables the LCD backlight.                             |
| `SetBrightness` | Sets the requested brightness level.                    |
| `GetBrightness` | Returns the brightness level maintained by the adapter. |
| `Context`       | Adapter-specific context passed to the callbacks.       |

The brightness level is expressed as a percentage from `0` to `100`. The actual implementation may provide continuous control, such as PWM, or binary control, such as GPIO.

---

## 8. API Reference

### 8.1 LCD_Init

- Initializes an LCD LCD controller according to the configuration stored in the device handle.

#### Function Signature
```c
LCD_OpStatus_t LCD_Init(
    LCD_Handle_t* Device
);
```
#### Parameters
| Parameter | Description                             |
| --------- | --------------------------------------- |
| `Device`  | Pointer to the LCD device instance. |

#### Return
| Return Value             | Description                                                         |
| ------------------------ | ------------------------------------------------------------------- |
| `LCD_OPERATION_OK`   | LCD successfully initialized.                                       |
| `LCD_OPERATION_FAIL` | Initialization failed or the selected configuration is unsupported. |

The initialization sequence configures the controller for the selected interface mode, number of display lines and character font.

**Note:**
- Repeated initialization of an already initialized device returns successfully without repeating the initialization sequence.


### 8.2 LCD_Clear

- Clears the complete display.


#### Function Signature
```c
LCD_OpStatus_t LCD_Clear(
    LCD_Handle_t* Device
);
```


#### Parameters
| Parameter | Description                                         |
| --------- | --------------------------------------------------- |
| `Device`  | Pointer to the initialized LCD device instance. |


#### Return
| Return Value             | Description                   |
| ------------------------ | ----------------------------- |
| `LCD_OPERATION_OK`   | Display successfully cleared. |
| `LCD_OPERATION_FAIL` | Operation failed.             |


### 8.3 LCD_Home

- Returns the display cursor to the home position.


#### Function Signature
```c
LCD_OpStatus_t LCD_Home(
    LCD_Handle_t* Device
);
```

#### Parameters
| Parameter | Description                                         |
| --------- | --------------------------------------------------- |
| `Device`  | Pointer to the initialized LCD device instance. |


#### Return
| Return Value             | Description                            |
| ------------------------ | -------------------------------------- |
| `LCD_OPERATION_OK`   | Home operation completed successfully. |
| `LCD_OPERATION_FAIL` | Operation failed.                      |


### 8.4 Display Control

- The following functions control the display visibility, cursor and cursor blinking:


#### Function Signatures
```c
LCD_OpStatus_t LCD_DisplayOn(
    LCD_Handle_t* Device
);

LCD_OpStatus_t LCD_DisplayOff(
    LCD_Handle_t* Device
);

LCD_OpStatus_t LCD_CursorOn(
    LCD_Handle_t* Device
);

LCD_OpStatus_t LCD_CursorOff(
    LCD_Handle_t* Device
);

LCD_OpStatus_t LCD_BlinkOn(
    LCD_Handle_t* Device
);

LCD_OpStatus_t LCD_BlinkOff(
    LCD_Handle_t* Device
);
```

| Function               | Description                  |
| ---------------------- | ---------------------------- |
| `LCD_DisplayOn()`  | Enables the display output.  |
| `LCD_DisplayOff()` | Disables the display output. |
| `LCD_CursorOn()`   | Enables cursor visibility.   |
| `LCD_CursorOff()`  | Disables cursor visibility.  |
| `LCD_BlinkOn()`    | Enables cursor blinking.     |
| `LCD_BlinkOff()`   | Disables cursor blinking.    |

Each function returns `LCD_OPERATION_OK` when the requested command is successfully transmitted and `LCD_OPERATION_FAIL` otherwise.


### 8.5 Cursor and Display Shift Control
- The following functions control the display shift and cursor increment/decrement:

#### Function Signatures
```c
LCD_OpStatus_t LCD_IncrementCursor(
    LCD_Handle_t* Device
);

LCD_OpStatus_t LCD_DecrementCursor(
    LCD_Handle_t* Device
);

LCD_OpStatus_t LCD_EnableShift(
    LCD_Handle_t* Device
);

LCD_OpStatus_t LCD_DisableShift(
    LCD_Handle_t* Device
);
```

| Function                    | Description                                                                |
| --------------------------- | -------------------------------------------------------------------------- |
| `LCD_IncrementCursor()` | Configures the cursor address to increment after data transfers.           |
| `LCD_DecrementCursor()` | Configures the cursor address to decrement after data transfers.           |
| `LCD_EnableShift()`     | Enables automatic display shifting according to the configured entry mode. |
| `LCD_DisableShift()`    | Disables automatic display shifting.                                       |

Each function returns `LCD_OPERATION_OK` when the requested command is successfully transmitted and `LCD_OPERATION_FAIL` otherwise.


### 8.6 LCD_SetCursor

- Positions the cursor at a specified row and column.

#### Function Signature
```c
LCD_OpStatus_t LCD_SetCursor(
    LCD_Handle_t* Device,
    uint8_t Row,
    uint8_t Col
);
```
#### Parameters
| Parameter | Description                                         |
| --------- | --------------------------------------------------- |
| `Device`  | Pointer to the initialized LCD device instance. |
| `Row`     | Zero-based display row.                             |
| `Col`     | Zero-based display column.                          |

#### Return
| Return Value             | Description                            |
| ------------------------ | -------------------------------------- |
| `LCD_OPERATION_OK`   | Set cursor operation completed successfully. |
| `LCD_OPERATION_FAIL` | Operation failed.                      |

**Note:**
- Values outside the configured display dimensions are clamped to the nearest valid position.


### 8.7 LCD_WriteChar

- Writes a single character at the current cursor position.

#### Function Signature
```c
LCD_OpStatus_t LCD_WriteChar(
    LCD_Handle_t* Device,
    uint8_t Char
);
```

#### Parameters
| Parameter | Description                                         |
| --------- | --------------------------------------------------- |
| `Device`  | Pointer to the initialized LCD device instance. |
| `Char`    | Character code to transmit.                         |

#### Return
| Return Value             | Description                            |
| ------------------------ | -------------------------------------- |
| `LCD_OPERATION_OK`   | Write char operation completed successfully. |
| `LCD_OPERATION_FAIL` | Operation failed.                      |

**Note:**
- The cursor movement after the write is determined by the current Entry Mode configuration.


### 8.8 LCD_WriteString

- Writes a null-terminated string beginning at the current cursor position.

#### Function Signature
```c
LCD_OpStatus_t LCD_WriteString(
    LCD_Handle_t* Device,
    const char* String
);
```
#### Parameters
| Parameter | Description                                         |
| --------- | --------------------------------------------------- |
| `Device`  | Pointer to the initialized LCD device instance. |
| `String`  | Pointer to a null-terminated string.                |

#### Return
| Return Value             | Description                            |
| ------------------------ | -------------------------------------- |
| `LCD_OPERATION_OK`   | Write string operation completed successfully. |
| `LCD_OPERATION_FAIL` | Operation failed.                      |

**Note:**
- The function does not reposition the cursor and does not limit the number of characters based on display width. Characters are transmitted until the null terminator is reached.


### 8.9 LCD_PrintLine

- Writes text starting at the beginning of a specified display row.

#### Function Signature
```c
LCD_OpStatus_t LCD_PrintLine(
    LCD_Handle_t* Device,
    uint8_t Row,
    const char* Text
);
```

#### Parameters
| Parameter | Description                                         |
| --------- | --------------------------------------------------- |
| `Device`  | Pointer to the initialized LCD device instance. |
| `Row`     | Zero-based display row.                             |
| `Text`    | Pointer to a null-terminated string.                |

#### Return
| Return Value             | Description                            |
| ------------------------ | -------------------------------------- |
| `LCD_OPERATION_OK`   | Print line operation completed successfully. |
| `LCD_OPERATION_FAIL` | Operation failed.                      |

**Note:**
- If the requested row exceeds the configured display size, it is clamped to the last valid row.
- If the supplied text exceeds the configured display width, only the characters that fit on the selected row are written.


### 8.10 LCD_ClearLine

- Clears a complete display row by writing space characters across all configured columns.

#### Function Signature
```c
LCD_OpStatus_t LCD_ClearLine(
    LCD_Handle_t* Device,
    uint8_t Row
);
```
#### Parameters
| Parameter | Description                                         |
| --------- | --------------------------------------------------- |
| `Device`  | Pointer to the initialized LCD device instance. |
| `Row`     | Zero-based display row.                             |

#### Return
| Return Value             | Description                            |
| ------------------------ | -------------------------------------- |
| `LCD_OPERATION_OK`   | Clear line operation completed successfully. |
| `LCD_OPERATION_FAIL` | Operation failed.                      |

**Note:**
- The cursor is restored to the beginning of the selected row after the operation.


### 8.11 LCD_CreateChar

- Creates or updates a custom character in CGRAM.

#### Function Signature
```c
LCD_OpStatus_t LCD_CreateChar(
    LCD_Handle_t* Device,
    uint8_t Position,
    const uint8_t* PatternBitMap
);
```
#### Parameters
| Parameter       | Description                                         |
| --------------- | --------------------------------------------------- |
| `Device`        | Pointer to the initialized LCD device instance. |
| `Position`      | CGRAM character position.                           |
| `PatternBitMap` | Pointer to the custom character bitmap.             |

#### Return
| Return Value             | Description                            |
| ------------------------ | -------------------------------------- |
| `LCD_OPERATION_OK`   | Create char operation completed successfully. |
| `LCD_OPERATION_FAIL` | Operation failed.                      |

Valid character positions depend on the selected font:

| Font                | Valid Positions | Bitmap Size |
| ------------------- | --------------: | ----------: |
| `LCD_5X8_FONT`  |         `0`–`7` |     8 bytes |
| `LCD_5X10_FONT` |         `0`–`3` |     4 bytes |

The driver tracks programmed CGRAM positions and restores the cursor to row `0`, column `0` after programming the character.


### 8.12 LCD_WriteCustomChar

- Writes a previously created custom character to the display.

#### Function Signature
```c
LCD_OpStatus_t LCD_WriteCustomChar(
    LCD_Handle_t* Device,
    uint8_t CharPosition
);
```
#### Parameters
| Parameter      | Description                                         |
| -------------- | --------------------------------------------------- |
| `Device`       | Pointer to the initialized LCD device instance. |
| `CharPosition` | Previously programmed CGRAM character position.     |

#### Return
| Return Value             | Description                            |
| ------------------------ | -------------------------------------- |
| `LCD_OPERATION_OK`   | Write custom char operation completed successfully. |
| `LCD_OPERATION_FAIL` | Operation failed.                      |

**Note:**
- The operation fails if the requested CGRAM position has not previously been programmed using `LCD_CreateChar()`.


### 8.13 LCD_BacklightOn

- Enables the LCD backlight through the configured backlight interface.

#### Function Signature
```c
LCD_OpStatus_t LCD_BacklightOn(
    LCD_Handle_t* Device
);
```
#### Parameters
| Parameter      | Description                                         |
| -------------- | --------------------------------------------------- |
| `Device`       | Pointer to the initialized LCD device instance. |

#### Return
| Return Value             | Description                            |
| ------------------------ | -------------------------------------- |
| `LCD_OPERATION_OK`   | Backlight on operation completed successfully. |
| `LCD_OPERATION_FAIL` | Operation failed.                      |

If the configured brightness is `0`, the driver requests a brightness of `100` before enabling the backlight.


### 8.14 LCD_BacklightOff

- Disables the LCD backlight.

#### Function Signature
```c
LCD_OpStatus_t LCD_BacklightOff(
    LCD_Handle_t* Device
);
```
#### Parameters
| Parameter      | Description                                         |
| -------------- | --------------------------------------------------- |
| `Device`       | Pointer to the initialized LCD device instance. |

#### Return
| Return Value             | Description                            |
| ------------------------ | -------------------------------------- |
| `LCD_OPERATION_OK`   | Backlight off operation completed successfully. |
| `LCD_OPERATION_FAIL` | Operation failed.                      |

**Note:**
- Turning the backlight off does not modify display contents, DDRAM data, cursor position or LCD controller state.


### 8.15 LCD_SetBrightness

- Sets the LCD backlight brightness level.

#### Function Signature
```c
LCD_OpStatus_t LCD_SetBrightness(
    LCD_Handle_t* Device,
    uint16_t BrightPercent
);
```
#### Parameters
| Parameter       | Description                                         |
| --------------- | --------------------------------------------------- |
| `Device`        | Pointer to the initialized LCD device instance. |
| `BrightPercent` | Requested brightness percentage from `0` to `100`.  |

#### Return
| Return Value             | Description                            |
| ------------------------ | -------------------------------------- |
| `LCD_OPERATION_OK`   | Set brightness operation completed successfully. |
| `LCD_OPERATION_FAIL` | Operation failed.                      |

**Note:**
- The actual brightness control behavior depends on the configured backlight adapter.

---

## 9. Operation Flow

### 9.1 Initialization Flow

The typical initialization sequence is shown below.

```mermaid
sequenceDiagram

    participant APP as Application
    participant PCF as PCF8574 Driver
    participant BUS as PCF8574 Bus Adapter
    participant PWM as PWM Driver
    participant BL as PWM Backlight Adapter
    participant LCD as LCD Driver

    APP->>PCF: PCF8574_Init()
    APP->>BUS: LCD_PCF8574_BusAdapterInit()

    BUS->>BUS: Store PCF8574 context

    APP->>PWM: PPWM_Create()
    APP->>PWM: PPWM_Init()
    APP->>PWM: PPWM_SetFrequency()

    APP->>BL: LCD_PWM_BacklightAdapterInit()

    BL->>BL: Store PWM context

    APP->>LCD: LCD_Init()

    LCD->>BUS: TransferNibble()
    BUS->>PCF: Write GPIO state

    PCF-->>BUS: I2C Transfer

    LCD->>LCD: Configure 4-bit mode
    LCD->>LCD: Configure display parameters

    APP->>LCD: LCD_SetBrightness()

    LCD->>BL: SetBrightness()

    BL->>PWM: Update duty cycle

    APP->>LCD: LCD_BacklightOn()

    LCD->>BL: TurnOn()

    BL->>PWM: Enable PWM output

    Note over APP,LCD: Display ready for application use
    
```

---

## 10. Usage Example

The following example demonstrates the initialization of the LCD driver using:

- PCF8574 as the bus communication interface.
- PWM as the backlight controller.

Required includes:

```c
#include "main.h"

#include "i2c.h"
#include "tim.h"
#include "gpio.h"

#include "LCD_Driver.h"
#include "LCD_PCF8574_BusAdapter.h"
#include "LCD_PWM_BacklightAdapter.h"
```

### 10.1 Driver Objects

```c
PCF8574_Handle_t LCD_BusAdapter;

const int16_t LCD_BusAdapter_I2C_Address = 0x20;

PWM_Handle_t LCD_BacklightAdapter;

LCD_Handle_t LCD =
{
    ._cols           = 16,
    ._rows           = LCD_2LINE,
    ._interface_mode = LCD_4BIT_MODE,
    ._font_dot_size  = LCD_5X8_FONT,
    ._initialized    = false,
};
```

### 10.2 Initialization

```c
int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_TIM4_Init();


    /*
     * Initialize PCF8574 bus expander
     */
    PCF8574_Init(
        &LCD_BusAdapter,
        LCD_BusAdapter_I2C_Address,
        &hi2c1
    );


    /*
     * Connect PCF8574 adapter to LCD bus interface
     */
    LCD_PCF8574_BusAdapterInit(
        &LCD._bus,
        &LCD_BusAdapter
    );


    /*
     * Configure PWM backlight
     */
    PPWM_Create(
        &LCD_BacklightAdapter,
        &htim4,
        PWM_CHANNEL_4
    );

    PPWM_Init(
        &LCD_BacklightAdapter
    );

    PPWM_SetFrequency(
        &LCD_BacklightAdapter,
        1500
    );


    /*
     * Connect PWM adapter to LCD backlight interface
     */
    LCD_PWM_BacklightAdapterInit(
        &LCD._backlight,
        &LCD_BacklightAdapter
    );


    /*
     * Initialize LCD controller
     */
    LCD_Init(
        &LCD
    );


    LCD_SetBrightness(
        &LCD,
        40
    );


    LCD_BacklightOn(
        &LCD
    );
    //LCD initialized and ready for application use
}
```

### 10.3 Write Text

```c
LCD_SetCursor(
    &LCD,
    0,
    0
);

LCD_WriteString(
    &LCD,
    "Hello World!"
);
```

### 10.4 Write Character

```c
LCD_SetCursor(
    &LCD,
    0,
    5
);

LCD_WriteChar(
    &LCD,
    'A'
);
```

### 10.5 Clear Display

```c
LCD_Clear(
    &LCD
);
```

### 10.6 Print Text on Specific Line

```c
LCD_PrintLine(
    &LCD,
    1,
    "Line 2"
);
```

### 10.7 Clear Specific Line

```c
LCD_ClearLine(
    &LCD,
    0
);
```

### 10.8 Backlight Control

```c
LCD_BacklightOn(
    &LCD
);


LCD_SetBrightness(
    &LCD,
    80
);


LCD_BacklightOff(
    &LCD
);
```

### 10.9 Custom Characters

The LCD controller provides 64 bytes of CGRAM memory that can be used to store custom characters.

The driver provides support for:

- Creating custom characters.
- Tracking programmed CGRAM positions.
- Writing custom characters to the display.

Example:

```c
const uint8_t lock_symbol_bitmap[8] =
{
    0b00100,
    0b01110,
    0b10001,
    0b11111,
    0b11011,
    0b11111,
    0b11111,
    0b00000
};


LCD_CreateChar(
    &LCD,
    0,
    lock_symbol_bitmap
);


LCD_SetCursor(
    &LCD,
    0,
    10
);


LCD_WriteCustomChar(
    &LCD,
    0
);
```

The driver verifies whether the requested CGRAM position has already been programmed before writing the character.

This prevents displaying undefined custom characters.

---

## 11. Design Decisions

### 11.1 Separation Between Driver and Hardware

The LCD driver does not access hardware peripherals directly.

The driver communicates only through:

- `LCD_BusInterface_t`
- `LCD_BacklightInterface_t`

This allows:

- Changing the communication bus without modifying the driver.
- Replacing PCF8574 with direct GPIO.
- Replacing PWM backlight with GPIO control.
- Creating hardware mocks for unit testing.

This design follows the **Dependency Inversion Principle**, where the high-level driver depends on abstractions instead of concrete hardware implementations.

### 11.2 Nibble-Based Bus Interface

When operating in 4-bit mode, the LCD requires each byte to be transferred as:

1. High nibble.
2. Low nibble.

The driver handles the transfer order and protocol timing.

The adapter is responsible for:

- Physical bit mapping.
- RS control.
- Enable pulse generation.
- Bus synchronization.

Example:

```text
Byte: 0x41 ('A')

Binary:
0100 0001

Transfer sequence:

1st nibble:
0100

2nd nibble:
0001
```

The driver therefore remains independent from the physical transport layer.

### 11.3 Internal State Management

The driver maintains internal configuration flags to keep track of LCD state.

Examples:

- Display enabled/disabled.
- Cursor enabled/disabled.
- Cursor blinking.
- Entry mode configuration.
- Display shift configuration.

This allows the driver to reconstruct configuration commands without resetting unrelated settings.

Example:

Instead of blindly sending:

```text
Display OFF
```

the driver can preserve:

```text
Cursor enabled
Blink enabled
Shift disabled
```

and only modify the required bit.


### 11.4 Custom Character Tracking

The driver tracks which CGRAM positions have been initialized.

Benefits:

- Prevents writing invalid custom characters.
- Avoids unexpected display behavior.
- Simplifies application usage.

The application only needs to create the character once before using it.

### 11.5 Delay-Based Synchronization

The LCD provides a busy flag that can be read to determine when the controller is ready for a new command.

This implementation uses fixed delays instead.

Advantages:

- Simpler hardware requirements.
- Compatible with write-only interfaces.
- Works correctly with PCF8574 adapters.

Trade-off:

- Slightly slower than busy flag polling.
- Additional waiting time after commands.

A future implementation could add busy-flag support if required.

### 11.6 Timing Abstraction

The driver does not directly depend on MCU-specific delay functions.

Timing is provided through the platform layer:

```c
Platform_DelayUs()

Platform_DelayMs()
```

This allows portability between:

- Different STM32 families.
- Different microcontrollers.
- Different operating environments.

---

## 12. Error Handling

The LCD driver uses a binary operation status model through `LCD_OpStatus_t`.

```c
typedef enum
{
    LCD_OPERATION_OK,
    LCD_OPERATION_FAIL

} LCD_OpStatus_t;
```

The application shall verify the returned status of driver operations whenever the result is relevant to subsequent execution.

### 12.1 Operation Failure Conditions

`LCD_OPERATION_FAIL` may be returned when:

* The device handle is `NULL`.
* The device has not been initialized.
* A required string or bitmap pointer is `NULL`.
* An unsupported interface mode is selected.
* A bus transfer fails.
* A backlight adapter operation fails.
* An invalid CGRAM position is requested.
* A custom character is requested before it has been programmed.

For example, `LCD_CreateChar()` explicitly validates the device, initialization state, bitmap pointer and CGRAM position before programming the character.

### 12.2 Bus Errors

Communication errors originating from the bus adapter are propagated to the LCD driver as `LCD_OPERATION_FAIL`.

The bus interface itself provides:

```c
LCD_BusOpStatus_t
```

with:

```text
LCD_BUS_OPERATION_OK
LCD_BUS_OPERATION_FAIL
```

The concrete adapter is responsible for translating hardware communication failures into this status.

### 12.3 Backlight Errors

Backlight adapter failures are also propagated to the main driver API as:

```text
LCD_OPERATION_FAIL
```

The application does not need to handle the adapter-specific status directly when using the public LCD API.

### 12.4 Error Information

The current API does not expose detailed error codes or failure reasons.

Therefore, `LCD_OPERATION_FAIL` indicates that the requested operation was not completed, but does not identify the exact underlying cause.

---

## 13. Usage Constraints

The following constraints shall be considered when integrating the LCD driver.

### 13.1 Initialization

The LCD controller must be successfully initialized using:

```c
LCD_Init()
```

before performing normal display operations.

The driver maintains an internal initialization state and rejects operations performed on an uninitialized device.


### 13.2 Interface Mode

The current implementation supports only:

```c
LCD_4BIT_MODE
```

Although `LCD_8BIT_MODE` exists in the public enumeration, the current driver implementation explicitly rejects 8-bit mode.


### 13.3 Timing Dependency

The driver relies on fixed delays rather than reading the LCD Busy Flag.

The required timing abstraction is provided through:

```c
Platform_DelayUs()
Platform_DelayMs()
```

This makes the driver suitable for write-only interfaces such as the provided PCF8574 adapter, but introduces additional waiting time compared with Busy Flag polling.


### 13.4 Display Geometry

The configured number of rows and columns must match the physical LCD module.

Cursor and line operations use the configured display geometry.

Row and column parameters passed to the cursor-related APIs are clamped when they exceed the configured dimensions.


### 13.5 String Handling

`LCD_WriteString()` requires a null-terminated string.

The function does not automatically limit the amount of data written according to the display width. The application is therefore responsible for ensuring that the transmitted string is appropriate for the desired display behavior.

`LCD_PrintLine()` provides explicit row and display-width handling and is preferable when writing bounded text to a specific display line.

### 13.6 Custom Characters

Custom character bitmap size depends on the configured font:

```text
5x8  → 8 bytes
5x10 → 4 bytes
```

The application must provide a bitmap buffer with the appropriate size.

A custom character must be created using:

```c
LCD_CreateChar()
```

before it can be displayed using:

```c
LCD_WriteCustomChar()
```

The driver internally tracks programmed CGRAM positions and rejects attempts to write undefined custom characters.

### 13.7 Backlight Availability

Backlight functions require a valid `LCD_BacklightInterface_t` implementation.

The LCD controller itself does not provide native backlight control. Backlight behavior is therefore determined by the external adapter implementation.

### 13.8 Multi-Instance Usage

The current implementation is primarily intended for single-display applications.

Although each LCD is represented through an `LCD_Handle_t`, some runtime state variables are maintained at module scope. Consequently, simultaneous use of multiple LCD instances may result in shared-state conflicts.

Complete multi-instance support requires moving all runtime state into the device handle.

### 13.9 Blocking Behavior

Display operations are synchronous and use fixed execution delays to satisfy the LCD timing requirements.

Consequently, operations such as display clearing, initialization and long string transmission may block the caller while communication and timing requirements are being executed.

Applications requiring strict real-time responsiveness should account for these execution times when scheduling LCD operations.

---

## 14. Applications

The LCD driver is intended for embedded systems that require a character-based human-machine interface while maintaining separation between application logic and hardware-specific communication.

Typical applications include:

* Embedded system status displays.
* Configuration and parameter interfaces.
* User-interface panels.
* Diagnostic and service interfaces.
* Electronic locks and access-control systems.
* Industrial control panels.
* Small appliance interfaces.
* Embedded instrumentation.
* Prototype and development-system displays.

The driver is particularly suitable for applications where the LCD communication hardware may change without requiring modifications to the application-level display logic.

For example, the same application interface can be maintained while replacing:

```text
PCF8574 + I2C
```

with another bus implementation such as:

```text
Direct GPIO
SPI shift register
Other I/O expander
```

Likewise, the backlight implementation can be changed independently between GPIO, PWM or another hardware mechanism through `LCD_BacklightInterface_t`.


```mermaid
flowchart LR
    APP[Application]

    API[Driver API]
    BUS_IF[Bus Interface]
    BL_IF[Backlight Interface]

    BUS_ADAPTER[Bus Adapter]
    BL_ADAPTER[Backlight Adapter]

    HW_BUS[Physical Bus]
    HW_BL[Physical Backlight]

    APP -->|"LCD operations"| API

    API -->|"Commands & Data"| BUS_IF
    API -->|"Backlight control"| BL_IF

    BUS_IF -.->|"Abstraction"| BUS_ADAPTER
    BL_IF -.->|"Abstraction"| BL_ADAPTER

    BUS_ADAPTER -->|"Physical communication"| HW_BUS
    BL_ADAPTER -->|"Physical control"| HW_BL

    style APP fill:#000,stroke:#000,stroke-width:2px,color:#fff
    style API fill:#555,stroke:#000,stroke-width:2px,color:#fff
    style BUS_IF fill:#777,stroke:#000,stroke-width:2px,color:#fff
    style BL_IF fill:#777,stroke:#000,stroke-width:2px,color:#fff
    style BUS_ADAPTER fill:#999,stroke:#000,stroke-width:2px,color:#000
    style BL_ADAPTER fill:#999,stroke:#000,stroke-width:2px,color:#000
    style HW_BUS fill:#bbb,stroke:#000,stroke-width:2px,color:#000
    style HW_BL fill:#bbb,stroke:#000,stroke-width:2px,color:#000
```

Within the current project, the driver is used as part of the Electronic Lock system to provide a character-based user interface.

---

## 15. Limitations

### 15.1 Interface Mode

- Only 4-bit communication mode is currently supported.
- 8-bit mode is not implemented.

### 15.2 Busy Flag

- The driver does not read the LCD busy flag.
- Synchronization is performed using fixed delays.


### 15.3 Display Shift API

Entry mode shift configuration is supported, but advanced display shifting operations are not currently exposed through dedicated APIs.

### 15.4 Multi-Instance Support

- The current implementation was designed primarily for single-display applications.
- Although the driver uses:

```c
LCD_Handle_t
```

to represent an instance, some internal state variables are still maintained at module level.

- Using multiple LCD instances simultaneously may cause conflicts.

- Future improvement:

    - Move all runtime state variables into:

```c
LCD_Handle_t
```

to achieve complete multi-instance support.

### 15.5 Available Adapters

Currently provided adapters:

- PCF8574 I2C bus adapter.
- PWM timer backlight adapter.

- Additional adapters can be created by implementing the corresponding interfaces:

```c
LCD_BusInterface_t

LCD_BacklightInterface_t
```

Examples:

- Direct GPIO bus.
- SPI shift register.
- GPIO-controlled backlight.
- DAC-controlled brightness.

---

## 16. Future Improvements

Possible improvements:

- Add 8-bit interface support.
- Add busy flag polling.
- Add complete multi-instance support.
- Add display shift API.
- Add display geometry abstraction for different LCD models.
- Add support for additional communication adapters.

---

## 17. License

This module is part of the:

```text
Electronic Lock Project
```

and follows the project's license terms.
