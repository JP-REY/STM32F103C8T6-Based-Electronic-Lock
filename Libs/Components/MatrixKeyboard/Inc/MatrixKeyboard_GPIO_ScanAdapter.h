/**********************************************************************************************************************************
 * @file    MatrixKeyboard_GPIO_ScanAdapter.h
 * @brief   GPIO-based scan adapter for the Matrix Keyboard driver.
 *
 * @details This module provides an implementation of the Matrix Keyboard scan interface
 *          using GPIO pins as the underlying hardware.
 *
 *          The adapter translates the generic scan operations defined by
 *          MatrixKeyboard_ScanInterface into GPIO read and write operations through
 *          the GPIO platform abstraction, allowing the Matrix Keyboard driver to
 *          operate independently of the target MCU or hardware library.
 *
 *          The application is responsible for providing the GPIO descriptors for
 *          all matrix rows and columns, as well as their respective counts.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 3, 2026
 **********************************************************************************************************************************/

#ifndef COMPONENTS_MATRIXKEYBOARD_INC_MATRIXKEYBOARD_GPIO_SCANADAPTER_H_
#define COMPONENTS_MATRIXKEYBOARD_INC_MATRIXKEYBOARD_GPIO_SCANADAPTER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "MatrixKeyboard_ScanInterface.h"
#include "GPIO_Platform_Interface.h"

/**********************************************************************************************************************************
 Macros
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Types
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   GPIO configuration context used by the Matrix Keyboard GPIO scan adapter.
 *
 * @details Holds the GPIO descriptors associated with the matrix rows and columns.
 *          These descriptors are supplied by the application and are used by the
 *          adapter to perform the hardware-specific scan operations.
 *
 *          The arrays shall remain valid for the entire lifetime of the adapter.
 **********************************************************************************************************************************/
typedef struct
{
    /* << Array containing the GPIO descriptors associated with the matrix columns. >> */ GPIO_HandleTypeDef *Columns;
    /* << Number of GPIO descriptors available in the Columns array.                >> */ uint8_t             ColumnCount;
    /* << Array containing the GPIO descriptors associated with the matrix rows.    >> */ GPIO_HandleTypeDef *Rows;
    /* << Number of GPIO descriptors available in the Rows array.                   >> */ uint8_t             RowCount;
    /* << Logic level that drives gpio into its active state                        >> */ GPIO_LevelTypeDef   ActiveLevel;

} MK_GPIO_ScanAdapterContextTypeDef;

/**********************************************************************************************************************************
 Data
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Function Prototypes
 **********************************************************************************************************************************/
MK_ScanOpStatusTypeDef MK_GPIO_ScanAdapterInit(MK_ScanInterfaceTypeDef* Scan, MK_GPIO_ScanAdapterContextTypeDef* Context);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_MATRIXKEYBOARD_INC_MATRIXKEYBOARD_GPIO_SCANADAPTER_H_ */
