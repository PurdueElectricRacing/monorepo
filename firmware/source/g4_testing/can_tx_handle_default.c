/**
 * @file can_tx_handle_default.c
 * @brief Weak default CAN TX task handle.
 *
 * Every bench test links the CAN node library, but only CAN tests start the CAN
 * tasks. DEFINE_CAN_TASKS() provides the real handle when a CAN test is
 * selected; this weak default keeps the other tests linking.
 */
#include <stddef.h>

#include "common/rtos/rtos.h"

__attribute__((weak)) TaskHandle_t CAN_tx_update_handle = NULL;
