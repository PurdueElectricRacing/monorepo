/**
 * @file sd_card.h
 * @brief Logging of received bus messages onto an SD card
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdint.h>
#include "common/rtos/rtos.h"

typedef enum {
    SD_STATE_DISABLED     = 0,
    SD_STATE_INSERT_CARD  = 1,
    SD_STATE_MOUNTING     = 2,
    SD_STATE_OPENING_FILE = 3,
    SD_STATE_READY2LOG    = 4,
    SD_STATE_CLOSING_FILE = 5,
    SD_STATE_UNMOUNTING   = 6,
    SD_STATE_RECOVERING   = 7,
    SD_STATE_FATAL        = 8
} sd_state_t;

static constexpr uint32_t SD_FSM_PERIOD_MS = 50;
void sd_card_periodic(void);
void sd_power_loss_callback(void);

#endif // SD_CARD_H
