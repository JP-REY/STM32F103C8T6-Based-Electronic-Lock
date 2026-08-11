# Hardware-Independent Driver for the PCF8574 8-bit I/O Expander Designed for Embedded Systems.

---
## Overview
- The driver provides an abstraction layer for controlling the PCF8574 device through an I2C interface, allowing applications to read and write the complete 8-bit quasi-bidirectional I/O port.

- The driver abstracts the underlying I2C communication through the platform interface, allowing reuse across different microcontrollers and hardware platforms.

---

## Features

- PCF8574 device initialization and deinitialization.
- Full 8-bit port write operation.
- Full 8-bit port read operation.
- Individual bit manipulation:
  - Set bit.
  - Clear bit.
  - Toggle bit.
  - Read bit.
- Internal port shadow management.
- Hardware-independent I2C communication.
- Support for multiple PCF8574 instances.
- Abstracted platform communication layer.
- Simple API for GPIO expander applications.

---

## Architecture

The driver follows a layered architecture where the PCF8574 communication is isolated from the MCU-specific I2C peripheral implementation.

The PCF8574 driver does not access hardware registers directly. All communication is performed through the I2C Platform Interface.

```mermaid
flowchart TD

    APP["Application"]
    DRIVER["PCF8574 Driver"]
    I2C_IF["I2C Platform Interface"]
    I2C_DRIVER["I2C Driver"]
    HAL["MCU Hardware Abstraction"]
    PCF["PCF8574 Device"]

    APP --> DRIVER
    DRIVER --> I2C_IF
    I2C_IF --> I2C_DRIVER
    I2C_DRIVER --> HAL
    HAL --> PCF
```

This architecture allows:

- Replacing the MCU I2C peripheral without modifying the driver.
- Reusing the driver across different platforms.
- Easier unit testing through mocked I2C interfaces.

---

## Directory Structure

Example project organization:

```text
PCF8574/
|
├── Inc/
│   └── PCF8574_Driver.h
|
└── Src/
    └── PCF8574_Driver.c
```

---

## Device Overview

The PCF8574 is an 8-bit I/O expander that communicates through the I2C bus.

Each pin is a quasi-bidirectional I/O.

Unlike a standard GPIO peripheral:

- Writing `0` drives the pin LOW.
- Writing `1` releases the pin HIGH through the internal pull-up mechanism.

This allows each pin to operate as:

- Output.
- Input.

depending on the external circuit and application requirements.

---

## Driver Responsibilities

The PCF8574 driver is responsible for:

- Managing PCF8574 device instances.
- Configuring the I2C context.
- Storing the device address.
- Maintaining the software port shadow.
- Performing read and write transactions.
- Providing bit-level manipulation functions.

The driver is **not responsible** for:

- Configuring MCU I2C peripherals.
- Handling interrupts.
- Managing application GPIO logic.
- Implementing device-specific protocols.

---

## Dependencies

Required dependency:

```text
I2C_Platform_Interface.h
```

The driver communicates with the physical I2C peripheral only through:

```c
PI2C_Write()

PI2C_Read()
```

This abstraction allows the driver to run on different platforms.

---
## Data Structures

---
### PCF8574 Handle

The driver uses a handle structure to represent a PCF8574 device instance.

```c
typedef struct
{
    void*   _i2c_context;
    uint8_t _device_address;
    uint8_t _port_shadow;
    bool    _initialized;

} PCF8574_HandleTypeDef;
```

| Member | Description |
|---|---|
| `_i2c_context` | Platform-specific I2C communication context. |
| `_device_address` | 7-bit I2C address of the PCF8574 device. |
| `_port_shadow` | Software copy of the last successfully written port state. |
| `_initialized` | Internal driver initialization state. |

Members marked as internal driver data should not be modified directly by the application.

---

### Operation Status

The driver uses the following status type:

```c
typedef enum
{
    PCF8574_OPERATION_OK,
    PCF8574_OPERATION_FAIL

} PCF8574_StatusTypeDef;

```
| Status	| Description |
|---------------------|-------------|
| `PCF8574_OPERATION_OK`	|Operation was successfully completed.|
|`PCF8574_OPERATION_FAIL`|	Operation has failed.|

---

## API Reference
---
### PCF8574_Init
- Initializes a PCF8574 device instance.
- Stores the I2C context.
- Stores the device address.
- Initializes the internal port shadow.
- Clears the device output port.

---
#### Function Signature

```c
PCF8574_StatusTypeDef PCF8574_Init(
    PCF8574_HandleTypeDef* Device,
    uint8_t Address,
    void* I2C_Context
);
```
---
#### Parameters

| Parameter | Description |
|-|-|
| `Device` | Pointer to PCF8574 device handle. |
| `Address` | I2C device address. |
| `I2C_Context` | Platform-specific I2C context. |

---
#### Return
| Status	| Description |
|---------------------|-------------|
| `PCF8574_OPERATION_OK`	|Operation was successfully completed.|
|`PCF8574_OPERATION_FAIL`|	Operation has failed.|

---
#### Example

```c
PCF8574_HandleTypeDef IO_Expander;

PCF8574_Init(
    &IO_Expander,
    0x20,
    &hi2c1
);
```

---

### PCF8574_Deinit
- Deinitializes a PCF8574 instance.
- Clears the output port.
- Invalidates the device context.
- Resets initialization state.

---
#### Function Signature
```c
PCF8574_StatusTypeDef PCF8574_Deinit(
    PCF8574_HandleTypeDef* Device
);
```
---
#### Parameters

| Parameter | Description |
|-|-|
| `Device` | Pointer to PCF8574 device handle. |


---
#### Return
| Status	| Description |
|---------------------|-------------|
| `PCF8574_OPERATION_OK`	|Operation was successfully completed.|
|`PCF8574_OPERATION_FAIL`|	Operation has failed.|


---
### PCF8574_WritePort
- Writes an 8-bit value to the complete I/O port.

---
#### Function Signature

```c
PCF8574_StatusTypeDef PCF8574_WritePort(
    PCF8574_HandleTypeDef* Device,
    uint8_t Mask
);
```
---
#### Parameters

| Parameter | Description |
|-|-|
| `Device` | Pointer to PCF8574 device handle. |
| `Mask` | Mask 8-bit value representing the desired port state. |

---
#### Return
| Status	| Description |
|---------------------|-------------|
| `PCF8574_OPERATION_OK`	|Operation was successfully completed.|
|`PCF8574_OPERATION_FAIL`|	Operation has failed.|

---
#### Example

```c
PCF8574_WritePort(
    &IO_Expander,
    0xAA
);
```

The internal port shadow is updated only after a successful I2C transmission.

---
### PCF8574_ClearPort
- Clears all PCF8574 I/O port bits.

---
#### Function Signature

```c
PCF8574_StatusTypeDef PCF8574_ClearPort(
    PCF8574_HandleTypeDef* Device,
);
```
---
#### Parameters

| Parameter | Description |
|-|-|
| `Device` | Pointer to PCF8574 device handle. |

---
#### Return
| Status	| Description |
|---------------------|-------------|
| `PCF8574_OPERATION_OK`	|Operation was successfully completed.|
|`PCF8574_OPERATION_FAIL`|	Operation has failed.|

---
#### Example

```c
PCF8574_WritePort(
    &IO_Expander,
    0xAA
);
```

The internal port shadow is updated only after a successful I2C transmission.

---

### PCF8574_ReadPort
- Reads the current state of all eight I/O pins.

---
#### Function Signature

```c
PCF8574_StatusTypeDef PCF8574_ReadPort(
    PCF8574_HandleTypeDef* Device,
    uint8_t* Buffer
);
```
---
#### Parameters

| Parameter | Description |
|-|-|
| `Device` | Pointer to PCF8574 device handle. |
| `Buffer` | Pointer to the buffer to store 8-bit value representing the readed port state. |

---
#### Return
| Status	| Description |
|---------------------|-------------|
| `PCF8574_OPERATION_OK`	|Operation was successfully completed.|
|`PCF8574_OPERATION_FAIL`|	Operation has failed.|

---
#### Example

```c
uint8_t port_state;

PCF8574_ReadPort(
    &IO_Expander,
    &port_state
);
```

---

### PCF8574_WriteBit
- Sets a specific I/O pin HIGH.
- Internally: 
```text
New Port State = Current Shadow OR Bit Mask
```
---
#### Function Signature
```c
PCF8574_StatusTypeDef PCF8574_WriteBit(
    PCF8574_HandleTypeDef* Device, 
    uint8_t Bit
);

```
---
#### Parameters

| Parameter | Description |
|-|-|
| `Device` | Pointer to PCF8574 device handle. |
| `Bit` | Port bit position to be set. |

---
#### Return
| Status	| Description |
|---------------------|-------------|
| `PCF8574_OPERATION_OK`	|Operation was successfully completed.|
|`PCF8574_OPERATION_FAIL`|	Operation has failed.|

---
### Example

```c
PCF8574_WriteBit(
    &IO_Expander,
    3
);

```

---

### PCF8574_ClearBit
- Clears a specific I/O pin LOW.
- Internally:
```text
New Port State = Current Shadow AND (~Bit Mask)
```
---
#### Function Signature
```c
PCF8574_StatusTypeDef PCF8574_WriteBit(
    PCF8574_HandleTypeDef* Device, 
    uint8_t Bit
);

```
---
#### Parameters

| Parameter | Description |
|-|-|
| `Device` | Pointer to PCF8574 device handle. |
| `Bit` | Port bit position to be cleared. |

---
#### Return
| Status	| Description |
|---------------------|-------------|
| `PCF8574_OPERATION_OK`	|Operation was successfully completed.|
|`PCF8574_OPERATION_FAIL`|	Operation has failed.|

---

#### Example
```c
PCF8574_ClearBit(
    &IO_Expander,
    3
);
```
---

### PCF8574_ReadBit
- Reads the state of a single pin.

---
#### Function Signature
```c
PCF8574_StatusTypeDef PCF8574_ReadBit(
    PCF8574_HandleTypeDef* Device, 
    uint8_t Bit, 
    uint8_t* Buffer
);
```
---
#### Parameters

| Parameter | Description |
|-|-|
| `Device` | Pointer to PCF8574 device handle. |
| `Bit` | Port bit position to be readed. |
| `Buffer` | Pointer to the buffer to store 8-bit value representing the readed port state.|

---
#### Return
| Status	| Description |
|---------------------|-------------|
| `PCF8574_OPERATION_OK`	|Operation was successfully completed.|
|`PCF8574_OPERATION_FAIL`|	Operation has failed.|

---
#### Example
```c
uint8_t state;

PCF8574_ReadBit(
    &IO_Expander,
    3,
    &state
);
```

---

### PCF8574_ToggleBit
- Inverts the current state of a pin.
- Behavior:
```text
LOW  -> HIGH

HIGH -> LOW
```
---
#### Function Signature
```c
PCF8574_StatusTypeDef PCF8574_ToggleBit(
    PCF8574_HandleTypeDef* Device, 
    uint8_t Bit
);
```
---
#### Parameters

| Parameter | Description |
|-|-|
| `Device` | Pointer to PCF8574 device handle. |
| `Bit` | Port bit position to be toggled. |
---
#### Return
| Status	| Description |
|---------------------|-------------|
| `PCF8574_OPERATION_OK`	|Operation was successfully completed.|
|`PCF8574_OPERATION_FAIL`|	Operation has failed.|

---
#### Example

```c
PCF8574_ToggleBit(
    &IO_Expander,
    3
);
```
---
## Port Shadow Mechanism

The PCF8574 driver maintains an internal software copy of the last written port state:

```c
uint8_t _port_shadow;
```

This is required because the PCF8574 does not provide individual bit write operations.

For example:

```text
Current state:

P7 P6 P5 P4 P3 P2 P1 P0

1  0  1  0  0  1  0  1
```

To change only P3:

```text
Read current shadow

Modify P3

Write complete byte again
```

Without the shadow register, modifying one pin would require reading the complete port state before every operation.

Advantages:

- Faster bit operations.
- Avoids unnecessary I2C reads.
- Preserves unrelated pin states.

---
## Operation FLow

---
### Initialization Flow

The typical initialization sequence of the PCF8574 driver is shown below.

```mermaid
flowchart TD

    START([Application Startup])

    I2C_INIT([Initialize I2C Peripheral])

    CREATE([Create PCF8574 Handle])

    INIT["PCF8574_Init()"]

    CONFIG["Store I2C Context<br/>Store Device Address<br/>Initialize Port Shadow"]

    CLEAR["PCF8574_ClearPort()"]

    READY([PCF8574 Ready])


    START --> I2C_INIT

    I2C_INIT --> CREATE

    CREATE --> INIT

    INIT --> CONFIG

    CONFIG --> CLEAR

    CLEAR --> READY
```

After initialization, the device is ready to perform read and write operations.

---

## Usage Example

The following example shows how to initialize and control a PCF8574 device.

```c
#include "PCF8574_Driver.h"
#include "I2C_Platform_Interface.h"

PCF8574_HandleTypeDef IO_Expander;

const int16_t I2C_Address = 0x20;

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_I2C1_Init();

    /*
     * Initialize PCF8574
     *
     * Device address: 0x20
     */
    PCF8574_Init(
        &IO_Expander,
        I2C_Address,
        &hi2c1
    );

    /*
     * Set P0 HIGH
     */
    PCF8574_WriteBit(
        &IO_Expander,
        0
    );

    /*
     * Clear P0 LOW
     */
    PCF8574_ClearBit(
        &IO_Expander,
        0
    );

    /*
     * Toggle P1
     */
    PCF8574_ToggleBit(
        &IO_Expander,
        1
    );
}
```

---

## Design Decisions
---

### Hardware Independent I2C Communication

The driver does not directly depend on any MCU vendor library.

Instead, all communication is performed through:

```text
PCF8574 Driver
        |
        v
I2C Platform Interface
        |
        v
MCU I2C Peripheral
```

Benefits:

- Portability between microcontrollers.
- Easier migration between platforms.
- Simplified unit testing.

---

### Handle-Based Driver Architecture

The driver uses a handle structure:

```c
PCF8574_HandleTypeDef
```

instead of global variables.

Advantages:

- Supports multiple PCF8574 devices.
- Avoids hidden driver state.
- Improves modularity.

Example:

```c
PCF8574_HandleTypeDef LCD_IO;

PCF8574_HandleTypeDef KEYPAD_IO;
```

Both devices can coexist using independent instances.

---

### Software Port Shadow

The PCF8574 hardware does not support individual bit write commands.

Every write operation transfers the complete 8-bit port value.

The driver solves this limitation using:

```c
uint8_t _port_shadow;
```

Example:

Changing only P4:

```text
Current shadow:

1010 0011


Modify P4:


1011 0011


Send complete byte through I2C
```

This prevents unintended changes on other pins.

---

### Driver Initialization Protection

All public operations verify the device initialization state before accessing the hardware.

This prevents invalid access before:

```c
PCF8574_Init()
```

has been executed.

---

### Abstraction Over Device Usage

The PCF8574 driver only provides low-level I/O expansion.

It does not know how the pins are used.

Possible applications:

- Character LCD interfaces.
- Matrix keyboards.
- Relay control.
- LED expanders.
- Sensor interfaces.
- User interface peripherals.

---

## Error Handling

|Function	|Failure Scenario	|Description|
|-----------|-------------------|-----------|
|`PCF8574_Init()`	|Invalid parameters	|Device or I2C_Context pointer is NULL.
|`PCF8574_Init()`	|Clear port failure	|Initial clear port operation failed.
|`PCF8574_Deinit()`	|Invalid parameters	|Device pointer is NULL or device not initialized.
|`PCF8574_WritePort()`	|Invalid parameters	|Device pointer is NULL or device not initialized.
|`PCF8574_WritePort()`	|I2C failure	|I2C write operation timed out or failed.
|`PCF8574_ReadPort()`	|Invalid parameters	|Device or Buffer pointer is NULL, or device not initialized.
|`PCF8574_ReadPort()`	|I2C failure	|I2C read operation timed out or failed.
|`All bit functions`	|Invalid parameters	|Device pointer is NULL or device not initialized.
|`All bit functions`	|Port operation failure	|Underlying WritePort or ReadPort operation failed.

**Important Notes:**
- The PCF8574 is a quasi-bidirectional I/O device. Writing 1 to a pin releases it HIGH through an internal pull-up, allowing it to be used as an input.
- To use a pin as an input, write 1 to that pin and then read the port.
- The driver does not provide explicit input/output configuration. This is managed by the hardware behavior of the PCF8574.
- Multiple PCF8574 devices can be used simultaneously by creating separate handles with different I2C addresses.

---
## Usage Constraints
The following constraints apply when using the PCF8574 driver:

### Initialization Requirements
- The `PCF8574_HandleTypeDef` must be initialized by calling `PCF8574_Init()` before using any other driver operation.
- The I2C context provided to `PCF8574_Init()` must be valid and properly initialized.
- The I2C device address must be correct for the PCF8574 device.
- The I2C context must remain valid for the entire lifetime of the driver instance.

---
### I2C Communication Constraints

- The driver uses blocking I2C communication with a timeout of 10 milliseconds.
- All I2C operations are performed synchronously.
- The driver does not support DMA or interrupt-based I2C transfers.
- The I2C peripheral must be properly configured before calling any driver function.

---
### Port Shadow Mechanism

- The driver maintains an internal software copy of the last successfully written port state (`_port_shadow`).
- The port shadow is updated only after a successful I2C transmission.
- The port shadow is used as the base state for all bit manipulation operations.
- Reading the port does not update the port shadow. The shadow represents the last successfully written state, not the current physical state.
- The port shadow must not be accessed or modified directly by the application.

---
### Bit Manipulation Functions

|Function	|Description|	Bit Range|
|-----------|-----------|------------|
|`PCF8574_WriteBit()`	|`Sets the specified bit to HIGH (1).`|`0 to 7`|
|`PCF8574_ClearBit()`	|`Sets the specified bit to LOW (0).`|	`0 to 8`|
|`PCF8574_ToggleBit()`|	`Inverts the specified bit.`|	`0 to 9`|
|`PCF8574_ReadBit()`	|`Reads the state of the specified bit.`	|`0 to 10`|

---
### Application Responsibilities
- The application is responsible for ensuring the I2C peripheral is initialized before calling `PCF8574_Init()`.
- The application must not access or modify internal driver state (members prefixed with _).
- The application must verify the return status of all driver operations.
- The application must ensure that the I2C bus is not being used by other peripherals simultaneously unless bus arbitration is properly handled.

---
### I2C Device Address

The PCF8574 uses a fixed I2C address determined by the hardware address pins (A0, A1, A2):

|Device	|Address Range	|Address Pins|
|-------|---------------|------------|
|`PCF8574`	|`0x20 to 0x27 (7-bit)`	|`A2 A1 A0`|
|`PCF8574A`	|`0x38 to 0x3F (7-bit)`	|`A2 A1 A0`|

Example address configuration:

- Address pins all connected to GND: `0x20 (PCF8574)` or `0x38 (PCF8574A)`.
- Address pins A0 HIGH, A1 and A2 LOW: `0x21 (PCF8574)` or `0x39 (PCF8574A)`.

---
### Port Write Operations
- Writing to the PCF8574 port updates all eight pins simultaneously.
- When using bit manipulation functions, the driver:
- Modifies the internal port shadow.
- Writes the complete 8-bit value to the device.
- This ensures that unrelated pins are not accidentally modified during bit operations.

---
### Port Read Operations

- Reading the PCF8574 port retrieves the current physical state of the pins.
- The port shadow is not updated during read operations.
- The read value may differ from the port shadow if an external device is driving the pins.

---
## Applications

The PCF8574 driver can be used as a building block for higher-level drivers:

```mermaid
flowchart TD

    APP["Application"]

    LCD["HD44780 LCD Driver"]

    KEYPAD["Matrix Keyboard Driver"]

    LED["LED Controller"]

    RELAY["Relay Controller"]

    PCF["PCF8574 Driver"]

    I2C["I2C Platform Interface"]


    APP --> LCD
    APP --> KEYPAD
    APP --> LED
    APP --> RELAY

    LCD --> PCF
    KEYPAD --> PCF
    LED --> PCF
    RELAY --> PCF

    PCF --> I2C
```

The PCF8574 driver acts as a reusable hardware abstraction layer between application-level modules and the physical I/O expander device.

---
## Limitations

Current implementation limitations:

- No interrupt-based operation.
- No DMA support.
- No automatic I2C bus recovery.
- No hardware fault diagnostics.
- No configurable communication timeout through API.
- Bit position validation is not internally enforced.

---
## License

This module is part of the:

```text
Electronic Lock Project
```

and follows the project's license terms.