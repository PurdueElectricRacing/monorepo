#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

#include "common/rtos/rtos.h"
#include "task.h"

void diagnostics_periodic(void);

#define DIAGNOSTICS_MAX_TASKS 16

#define DEFINE_DIAGNOSTICS_TASK() RTOS_DEFINE_TASK(diagnostics_periodic, 1000, TASK_PRIORITY_LOW, STACK_1024)
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


#endif // DIAGNOSTICS_H
