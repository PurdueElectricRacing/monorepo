#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_DIAGNOSTICS)

#include <stdint.h>
#include <stdio.h>

#include "common/phal_G4/rcc/rcc.h"
#include "rtos.h"

#define MAX_TASKS 16

void HardFault_Handler();

static void waste_stack(size_t size);
static void dumb_task(void *argument);

volatile UBaseType_t g_stack_hwm = 0;
volatile size_t g_stack_amount   = 0;
volatile size_t g_free_heap      = 0;
volatile size_t g_min_free_heap  = 0;

static TaskStatus_t task_snapshots[MAX_TASKS];
static configRUN_TIME_COUNTER_TYPE total_runtime;

volatile UBaseType_t task_count = 0;

RTOS_DEFINE_TASK(dumb_task, 10, TASK_PRIORITY_HIGH, 1024);

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    RTOS_START_TASK(dumb_task);
    vTaskStartScheduler();

    
    return 0;
}

static void waste_stack(size_t size) {
    volatile uint8_t junk[256];

    for (size_t i = 0; i < size && i < sizeof(junk); i++) {
        junk[i] = (uint8_t)i;
    }
}

void dumb_task(void *argument) {
    size_t amount = 16;

    while (1) {
        // Gradually use more stack
        waste_stack(amount);

        g_stack_hwm    = uxTaskGetStackHighWaterMark(NULL);
        g_stack_amount = amount;

        // g_free_heap = xPortGetFreeHeapSize();
        // g_min_free_heap = xPortGetMinimumEverFreeHeapSize();

        amount += 16;

        if (amount > 256)
            amount = 16;
        task_count = uxTaskGetSystemState(task_snapshots, MAX_TASKS, &total_runtime);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#endif