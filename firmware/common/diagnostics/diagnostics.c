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

void diagnostics_periodic(void) {
    UBaseType_t count = uxTaskGetSystemState(g_diagnostics_context.raw_tasks,
                                             DIAGNOSTICS_MAX_TASKS,
                                             &g_diagnostics_context.total_runtime);
    // total time delta inbetween diagnostics_periodic function calls
    uint32_t total_delta =
        g_diagnostics_context.total_runtime - g_diagnostics_context.previous_total_runtime;
    TaskHandle_t idle_task                              = xTaskGetIdleTaskHandle();
    g_diagnostics_context.working_snapshot.task_count   = (uint32_t)count;
    g_diagnostics_context.working_snapshot.timestamp_ms = (uint32_t)xTaskGetTickCount();
    g_diagnostics_context.working_snapshot.sample_count++;
    g_diagnostics_context.working_snapshot.cpu_usage_percent = 0.0f;
    g_diagnostics_context.working_snapshot.cpu_usage_valid   = false;
    // if you have more than the max tasks, uxTaskGetSystemState fails and returns 0
    g_diagnostics_context.working_snapshot.task_capacity_exceeded = (count == 0);
    for (UBaseType_t i = 0; i < count; i++) {
        TaskStatus_t *source_task            = &g_diagnostics_context.raw_tasks[i];
        diagnostics_task_t *destination_task = &g_diagnostics_context.working_snapshot.tasks[i];
        // zero out destination task
        *destination_task = (diagnostics_task_t) {0};

        destination_task->task_id = source_task->xTaskNumber;
        strcpy(destination_task->name, source_task->pcTaskName);
        destination_task->stack_min_free_bytes =
            source_task->usStackHighWaterMark * sizeof(StackType_t);
        destination_task->state = source_task->eCurrentState;

        // Calculate per-task CPU usage
        for (UBaseType_t j = 0; j < g_diagnostics_context.previous_task_count; j++) {
            if (g_diagnostics_context.prev_task_times[j].task_id == source_task->xTaskNumber) {
                destination_task->cpu_usage_valid =
                    calculate_cpu_utilization(source_task->ulRunTimeCounter,
                                              g_diagnostics_context.prev_task_times[j].runtime,
                                              total_delta,
                                              &destination_task->cpu_usage_percent);
                break;
            }
        }

        // Calculate overall CPU usage
        if (source_task->xHandle == idle_task && destination_task->cpu_usage_valid) {
            g_diagnostics_context.working_snapshot.cpu_usage_percent =
                100.0f - destination_task->cpu_usage_percent;
            g_diagnostics_context.working_snapshot.cpu_usage_valid = true;
        }
    }
    // update previous task run times
    for (UBaseType_t i = 0; i < count; i++) {
        g_diagnostics_context.prev_task_times[i].task_id =
            g_diagnostics_context.raw_tasks[i].xTaskNumber;
        g_diagnostics_context.prev_task_times[i].runtime =
            g_diagnostics_context.raw_tasks[i].ulRunTimeCounter;
    }

    g_diagnostics_context.previous_total_runtime = g_diagnostics_context.total_runtime;
    g_diagnostics_context.previous_task_count    = count;
}
