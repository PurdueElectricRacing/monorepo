#include "diagnostics.h"
#include <string.h>
#include "common/rtos/rtos.h"
#include "can_library/generated/G4_TESTING.h"

static TaskStatus_t raw_tasks[DIAGNOSTICS_MAX_TASKS];
static diagnostics_snapshot_t latest_snapshot;
static diagnostics_snapshot_t working_snapshot;
static uint32_t total_rutime;
static uint32_t previous_total_runtime;
static UBaseType_t previous_task_count;

typedef struct {
    UBaseType_t task_id;
    uint32_t runtime;
} diagnostics_prev_task_time_t;

static diagnostics_prev_task_time_t prev_task_times[DIAGNOSTICS_MAX_TASKS];

// static void diagnostics_send_can(const diagnostics_snapshot_t *snapshot);

static void diagnostics_periodic(void);

RTOS_DEFINE_TASK(diagnostics_periodic, 1000, TASK_PRIORITY_LOW, STACK_1024);

void diagnostics_start(void) {
    TaskHandle_t handle = RTOS_START_TASK(diagnostics_periodic);
    configASSERT(handle != NULL);
}

static void diagnostics_periodic(void) {
    UBaseType_t count = uxTaskGetSystemState(raw_tasks, DIAGNOSTICS_MAX_TASKS, &total_rutime);
    // total time delta inbetween diagnostics_periodic function calls
    uint32_t total_delta = total_rutime - previous_total_runtime;
    uint32_t task_delta;
    TaskHandle_t idle_task                  = xTaskGetIdleTaskHandle();
    working_snapshot.task_count             = (uint32_t)count;
    working_snapshot.timestamp_ms           = (uint32_t)xTaskGetTickCount();
    working_snapshot.sample_count++;
    working_snapshot.cpu_usage_percent = 0.0f;
    working_snapshot.cpu_usage_valid   = false;
    // if you have more than the max tasks, uxTaskGetSystemState fails and returns 0
    working_snapshot.task_capacity_exceeded = (count == 0);
    for (UBaseType_t i = 0; i < count; i++) {
        TaskStatus_t *source_task            = &raw_tasks[i];
        diagnostics_task_t *destination_task = &working_snapshot.tasks[i];
        // zero out destination task
        *destination_task = (diagnostics_task_t) {0};

        destination_task->task_id = source_task->xTaskNumber;
        strcpy(destination_task->name, source_task->pcTaskName);
        destination_task->stack_min_free_bytes =
            source_task->usStackHighWaterMark * sizeof(StackType_t);
        destination_task->state = source_task->eCurrentState;

        // Calculate per-task CPU usage
        for (UBaseType_t j = 0; j < previous_task_count; j++) {
            if (prev_task_times[j].task_id == source_task->xTaskNumber && total_delta > 0
                && (task_delta = source_task->ulRunTimeCounter - prev_task_times[j].runtime)
                    <= total_delta) {
                destination_task->cpu_usage_percent =
                    100.0f * (float)(task_delta) / (float)(total_delta);
                destination_task->cpu_usage_valid = true;
                break;
            }
        }

        // Calculate overall CPU usage
        if (source_task->xHandle == idle_task && destination_task->cpu_usage_valid) {
            working_snapshot.cpu_usage_percent = 100.0f - destination_task->cpu_usage_percent;
            working_snapshot.cpu_usage_valid   = true;
        }
    }
    // update previous task run times
    for (UBaseType_t i = 0; i < count; i++) {
        prev_task_times[i].task_id = raw_tasks[i].xTaskNumber;
        prev_task_times[i].runtime = raw_tasks[i].ulRunTimeCounter;
    }

    previous_total_runtime = total_rutime;
    previous_task_count    = count;
    latest_snapshot        = working_snapshot;
    // diagnostics_send_can(&latest_snapshot);
}

// static void diagnostics_send_can(const diagnostics_snapshot_t *snapshot) {
//     CAN_SEND_g4_testing_diagnostics(
//         snapshot->cpu_usage_percent,
//         (uint8_t)snapshot->task_count,
//         (uint8_t)snapshot->cpu_usage_valid,
//         (uint8_t)snapshot->task_capacity_exceeded
//     );

//     for (uint32_t i = 0; i < snapshot->task_count; i++) {
//         const diagnostics_task_t *task = &snapshot->tasks[i];

//         CAN_SEND_g4_testing_diagnostics_task_cpu(
//             task->task_id,
//             task->cpu_usage_percent
//         );

//         CAN_SEND_g4_testing_diagnostics_task_stack(
//             task->task_id,
//             task->stack_min_free_bytes
//         );

//         CAN_SEND_g4_testing_diagnostics_task_status(
//             task->task_id,
//             (uint8_t)task->state,
//             (uint8_t)task->cpu_usage_valid
//         );
//     }
// }