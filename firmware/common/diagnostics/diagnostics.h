#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#define DIAGNOSTICS_MAX_TASKS 16

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

void diagnostics_start(void);

bool diagnostics_get_snapshot(diagnostics_snapshot_t *out);

#endif // DIAGNOSTICS_H
