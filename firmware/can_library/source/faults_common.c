/**
 * @file faults_common.c
 * @brief Event-based FIDR system
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "can_library/faults_common.h"

#include "can_library/generated/can_router.h"
#include "can_library/generated/fault_data.h"

#ifdef FAULT_LIB_ENABLED

static uint16_t fault_counters[NUM_FAULT_PRIOS];

#ifdef MY_FAULT_START
static_assert(MY_FAULT_START <= MY_FAULT_END);
static_assert(MY_FAULT_END < TOTAL_NUM_FAULTS);
#else
#error "Fault library misconfigured"
#endif

bool is_latched(fault_id_t fault_id) {
    if (fault_id >= TOTAL_NUM_FAULTS) {
        return false;
    }

    fault_state_t state = faults[fault_id].state;

    return state == FAULT_STATE_LATCHED || state == FAULT_STATE_RECOVERING;
}

bool is_clear(fault_id_t fault_id) {
    if (fault_id >= TOTAL_NUM_FAULTS) {
        return false;
    }

    fault_state_t state = faults[fault_id].state;

    return state == FAULT_STATE_CLEAR || state == FAULT_STATE_PENDING;
}

void update_fault(fault_id_t fault_id, float value) {
    if ((fault_id < MY_FAULT_START) || (fault_id > MY_FAULT_END)) {
        return; // dont allow update of a fault not owned by this node
    }

    fault_t *fault = &faults[fault_id];
    uint32_t now   = xTaskGetTickCount();
    // min is inclusive, max is exclusive, so the healthy range is [min, max)
    bool is_out_of_bounds = (value >= fault->max_value) || (value < fault->min_value);
    if (value != value) {
        is_out_of_bounds = true; // NaN values are considered out of bounds
    }

    // Implementation of the FSM diagram in docs/can_library/README.md
    // I know this FSM is not "mathematically pure", but it must be implemented in this way
    // to satisfy timing constraints/defensive programming
    switch (fault->state) {
        case FAULT_STATE_CLEAR: {
            if (is_out_of_bounds) {
                fault->state         = FAULT_STATE_PENDING;
                fault->start_time_ms = now;
            }
            break;
        }
        case FAULT_STATE_PENDING: {
            if (!is_out_of_bounds) {
                fault->state = FAULT_STATE_CLEAR;
                break;
            }

            uint32_t elapsed = now - fault->start_time_ms;
            // do not update the start_time
            if (elapsed >= fault->latch_time_ms) {
                fault->state = FAULT_STATE_LATCHED;
            }
            break;
        }
        case FAULT_STATE_LATCHED: {
            if (!is_out_of_bounds) {
                fault->state         = FAULT_STATE_RECOVERING;
                fault->start_time_ms = now;
            }

            break;
        }
        case FAULT_STATE_RECOVERING: {
            if (is_out_of_bounds) {
                fault->state = FAULT_STATE_LATCHED;
                break;
            }

            uint32_t elapsed = now - fault->start_time_ms;
            // do not update the start_time
            if (elapsed >= fault->unlatch_time_ms) {
                fault->state = FAULT_STATE_CLEAR;
            }
            break;
        }
    }
}

void fault_library_periodic() {
    // Reset priority counters
    for (unsigned int i = 0; i < NUM_FAULT_PRIOS; i++) {
        fault_counters[i] = 0;
    }

    // Count latched faults
    for (unsigned int i = 0; i < TOTAL_NUM_FAULTS; i++) {
        if (is_latched(i)) {
            fault_counters[faults[i].priority]++;
        }
    }

    // Broadcast our local state to the rest of the car
    tx_fault_sync();
}

bool is_warning_latched() {
    return fault_counters[FAULT_PRIO_WARNING] > 0;
}

bool is_error_latched() {
    return fault_counters[FAULT_PRIO_ERROR] > 0;
}

bool is_fatal_latched() {
    return fault_counters[FAULT_PRIO_FATAL] > 0;
}

bool is_any_latched() {
    return is_warning_latched() || is_error_latched() || is_fatal_latched();
}

#ifdef HAS_FAULT_STRINGS
const char *get_fault_string(fault_id_t fault_id) {
    if (fault_id >= TOTAL_NUM_FAULTS)
        return nullptr;
    return fault_strings[fault_id];
}
#endif

bool is_curr_mcu_latched() {
#ifdef MY_FAULT_START
    for (unsigned int i = MY_FAULT_START; i <= MY_FAULT_END; i++) {
        if (is_latched(i))
            return true;
    }
#endif
    return false;
}

bool is_other_mcus_latched() {
    for (unsigned int i = 0; i < TOTAL_NUM_FAULTS; i++) {
#ifdef MY_FAULT_START
        if (i >= MY_FAULT_START && i <= MY_FAULT_END)
            continue;
#endif
        if (is_latched(i))
            return true;
    }
    return false;
}

#endif // FAULT_LIB_ENABLED
