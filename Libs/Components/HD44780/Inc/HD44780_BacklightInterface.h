/**********************************************************************************************************************************
 * @file    HD44780_BacklightInterface.h
 * @brief   Abstract backlight control interface for the HD44780 HD44780 driver.
 *
 * @details Defines the abstraction layer used by the HD44780 driver to control the
 *          HD44780 module backlight independently of the underlying hardware implementation.
 *
 *          Although the backlight is not controlled by the HD44780 controller itself,
 *          this interface is provided as a convenience abstraction for complete HD44780
 *          module management.
 *
 *          Different hardware implementations (GPIO, PWM, I/O expanders, etc.) can
 *          expose a common API through this interface.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Jul 29, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_COMPONENTS_HD44780_INC_HD44780_BACKLIGHTINTERFACE_H_
#define LIBS_COMPONENTS_HD44780_INC_HD44780_BACKLIGHTINTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "stm32f4xx.h"
#include "stdint.h"
#include "stdbool.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/

/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Status returned by a backlight control operation.
 *
 * @details Indicates whether a backlight operation completed successfully.
 **********************************************************************************************************************************/
typedef enum
{
    HD44780_BACKLIGHT_OP_OK,
    HD44780_BACKLIGHT_OP_FAIL

}HD44780_BacklightOpStatusTypeDef;

/**********************************************************************************************************************************
 * @brief   Generic backlight control interface.
 *
 * @details Provides a hardware-independent interface used by the HD44780 driver
 *          to control the HD44780 module backlight.
 *
 *          Each function pointer represents a backlight operation that may be
 *          implemented by different hardware adapters such as GPIO outputs,
 *          PWM peripherals, I/O expanders, etc.
 *
 *          The Context member stores an opaque pointer to the implementation-
 *          specific context. This pointer is forwarded unchanged to every
 *          interface callback, allowing each adapter to access the resources
 *          required to perform the requested operation.
 *
 * @note    The HD44780 driver interacts exclusively through this interface and
 *          remains completely independent of the underlying hardware implementation.
 **********************************************************************************************************************************/
typedef struct
{
   /**********************************************************************************************************************************
     * @brief   Turns the HD44780 module backlight on.
     *
     * @details Requests the underlying hardware implementation to enable the
     *          backlight.
     *
     * @param   Context - Pointer to the implementation-specific context required
     *                    by the backlight adapter.
     *
     * @return  Status indicating whether the operation completed successfully.
    **********************************************************************************************************************************/
    HD44780_BacklightOpStatusTypeDef (*TurnOn)(void* Context);

    /**********************************************************************************************************************************
     * @brief   Turns the HD44780 module backlight off.
     *
     * @details Requests the underlying hardware implementation to disable the
     *          backlight.
     *
     * @param   Context - Pointer to the implementation-specific context required
     *                    by the backlight adapter.
     *
     * @return  Status indicating whether the operation completed successfully.
     **********************************************************************************************************************************/
    HD44780_BacklightOpStatusTypeDef (*TurnOff)(void* Context);

    /**********************************************************************************************************************************
     * @brief   Sets the HD44780 module backlight brightness.
     *
     * @details Requests the underlying hardware implementation to adjust the
     *          backlight brightness to the specified percentage.
     *
     *          Implementations that do not support variable brightness may treat
     *          any non-zero percentage as fully enabled.
     *
     * @param   Context         - Pointer to the implementation-specific context required
     *                            by the backlight adapter.
     * @param   Level           - Desired brightness level expressed as a
     *                            percentage in the range from 0 to 100.
     *
     * @return  Status indicating whether the operation completed successfully.
     **********************************************************************************************************************************/
    HD44780_BacklightOpStatusTypeDef (*SetBrightness)(void* Context, uint16_t Level);

    /**********************************************************************************************************************************
     * @brief   Gets the HD44780 module backlight brightness level.
     *
     * @details Requests the underlying hardware implementation to return the
     *          current backlight brightness level configured in the adapter.
     *
     *          The returned value represents the brightness setting maintained by
     *          the backlight implementation. It does not necessarily represent the
     *          actual optical brightness of the HD44780 module, since the perceived
     *          brightness depends on hardware characteristics such as the LED
     *          driver circuit, supply voltage and environmental conditions.
     *
     * @param   Context - Pointer to the implementation-specific context required
     *                    by the backlight adapter.
     *
     * @note    Implementations without variable brightness control may return a
     *          binary state representation, such as 0 for disabled and 100 for
     *          enabled.
     *
     * @return  Current backlight brightness level expressed as a percentage from
     *          0 to 100.
     **********************************************************************************************************************************/
    uint16_t (*GetBrightness)(void* Context);

    /**********************************************************************************************************************************
     * @brief   Implementation-specific backlight context.
     *
     * @details Opaque pointer forwarded unchanged to every interface callback.
     *          The concrete implementation defines the type and contents of this
     *          context, which may contain peripheral handles, GPIO information,
     *          timer configuration, or any other resources required to control the
     *          HD44780 module backlight.
     **********************************************************************************************************************************/
    void* Context;

}HD44780_BacklightInterfaceTypeDef;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_HD44780_INC_HD44780_BACKLIGHTINTERFACE_H_ */
