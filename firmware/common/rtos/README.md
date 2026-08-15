# FreeRTOS
Eileen Yoon (eyn@purdue.edu)

usage as drop-in replacement for psched:
```c
#include "common/rtos/rtos.h" // also add RTOS to cmake.txt LIBS =

void heartbeat_LED() { PHAL_toggleGPIO(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN); }
RTOS_DEFINE_TASK(heartbeat_LED, 500, TASK_PRIORITY_NORMAL, STACK_256); // define up here so it is global
RTOS_DEFINE_QUEUE(tcp_tx_queue, timestamped_frame_t, TCP_TX_ITEM_COUNT);

int main(void)
{
    // Do all hardware initialization + rtos object creations here
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    NVIC_SetPriority(CAN1_RX0_IRQn, 6); // set priority >= 6, see configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY in FreeRTOSConfig.h

    RTOS_START_TASK(heartbeat_LED); // s/taskCreate/RTOS_START_TASK/
    RTOS_INIT_QUEUE(tcp_tx_queue, timestamped_frame_t, TCP_TX_ITEM_COUNT);

    vTaskStartScheduler(); // s/schedStart/vTaskStartScheduler
}
```

Very good resource: Mastering the FreeRTOS™ Real Time Kernel
https://www.freertos.org/Documentation/02-Kernel/07-Books-and-manual/01-RTOS_book

Key notes:
- under the freertos kernel, a task can either be in running or blocked state. you can only block a task (in the freertos sense) by calling an rtos blocking function, e.g. RTOS_delay_ms, semaphore wait, queue receive, etc.
- blocking in the freertos sense does not mean it blocks in the usual sense (i.e. polls and waits within that function), it will switch to another task.
- specifically freertos will always execute the highest priority task that can run (i.e. not in blocked state)

Other notes:
- if it crashes try increasing stack size
- lock all hardware acceses, e.g. spi transfers. best to raise a semaphore in the ISR when its done
- switch queues (e.g. CAN queues) to FreeRTOS queues so FreeRTOS knows they can block
- if the total given loop time is less than the time it takes to actually execute all the tasks, lower priority tasks (or the tasks started last) will not be executed (task starved)
- use static allocations for all freertos objects (queues, sema) hence the macros here
- no static local variables
- direct task notifications are useful for state machines. e.g.

e.g. eth_send_udp_periodic() blocks until a direct notification from the state updater eth_update_connection_state()

```c
static void eth_update_connection_state(void)
    switch (dh.eth_state)
        case ETH_IDLE:
            if (eth_init() == ETH_ERROR_NONE)
            {
                dh.eth_state = ETH_LINK_UP;
                xTaskNotify(getTaskHandle(eth_send_udp_periodic), 0, eNoAction);
            }

static void eth_send_udp_periodic(void)
    if (dh.eth_state != ETH_LINK_UP)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
```
- note eth_send_udp_periodic child task has to be started before eth_update_connection_state so it can enter blocking state
