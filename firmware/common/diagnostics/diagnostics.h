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
/** Define diagnostics task with FreeRTOS */
#define DEFINE_DIAGNOSTICS_TASK() \
    RTOS_DEFINE_TASK(diagnostics_periodic, 1000, TASK_PRIORITY_LOW, STACK_1024)
/** Start previously defined diagnostic task */
#define START_DIAGNOSTICS_TASK() RTOS_START_TASK(diagnostics_periodic)

/** Snapshot of a task. Contains id, state, name, cpu usage %, if the cpu usage is valid, stack HWM. */
typedef struct {
    uint32_t task_id;
    char name[configMAX_TASK_NAME_LEN];
    eTaskState state;

    uint32_t stack_min_free_bytes; /** Fewest bytes that have ever been available on the stack. */

    float cpu_usage_percent;
    bool cpu_usage_valid; /** True if usage can be computed. */
} diagnostics_task_t;

/** Snapshot of the CPU. Contains system and task stats.  */
typedef struct {
    uint32_t sample_count;
    uint32_t timestamp_ms;
    uint32_t task_count;

    float cpu_usage_percent;
    bool cpu_usage_valid; /** True if usage can be computed. */
    bool
        task_capacity_exceeded; /** True if there are more than DIAGNOSTICS_MAX_TASKS. If true no diagnostics data will be recorded. */

    diagnostics_task_t tasks[DIAGNOSTICS_MAX_TASKS];
} diagnostics_snapshot_t;

/** Contains the id of a task, and its last recorded runtime.
*   Used for calculating how long a task has been running.
*/
typedef struct {
    UBaseType_t task_id;
    uint32_t runtime;
} diagnostics_prev_task_time_t;

/** @brief Contains all globals used in diagnostics.c 
* 
*   This struct is filled out by diagnostics_periodic()
*   Includes all needed variables for performing profiling.
*/
typedef struct {
    TaskStatus_t raw_tasks[DIAGNOSTICS_MAX_TASKS];
    diagnostics_snapshot_t working_snapshot;
    uint32_t total_runtime;
    uint32_t previous_total_runtime;
    UBaseType_t previous_task_count;
    diagnostics_prev_task_time_t prev_task_times[DIAGNOSTICS_MAX_TASKS];
} diagnostics_context_t;

/**
* @brief Periodic function that collects profiling data
*
*  Fills out a diagnostics_context_t global struct.
*/
void diagnostics_periodic(void);

#endif // DIAGNOSTICS_H
