/**
 * @file diagnostics.h
 * @brief Common logger for basic CPU and FreeRTOS task profiling data.
 *
 * @author Patrick McNaughton (pmcnaugh@purdue.edu)
 */
#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

#include "can_library/generated/can_types.h"
#include "common/rtos/rtos.h"
#include "task.h"


#define DIAGNOSTICS_MAX_TASKS      16
#define DIAGNOSTICS_TICK_PERIOD_MS 50

typedef void (*diagnostics_send_task_fn_t)(
    uint32_t task_id,
    uint16_t stack_hwm,
    uint8_t cpu_percent,
    diagnostics_task_state_t state
);
typedef void (*diagnostics_send_summary_fn_t)(
    uint32_t sample_count,
    uint16_t task_count,
    uint8_t cpu_percent,
    bool capacity_exceeded
);
typedef void (*diagnostics_send_name_fn_t)(
    uint32_t task_id,
    uint8_t char_index,
    uint8_t character
);

typedef struct {
    diagnostics_send_task_fn_t send_task;
    diagnostics_send_summary_fn_t send_summary;
    diagnostics_send_name_fn_t send_name;
} diagnostics_can_fns_t;

#define DEFINE_DIAGNOSTICS_TASK(SEND_TASK, SEND_SUMMARY, SEND_NAME) \
    static void diagnostics_send_task_wrapper( \
        uint32_t diag_task_id, \
        uint16_t diag_stack_hwm, \
        uint8_t diag_cpu_percent, \
        diagnostics_task_state_t diag_state \
    ) { \
        SEND_TASK(diag_task_id, diag_stack_hwm, diag_cpu_percent, diag_state); \
    } \
    static void diagnostics_send_summary_wrapper( \
        uint32_t diag_sample_count, \
        uint16_t diag_task_count, \
        uint8_t diag_cpu_percent, \
        bool diag_capacity_exceeded \
    ) { \
        SEND_SUMMARY(diag_sample_count, diag_task_count, diag_cpu_percent, diag_capacity_exceeded); \
    } \
    static void diagnostics_send_name_wrapper( \
        uint32_t diag_task_id, \
        uint8_t diag_char_index, \
        uint8_t diag_character \
    ) { \
        SEND_NAME(diag_task_id, diag_char_index, diag_character); \
    } \
    static const diagnostics_can_fns_t diagnostics_can_fns = { \
        .send_task    = diagnostics_send_task_wrapper, \
        .send_summary = diagnostics_send_summary_wrapper, \
        .send_name    = diagnostics_send_name_wrapper, \
    }; \
    static void diagnostics_task_wrapper(void) { \
        diagnostics_periodic(&diagnostics_can_fns); \
    } \
    RTOS_DEFINE_TASK(diagnostics_task_wrapper, DIAGNOSTICS_TICK_PERIOD_MS, TASK_PRIORITY_LOW, STACK_1024)

#define START_DIAGNOSTICS_TASK() RTOS_START_TASK(diagnostics_task_wrapper)

/** Snapshot of a task. Contains id, state, name, cpu usage %, if the cpu usage is valid, stack HWM. */
typedef struct {
    uint32_t task_id;
    char name[configMAX_TASK_NAME_LEN];
    eTaskState state;

    uint32_t stack_min_free_bytes; /**< Fewest bytes that have ever been available on the stack. */
    float cpu_usage_percent;
    bool cpu_usage_valid; /**< True if usage can be computed. */
} diagnostics_task_t;

/** Snapshot of the CPU. Contains system and task stats.  */
typedef struct {
    uint32_t sample_count;
    uint32_t timestamp_ms;
    uint32_t task_count;

    float cpu_percent;
    bool cpu_valid;        /**< True if usage can be computed. */
    bool capacity_exceeded; /**< True if there are more than DIAGNOSTICS_MAX_TASKS.
                                  *   If true no diagnostics data will be recorded. */

    diagnostics_task_t tasks[DIAGNOSTICS_MAX_TASKS];
} diagnostics_snapshot_t;

/** Contains the id of a task, and its last recorded runtime.
 * Used for calculating how long a task has been running.
 */
typedef struct {
    UBaseType_t task_id;
    uint32_t runtime;
} diagnostics_prev_task_time_t;

/** @brief Contains all globals used in diagnostics.c
 *
 * This struct is filled out by diagnostics_periodic()
 * Includes all needed variables for performing profiling.
 */
typedef struct {
    TaskStatus_t raw_tasks[DIAGNOSTICS_MAX_TASKS];
    diagnostics_snapshot_t snapshot;
    uint32_t total_runtime;
    uint32_t previous_total_runtime;
    UBaseType_t previous_task_count;
    diagnostics_prev_task_time_t prev_task_times[DIAGNOSTICS_MAX_TASKS];
    uint8_t tick_count;
} diagnostics_context_t;

/**
 * @brief Periodically samples task statistics and transmits diagnostics over CAN.
 *
 * Samples all tasks once per second (every 20 ticks at 50 ms)
 * Sends at most one task (status + name character) per tick and one board summary per cycle.
 *
 * @param can_fns Optional CAN send callbacks; pass nullptr to sample without transmitting.
 */
void diagnostics_periodic(const diagnostics_can_fns_t *can_fns);

#endif // DIAGNOSTICS_H
