/**
 * @file diagnostics.c
 * @brief Common logger for basic CPU and FreeRTOS task profiling data.
 *
 * @author Patrick McNaughton (pmcnaugh@purdue.edu)
 */
#include "diagnostics.h"

#include <string.h>

#include "common/rtos/rtos.h"

static diagnostics_context_t g_diagnostics_context;

/**                                                                                                                      
    * @brief Calculates task CPU utilization.                                                    
    *                                                                                                                       
    * @param current_runtime Current cumulative task runtime.                                                               
    * @param previous_runtime Previous cumulative task runtime.                                                        
    * @param total_delta Total elapsed runtime between samples (same units as current and previous runtime).                                          
    * @param[out] cpu_usage Pointer receiving utilization as a percentage                                          
    *                      (0–100). Left unchanged if the calculation is invalid. Must be non-NULL.                                     
    *                                                                                                                       
    * @return true if utilization was calculated; false if total_delta is zero                                              
    *         or the task runtime delta exceeds total_delta.                                                                
    */
static bool calculate_cpu_utilization(uint32_t current_runtime,
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
                    calculate_cpu_utilization(raw_task->ulRunTimeCounter,
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

void diagnostics_periodic(void) {
    UBaseType_t task_count = uxTaskGetSystemState(g_diagnostics_context.raw_tasks,
                                                  DIAGNOSTICS_MAX_TASKS,
                                                  &g_diagnostics_context.total_runtime);
    // total time delta inbetween diagnostics_periodic function calls
    uint32_t total_delta =
        g_diagnostics_context.total_runtime - g_diagnostics_context.previous_total_runtime;
    TaskHandle_t idle_task = xTaskGetIdleTaskHandle();

    begin_snapshot(task_count);

    for (UBaseType_t i = 0; i < task_count; i++) {
        TaskStatus_t *raw_task   = &g_diagnostics_context.raw_tasks[i];
        diagnostics_task_t *task = &g_diagnostics_context.snapshot.tasks[i];
        // zero out task
        *task = (diagnostics_task_t) {0};

        task->task_id = raw_task->xTaskNumber;
        strcpy(task->name, raw_task->pcTaskName);
        task->stack_min_free_bytes =
            raw_task->usStackHighWaterMark * sizeof(StackType_t);
        task->state = raw_task->eCurrentState;
        calculate_task_utilization(raw_task, task, total_delta);

        // Calculate overall CPU usage
        if (raw_task->xHandle == idle_task && task->cpu_usage_valid) {
            g_diagnostics_context.snapshot.cpu_percent =
                100.0f - task->cpu_usage_percent;
            g_diagnostics_context.snapshot.cpu_valid = true;
        }
    }
    save_task_runtimes(task_count);
}