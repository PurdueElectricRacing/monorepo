#include "diagnostics.h"
#include <string.h>
#include "common/rtos/rtos.h"

static TaskStatus_t raw_tasks[DIAGNOSTICS_MAX_TASKS];
static diagnostics_snapshot_t latest_snapshot;
static diagnostics_snapshot_t working_snapshot;
static bool snapshot_ready = false;

static void diagnostics_periodic(void);

RTOS_DEFINE_TASK(diagnostics_periodic, 1000, TASK_PRIORITY_LOW, STACK_1024);

void diagnostics_start(void) {
    TaskHandle_t handle = RTOS_START_TASK(diagnostics_periodic);
    configASSERT(handle != NULL);
}

static void diagnostics_periodic(void) {
    configRUN_TIME_COUNTER_TYPE total_runtime = 0;
    UBaseType_t count = uxTaskGetSystemState(raw_tasks, DIAGNOSTICS_MAX_TASKS, &total_runtime);

    working_snapshot.task_count = (uint32_t)count;
    working_snapshot.timestamp_ms = (uint32_t) xTaskGetTickCount();
    working_snapshot.task_capacity_exceeded = false;

    for (UBaseType_t i = 0; i < count; i++) {
      TaskStatus_t *source = &raw_tasks[i];
      diagnostics_task_t *destination = &working_snapshot.tasks[i];

      *destination = (diagnostics_task_t){0};

      destination->task_id = (uint32_t)source->xTaskNumber;
      destination->state = source->eCurrentState;

      destination->stack_min_free_bytes =
          (uint32_t)source->usStackHighWaterMark
          * (uint32_t)sizeof(StackType_t);

      strncpy(
          destination->name,
          source->pcTaskName,
          sizeof(destination->name) - 1
      );

      destination->cpu_usage_valid = false;
    }
}