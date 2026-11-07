/**
 * @file can_tx_handle_default.c
 * @brief Weak default for the CAN TX task handle.
 *
 * The diagnostics code is linked into every g4_testing image, so its CAN send
 * calls pull can_tx.c in even for tests like blinky that never bring up CAN. can_tx.c needs
 * CAN_tx_update_handle, which only DEFINE_CAN_TASKS() defines, so tests
 * without it don't link. This weak handle is only used when CAN isn't defined.
 */
#include <stddef.h>

#include "common/rtos/rtos.h"

__attribute__((weak)) TaskHandle_t CAN_tx_update_handle = NULL;
