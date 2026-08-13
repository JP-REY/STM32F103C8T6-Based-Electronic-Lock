/**********************************************************************************************************************************
 * @file    MatrixKeyboard_ScanInterface.h
 * @brief   Matrix keyboard hardware scan abstraction interface.
 *
 * @details Defines the hardware abstraction contract used by the Matrix Keyboard driver to
 *          perform matrix scanning independently of the underlying platform.
 *
 *          The keyboard driver is responsible for:
 *              - Sequentially selecting each matrix column.
 *              - Sampling the logical state of every row.
 *              - Updating the key state machines.
 *              - Performing debounce filtering.
 *              - Detecting click, double-click and long-click events.
 *
 *          The scan adapter is responsible only for:
 *              - Selecting the requested matrix column.
 *              - Reading the electrical state of the matrix rows.
 *              - Translating the hardware-specific electrical levels into a normalized
 *                logical representation expected by the driver.
 *
 *          The row bit mask returned by ReadRows() shall always follow the convention:
 *
 *              Bit = 1  -> Corresponding key is physically pressed.
 *              Bit = 0  -> Corresponding key is physically released.
 *
 *          This convention shall be preserved regardless of the electrical implementation
 *          (active-low, active-high, GPIO, I/O expander, shift register, etc.).
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 3, 2026
 **********************************************************************************************************************************/

#ifndef LIBS_COMPONENTS_MATRIXKEYBOARD_INC_MATRIXKEYBOARD_SCANINTERFACE_H_
#define LIBS_COMPONENTS_MATRIXKEYBOARD_INC_MATRIXKEYBOARD_SCANINTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Matrix keyboard scan interface operation status.
 *
 * @details Defines the return status of every operation provided by the matrix scan
 *          interface.
 *
 *          These values indicate only whether the requested hardware scan operation was
 *          successfully executed by the scan adapter. They do not convey any information
 *          about key states, debounce processing or event generation.
 *
 * @note    Every callback defined by the scan interface shall return one of these values
 *          to indicate the execution status of the requested operation.
 **********************************************************************************************************************************/
typedef enum
{
    MK_SCAN_OP_OK,      /* << Scan operation completed successfully. >> */
    MK_SCAN_OP_FAIL     /* << Scan operation could not be completed. >> */

}MK_ScanOpStatus_t;

/**********************************************************************************************************************************
 * @brief   Matrix keyboard hardware scan abstraction.
 *
 * @details This interface isolates the keyboard driver from the underlying hardware used
 *          to scan the matrix.
 *
 *          The keyboard driver is responsible only for:
 *
 *              - Selecting each column sequentially;
 *              - Sampling the row states;
 *              - Updating and processing key events;
 *              - Generating logical key actions.
 *
 *          The scan adapter is responsible only for:
 *
 *              - Selecting the requested column;
 *              - Reading the electrical state of every row.
 *
 *          Debounce filtering, key state transitions, click detection and event generation
 *          are intentionally outside the scope of this interface.
 **********************************************************************************************************************************/
typedef struct
{
    /**********************************************************************************************************************************
     * @brief   Selects the matrix column to be scanned.
     *
     * @details Configures the underlying hardware so that the specified column becomes the
     *          active column for the next row sampling operation.
     *
     *          The implementation is hardware dependent and may use GPIOs, I/O expanders,
     *          shift registers or any other mechanism required by the target platform.
     *
     *          After a successful call, the requested column shall be the only active column
     *          participating in the scan operation.
     *
     * @param   Context     - Pointer to the implementation-specific context associated with the
     *                        scan adapter instance.
     * @param   Column      - Zero-based index of the column to be selected.
     *
     * @note    This function only prepares the hardware for scanning. It shall not perform
     *          key processing, debounce filtering or event generation.
     *
     * @return  MK_SCAN_OP_OK   - Column successfully selected.
     * @return  MK_SCAN_OP_FAIL - Failed to select the requested column.
     **********************************************************************************************************************************/
    MK_ScanOpStatus_t (*SelectColumn) (void* Context, uint8_t Column);

    /**********************************************************************************************************************************
     * @brief   Samples the logical state of all keyboard rows.
     *
     * @details Reads every row while the previously selected column remains active and returns
     *          the sampled state through a bit mask.
     *
     *          The implementation shall normalize the electrical levels so that every bit
     *          follows the convention below:
     *
     *              Bit = 1  -> Corresponding key is physically pressed.
     *              Bit = 0  -> Corresponding key is physically released.
     *
     *          Row mapping:
     *
     *              bit0 -> Row 0
     *              bit1 -> Row 1
     *              bit2 -> Row 2
     *              ...
     *
     *          The driver relies exclusively on this normalized representation and shall not
     *          require any knowledge of the underlying electrical implementation.
     *
     * @param   Context     - Pointer to the implementation-specific context associated with the
     *                        scan adapter instance.
     * @param   RowsMask    - Pointer receiving the normalized row state bit mask.
     *
     * @note    This function performs only the acquisition of the current matrix state.
     *          Debounce filtering, state transitions and event generation are outside the
     *          scope of this interface.
     *
     * @return  MK_SCAN_OP_OK   - Row states successfully sampled.
     * @return  MK_SCAN_OP_FAIL - Failed to acquire the row states.
     **********************************************************************************************************************************/
    MK_ScanOpStatus_t (*ReadRows) (void* Context, uint32_t* RowsMask);

    /**********************************************************************************************************************************
     * @brief   Pointer to the implementation-specific context.
     *
     * @details Stores any hardware-specific resources required by the scan adapter
     *          implementation.
     *
     *          The driver treats this pointer as an opaque object and never accesses its
     *          contents directly.
     *
     *          Typical examples include:
     *              - GPIO descriptors.
     *              - I/O expander handles.
     *              - Platform driver instances.
     *              - Private adapter configuration data.
     *
     *          The same pointer is supplied unchanged to every callback defined by this
     *          interface.
     **********************************************************************************************************************************/
    void* Context;

}MK_ScanInterface_t;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* LIBS_COMPONENTS_MATRIXKEYBOARD_INC_MATRIXKEYBOARD_SCANINTERFACE_H_ */
