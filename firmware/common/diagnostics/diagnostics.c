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

static constexpr uint32_t DIAGNOSTICS_SAMPLE_PERIOD_MS = 1000;
static constexpr uint8_t DIAGNOSTICS_TICKS_PER_SAMPLE =
    (uint8_t)(DIAGNOSTICS_SAMPLE_PERIOD_MS / DIAGNOSTICS_TICK_PERIOD_MS);

static_assert(
    DIAGNOSTICS_SAMPLE_PERIOD_MS % DIAGNOSTICS_TICK_PERIOD_MS == 0,
    "sample period must be a multiple of tick period"
);
// One tick per task plus one for the summary must fit in a sample cycle.
static_assert(
    DIAGNOSTICS_MAX_TASKS < DIAGNOSTICS_TICKS_PER_SAMPLE,
    "summary tick must fall within the sample cycle"
);

#ifdef DIAGNOSTICS_CAN_ENABLED
static constexpr uint8_t DIAGNOSTICS_CPU_INVALID = UINT8_MAX;
#endif


static diagnostics_context_t g_diagnostics_context = {0};

// Calculates utilization as the percentage of the total_delta that is equal to current_runtime - previous_runtime
// Returns true if utilization can be calcualted. 
static bool calculate_utilization(
    uint32_t current_runtime,
    uint32_t previous_runtime,
    uint32_t total_delta,
    float *cpu_usage
) {
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
                task->cpu_usage_valid = calculate_utilization(
                    raw_task->ulRunTimeCounter,
                    g_diagnostics_context.prev_task_times[j].runtime,
                    total_delta,
                    &task->cpu_usage_percent
                );
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

#ifdef DIAGNOSTICS_CAN_ENABLED
static diagnostics_task_state_t encode_task_state(eTaskState state) {
    switch (state) {
        case eRunning:
            return DIAGNOSTICS_TASK_STATE_RUNNING;
        case eReady:
            return DIAGNOSTICS_TASK_STATE_READY;
        case eBlocked:
            return DIAGNOSTICS_TASK_STATE_BLOCKED;
        case eSuspended:
            return DIAGNOSTICS_TASK_STATE_SUSPENDED;
        case eDeleted:
            return DIAGNOSTICS_TASK_STATE_DELETED;
        case eInvalid:
        default:
            return DIAGNOSTICS_TASK_STATE_INVALID;
    }
}

static uint8_t encode_cpu_percent(float percent, bool valid) {
    if (!valid || !(percent >= 0.0f && percent <= 100.0f)) {
        return DIAGNOSTICS_CPU_INVALID;
    }
    return (uint8_t)(percent + 0.5f); // round to nearest percent
}

static uint16_t encode_stack_hwm(uint32_t bytes) {
    if (bytes > UINT16_MAX) {
        return UINT16_MAX;
    }
    return (uint16_t)bytes;
}

static void send_task_diagnostics(uint8_t task_index) {
    const diagnostics_snapshot_t *snapshot = &g_diagnostics_context.snapshot;
    const diagnostics_task_t *task         = &snapshot->tasks[task_index];

    DIAGNOSTICS_SEND_TASK(
        task->task_id,
        encode_stack_hwm(task->stack_min_free_bytes),
        encode_cpu_percent(task->cpu_usage_percent, task->cpu_usage_valid),
        encode_task_state(task->state)
    );

    // Cycle through each name, including its terminating NUL.
    uint32_t name_length = (uint32_t)strlen(task->name);
    uint8_t char_index   = (uint8_t)((snapshot->sample_count - 1U) % (name_length + 1U));
    DIAGNOSTICS_SEND_NAME(task->task_id, char_index, (uint8_t)task->name[char_index]);
}

static void send_summary_diagnostics(void) {
    const diagnostics_snapshot_t *snapshot = &g_diagnostics_context.snapshot;
    DIAGNOSTICS_SEND_SUMMARY(
        snapshot->sample_count,
        (uint16_t)snapshot->task_count,
        encode_cpu_percent(snapshot->cpu_percent, snapshot->cpu_valid),
        snapshot->capacity_exceeded
    );
}
#endif // DIAGNOSTICS_CAN_ENABLED

static void sample_diagnostics(void) {
    // returns the number of tasks, reads them into the raw_tasks array, returns 0 if DIAGNOSTICS_MAX_TASKS < number of running tasks
    UBaseType_t task_count = uxTaskGetSystemState(
        g_diagnostics_context.raw_tasks,
        DIAGNOSTICS_MAX_TASKS,
        &g_diagnostics_context.total_runtime
    );
    // total time delta inbetween samples
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

        // Calculate overall CPU usage
        if (raw_task->xHandle == idle_task && task->cpu_usage_valid) {
            g_diagnostics_context.snapshot.cpu_percent = 100.0f - task->cpu_usage_percent;
            g_diagnostics_context.snapshot.cpu_valid = true;
        }
    }
    save_task_runtimes(task_count);
}

void diagnostics_periodic(void) {
    uint8_t tick = g_diagnostics_context.tick_count;
    g_diagnostics_context.tick_count = (uint8_t)((tick + 1U) % DIAGNOSTICS_TICKS_PER_SAMPLE);

    if (tick == 0) {
        sample_diagnostics();
    }
#ifdef DIAGNOSTICS_CAN_ENABLED
    if (tick < g_diagnostics_context.snapshot.task_count) {
        send_task_diagnostics(tick);
    } else if (tick == DIAGNOSTICS_MAX_TASKS) {
        send_summary_diagnostics();
    }
#endif
}
