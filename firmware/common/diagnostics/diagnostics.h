#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

/**
 * @file diagnostics.h
 * @brief Common logger for basic CPU and FreeRTOS task profiling data.
 *
 * @author Patrick McNaughton (pmcnaugh@purdue.edu)
 */
#include <stdbool.h>
#include <stdint.h>

#include "common/rtos/rtos.h"
#include "task.h"

#define DIAGNOSTICS_MAX_TASKS 16

#define DEFINE_DIAGNOSTICS_TASK() \
    RTOS_DEFINE_TASK(diagnostics_periodic, 1000, TASK_PRIORITY_LOW, STACK_1024)
#define START_DIAGNOSTICS_TASK() RTOS_START_TASK(diagnostics_periodic)

typedef struct {
    uint32_t task_id;
    char name[configMAX_TASK_NAME_LEN];
    eTaskState state;

    uint32_t stack_min_free_bytes;

    float cpu_usage_percent;
    bool cpu_usage_valid;
} diagnostics_task_t;

typedef struct {
    uint32_t sample_count;
    uint32_t timestamp_ms;
    uint32_t task_count;

    float cpu_usage_percent;
    bool cpu_usage_valid;
    bool task_capacity_exceeded;

    diagnostics_task_t tasks[DIAGNOSTICS_MAX_TASKS];
} diagnostics_snapshot_t;

typedef struct {
    UBaseType_t task_id;
    uint32_t runtime;
} diagnostics_prev_task_time_t;

typedef struct {
    TaskStatus_t raw_tasks[DIAGNOSTICS_MAX_TASKS];
    diagnostics_snapshot_t working_snapshot;
    uint32_t total_runtime;
    uint32_t previous_total_runtime;
    UBaseType_t previous_task_count;
    diagnostics_prev_task_time_t prev_task_times[DIAGNOSTICS_MAX_TASKS];
} diagnostics_context_t;

void diagnostics_periodic(void);

#endif // DIAGNOSTICS_H
