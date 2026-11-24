/**
 * @file faults_common.h
 * @brief Event-based FIDR system
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef FAULTS_COMMON_H
#define FAULTS_COMMON_H

#include <stdint.h>
#include "can_library/generated/can_types.h"

typedef enum : uint8_t {
    FAULT_PRIO_WARNING,
    FAULT_PRIO_ERROR,
    FAULT_PRIO_FATAL,
    NUM_FAULT_PRIOS
} fault_priority_t;

typedef enum : uint8_t {
    FAULT_STATE_CLEAR = 0,
    FAULT_STATE_PENDING = 1,
    FAULT_STATE_LATCHED = 2,
    FAULT_STATE_RECOVERING = 3
} fault_state_t;

typedef struct {
    uint32_t start_time_ms;
    float max_value;
    float min_value;
    uint16_t latch_time_ms;
    uint16_t unlatch_time_ms;
    fault_state_t state;
    fault_priority_t priority;
} fault_t;

// API
void update_fault(fault_id_t fault_id, float value);
bool is_latched(fault_id_t fault_id);
bool is_clear(fault_id_t fault_id);
void fault_library_periodic();

bool is_warning_latched();
bool is_error_latched();
bool is_fatal_latched();
bool is_curr_mcu_latched();
bool is_other_mcus_latched();
bool is_any_latched();

#ifdef HAS_FAULT_STRINGS
const char* get_fault_string(fault_id_t fault_id);
#endif

#endif // FAULTS_COMMON_H
