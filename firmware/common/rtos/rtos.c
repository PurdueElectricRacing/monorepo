/**
 * @file rtos.c
 * @brief Wrapper macros for FreeRTOS constructs (tasks, queues, semaphores) to
 *        simplify static memory allocation and initialization.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 * @author Ronak Jain (jain717@purdue.edu)
 */

#include "rtos.h"

void RTOS_periodic_task_runner(void *arg) {
    RTOS_periodic_task_params_t *wrapper = (RTOS_periodic_task_params_t *)arg;

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period_ticks = pdMS_TO_TICKS(wrapper->period_ms);

    while (true) {
        wrapper->taskFunction();
        if (period_ticks > 0) {
            vTaskDelayUntil(&last_wake_time, period_ticks);
        } else {
            // Yields only to tasks with equal priority
            // If the task is high priority and has no delay, it must block internally
            // (ex: on a queue or notification)
            taskYIELD();
        }
    }

    // Unreachable!
    vTaskDelete(NULL);
}


// Required FreeRTOS hook functions for static allocation of the Idle and Timer tasks.
void vApplicationGetIdleTaskMemory(
    StaticTask_t **ppxIdleTaskTCBBuffer,
    StackType_t **ppxIdleTaskStackBuffer,
    uint32_t *pulIdleTaskStackSize
) {
    static StaticTask_t xIdleTaskTCB;
    static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = xIdleStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}


void vApplicationGetTimerTaskMemory(
    StaticTask_t **ppxTimerTaskTCBBuffer,
    StackType_t **ppxTimerTaskStackBuffer,
    uint32_t *pulTimerTaskStackSize
) {
    static StaticTask_t xTimerTaskTCB;
    static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = xTimerStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}