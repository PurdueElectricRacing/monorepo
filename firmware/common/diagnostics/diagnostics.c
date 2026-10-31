/**
 * @file diagnostics.c
 * @brief Common logger for basic CPU and FreeRTOS task profiling data.
 *
 * @author Patrick McNaughton (pmcnaugh@purdue.edu)
 */
#include "diagnostics.h"

#include <string.h>

#include "common/rtos/rtos.h"
#include "can_library/generated/diagnostics_can.h"


static diagnostics_context_t g_diagnostics_context;

// Calculates utilization as the percentage of the total_delta that is equal to current_runtime - previous_runtime
// Returns true if utilization can be calcualted. 
static bool calculate_utilization(uint32_t current_runtime,
                                      uint32_t previous_runtime,
                                      uint32_t total_delta,
                                      float *cpu_usage) {
    uint32_t task_delta = current_runtime - previous_runtime;
    if (total_delta > 0 && task_delta <= total_delta) {
        *cpu_usage = 100.0f * (float)(task_delta) / (float)(total_delta);
        return true;
    }
    return false;
}

static void begin_snapshot(UBaseType_t count) {
    diagnostics_snapshot_t *snapshot = &g_diagnostics_context.snapshot;

    snapshot->task_count   = (uint32_t)count;
    snapshot->timestamp_ms = (uint32_t)xTaskGetTickCount();
    snapshot->sample_count++;
    snapshot->cpu_percent = 0.0f;
    snapshot->cpu_valid   = false;
    // uxTaskGetSystemState returns zero when the task array is too small.
    snapshot->capacity_exceeded = (count == 0);
}

static void calculate_task_utilization(TaskStatus_t *raw_task, diagnostics_task_t *task, uint32_t total_delta) {
        for (UBaseType_t j = 0; j < g_diagnostics_context.previous_task_count; j++) {
            if (g_diagnostics_context.prev_task_times[j].task_id == raw_task->xTaskNumber) {
                task->cpu_usage_valid =
                    calculate_utilization(raw_task->ulRunTimeCounter,
                                              g_diagnostics_context.prev_task_times[j].runtime,
                                              total_delta,
                                              &task->cpu_usage_percent);
                break;
            }
        }
}

static void save_task_runtimes(UBaseType_t task_count) {
    for (UBaseType_t i = 0; i < task_count; i++) {
        g_diagnostics_context.prev_task_times[i].task_id =
            g_diagnostics_context.raw_tasks[i].xTaskNumber;
        g_diagnostics_context.prev_task_times[i].runtime =
            g_diagnostics_context.raw_tasks[i].ulRunTimeCounter;
    }

    g_diagnostics_context.previous_total_runtime = g_diagnostics_context.total_runtime;
    g_diagnostics_context.previous_task_count    = task_count;
}

static uint8_t encode_cpu_percent(float percent, bool valid) {
    if (!valid || !(percent >= 0.0f && percent <= 100.0f)) {
        return UINT8_MAX;
    }
    return (uint8_t) (percent + 0.5f); // round to nearest percent
}

static uint16_t encode_stack_hwm(uint32_t bytes) {
    if (bytes > UINT16_MAX) {
        return UINT16_MAX;
    }
    return (uint16_t) bytes;
}

static void send_task_names(const diagnostics_snapshot_t *snapshot) {
    for (uint32_t i = 0; i < snapshot->task_count; i++) {
        const diagnostics_task_t *task = &snapshot->tasks[i];

        for (uint8_t index = 0; index < sizeof(task->name); index++) {
            uint8_t character = (uint8_t)task->name[index];

            DIAGNOSTICS_SEND_NAME(
                task->task_id,
                index,
                character
            );

            vTaskDelay(1);

            if (character == 0) {
                break;
            }
        }
    }
}

void diagnostics_periodic(void) {
    // returns the number of tasks, reads them into the raw_tasks array, returns 0 if DIAGNOSTICS_MAX_TASKS < number of running tasks
    UBaseType_t task_count = uxTaskGetSystemState(g_diagnostics_context.raw_tasks,
                                                  DIAGNOSTICS_MAX_TASKS,
                                                  &g_diagnostics_context.total_runtime);
    // total time delta inbetween diagnostics_periodic function calls
    uint32_t total_delta =
        g_diagnostics_context.total_runtime - g_diagnostics_context.previous_total_runtime;
    // used to calculate CPU usage
    TaskHandle_t idle_task = xTaskGetIdleTaskHandle();

    begin_snapshot(task_count);

    for (UBaseType_t i = 0; i < task_count; i++) {
        TaskStatus_t *raw_task   = &g_diagnostics_context.raw_tasks[i];
        diagnostics_task_t *task = &g_diagnostics_context.snapshot.tasks[i];
        // zero out task
        *task = (diagnostics_task_t) {0};

        task->task_id = raw_task->xTaskNumber;
        strncpy(task->name, raw_task->pcTaskName, sizeof(task->name) - 1);
        task->stack_min_free_bytes =
            raw_task->usStackHighWaterMark * sizeof(StackType_t);
        task->state = raw_task->eCurrentState;
        calculate_task_utilization(raw_task, task, total_delta);
        DIAGNOSTICS_SEND_TASK(
            task -> task_id,
            encode_stack_hwm(task -> stack_min_free_bytes),
            encode_cpu_percent(task -> cpu_usage_percent, task -> cpu_usage_valid),
            (uint8_t) task -> state 
        );

        // Calculate overall CPU usage
        if (raw_task->xHandle == idle_task && task->cpu_usage_valid) {
            g_diagnostics_context.snapshot.cpu_percent =
                100.0f - task->cpu_usage_percent;
            g_diagnostics_context.snapshot.cpu_valid = true;
        }
    }
    diagnostics_snapshot_t *snapshot = &g_diagnostics_context.snapshot;
    
    uint16_t reported_task_count =
        snapshot -> task_count > UINT16_MAX ? UINT16_MAX : (uint16_t) snapshot -> task_count;
    DIAGNOSTICS_SEND_SUMMARY(
        snapshot-> sample_count,
        reported_task_count,
        encode_cpu_percent(snapshot->cpu_percent, snapshot->cpu_valid),
        (uint8_t)snapshot->capacity_exceeded
    );
    save_task_runtimes(task_count);
    if (snapshot->sample_count % 10 == 1) {
        send_task_names(snapshot);
    }
}