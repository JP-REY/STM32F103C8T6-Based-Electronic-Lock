/**********************************************************************************************************************************
 * @file    MatrixKeyboard_GPIO_ScanAdapter.c
 * @brief   MatrixKeyboard_GPIO_ScanAdapter.h module implementation.
 *
 * @author  Joao Pedro Rey
 * @version 1.0.0
 * @date    Aug 3, 2026
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 Includes
 **********************************************************************************************************************************/
#include "MatrixKeyboard_GPIO_ScanAdapter.h"

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
static MK_ScanOpStatus_t SelectColumn (void* Context, uint8_t Column);
static MK_ScanOpStatus_t ReadRows     (void* Context, uint32_t* RowsMask);

/**********************************************************************************************************************************
 Private Functions
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Selects the active keyboard column.
 *
 * @details Deactivates all matrix columns and drives only the specified column
 *          to its active logic level. The selected column remains active until
 *          another column is selected by a subsequent call.
 *
 * @param   Context     - Pointer to the GPIO scan adapter context.
 * @param   Column      - Zero-based index of the column to be activated.
 * @param   ActiveLevel - Logic level used to activate the selected column
 *                        (0 for active-low, non-zero for active-high).
 *
 * @note    This function only controls the column drive signals. It does not
 *          read row inputs or perform any key processing.
 *
 * @return  MK_SCAN_OP_OK   - Column successfully selected.
 * @return  MK_SCAN_OP_FAIL - Failed to select the requested column or invalid
 *                            parameters were provided.
 **********************************************************************************************************************************/
static MK_ScanOpStatus_t SelectColumn(void* Context, uint8_t Column)
{
    if(Context == NULL)
    {
        return MK_SCAN_OP_FAIL;
    }

    MK_GPIO_ScanAdapter_t* ctx = Context;

    if(Column >= ctx->ColumnCount || ctx->ActiveLevel > 1)
    {
        return MK_SCAN_OP_FAIL;
    }

    for(uint8_t i = 0; i < ctx->ColumnCount; i++)
    {
        if(ctx->ActiveLevel == 0)
        {
            if (PGPIO_Set(&ctx->Columns[i]) != GPIO_OPERATION_OK)
            {
                return MK_SCAN_OP_FAIL;
            }
        }

        else
        {
            if (PGPIO_Reset(&ctx->Columns[i]) != GPIO_OPERATION_OK)
            {
                return MK_SCAN_OP_FAIL;
            }
        }
    }


    if(ctx->ActiveLevel == 0)
    {
        if(PGPIO_Reset(&ctx->Columns[Column]) != GPIO_OPERATION_OK)
        {
            return MK_SCAN_OP_FAIL;
        }
    }

    else
    {
        if(PGPIO_Set(&ctx->Columns[Column]) != GPIO_OPERATION_OK)
        {
            return MK_SCAN_OP_FAIL;
        }
    }

    return MK_SCAN_OP_OK;
}

/**********************************************************************************************************************************
 * @brief   Reads the current logical state of all keyboard rows.
 *
 * @details Samples every configured row input and packs the resulting
 *          logical states into a bitmask, where bit N corresponds to
 *          row N.
 *
 *          The scan adapter is responsible for translating the physical
 *          GPIO levels into logical row states. Regardless of the
 *          electrical configuration (active-low or active-high), the
 *          returned bitmask shall always use the following convention:
 *
 *              - Bit = 1 : Row is active.
 *              - Bit = 0 : Row is inactive.
 *
 *          This normalization isolates the matrix keyboard driver from
 *          hardware-specific signal polarity.
 *
 * @param   Context  Pointer to the GPIO scan adapter context.
 * @param   RowsMask Pointer to the variable that receives the normalized
 *                   row state bitmask.
 *
 * @note    The adapter is responsible for setting or clearing every bit
 *          in the output mask before returning.
 *
 * @return  MK_SCAN_OP_OK   Row states successfully read.
 * @return  MK_SCAN_OP_FAIL Invalid parameters or row sampling failed.
 **********************************************************************************************************************************/
static MK_ScanOpStatus_t ReadRows(void* Context, uint32_t* RowsMask)
{
    if(Context == NULL || RowsMask == NULL)
    {
        return MK_SCAN_OP_FAIL;
    }

    MK_GPIO_ScanAdapter_t* ctx = Context;

    for(uint8_t row = 0U; row < ctx->RowCount; row++)
    {
        GPIO_Level_t level = PGPIO_GetLevel(&ctx->Rows[row]);

        bool pressed = (level == ctx->ActiveLevel);

        if(pressed)
        {
            *RowsMask |= (1UL << row);
        }

        else
        {
            *RowsMask &=  ~(1UL << row);
        }
    }

    return MK_SCAN_OP_OK;
}

/**********************************************************************************************************************************
 Functions
 **********************************************************************************************************************************/
/**********************************************************************************************************************************
 * @brief   Initializes the GPIO scan adapter.
 *
 * @details Initializes a Matrix Keyboard scan interface instance with the GPIO-
 *          based scan adapter implementation. The supplied context is stored in
 *          the scan interface and the GPIO-specific callback functions are
 *          assigned to the interface.
 *
 *          This function assumes that all GPIO peripherals have already been
 *          configured by the application.
 *
 * @param   Scan    - Pointer to the scan interface instance to initialize.
 * @param   Context - Pointer to the GPIO scan adapter context containing the
 *                    row and column GPIO descriptors.
 *
 * @note    The context object and the GPIO descriptor arrays it references
 *          shall remain valid for the entire lifetime of the scan interface.
 *
 * @return  MK_SCAN_OP_OK   - Adapter successfully initialized.
 * @return  MK_SCAN_OP_FAIL - Initialization failed due to invalid parameters.
 **********************************************************************************************************************************/
MK_ScanOpStatus_t MK_GPIO_ScanAdapterInit(MK_ScanInterface_t *Scan, MK_GPIO_ScanAdapter_t *Context)
{
    if (Scan == NULL || Context == NULL)
    {
        return MK_SCAN_OP_FAIL;
    }

    if((Context->Columns     == NULL) ||
       (Context->Rows        == NULL) ||
       (Context->ColumnCount == 0U  ) ||
       (Context->RowCount    == 0U ))
    {
        return MK_SCAN_OP_FAIL;
    }

    Scan->Context      = Context;
    Scan->SelectColumn = SelectColumn;
    Scan->ReadRows     = ReadRows;

    return MK_SCAN_OP_OK;
}





























