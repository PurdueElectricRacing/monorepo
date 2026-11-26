#ifndef FAULT_DATA_H
#define FAULT_DATA_H

/**
 * @file fault_data.h
 * @brief Auto-generated global fault definitions and limits.
 * @note This file is auto-generated; do not modify manually.
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>
#include "can_library/generated/can_types.h"

// Node Configuration (Activation & Ownership)
#ifdef CAN_NODE_A_BOX
#define FAULT_LIB_ENABLED
#define MY_FAULT_START (FAULT_ID_PACK_OVERTEMP)
#define MY_FAULT_END (FAULT_ID_PACK_EMPTY)
#endif
#ifdef CAN_NODE_DASHBOARD
#define FAULT_LIB_ENABLED
#define MY_FAULT_START (FAULT_ID_APPS_WIRING_T1)
#define MY_FAULT_END (FAULT_ID_BSE)
#define HAS_FAULT_STRINGS
#endif
#ifdef CAN_NODE_MAIN_MODULE
#define FAULT_LIB_ENABLED
#define MY_FAULT_START (FAULT_ID_SDC1_IMD)
#define MY_FAULT_END (FAULT_ID_PRECHARGE_INCOMPLETE)
#endif
#ifdef CAN_NODE_PDU
#define FAULT_LIB_ENABLED
#define MY_FAULT_START (FAULT_ID_SDC_SWITCH_FAULT)
#define MY_FAULT_END (FAULT_ID_EMETER_SWITCH_FAULT)
#endif
#ifdef CAN_NODE_TORQUE_VECTOR
#define FAULT_LIB_ENABLED
#define MY_FAULT_START (FAULT_ID_GPS_INVALID_FIX)
#define MY_FAULT_END (FAULT_ID_GPS_INVALID_UTC)
#endif

#include "can_library/faults_common.h"

#define TOTAL_NUM_FAULTS (55U)

// Global fault state and attribute arrays
extern fault_t faults[TOTAL_NUM_FAULTS];

#ifdef HAS_FAULT_STRINGS
extern const char* const fault_strings[TOTAL_NUM_FAULTS];
#endif

// Transmission Helpers
void tx_fault_sync(void);
void tx_fault_event(fault_id_t idx, uint16_t value);

#endif // FAULT_DATA_H
