#ifndef CAN_COMMON_H
#define CAN_COMMON_H

/**
 * @file can_common.h
 * @brief Common functions and data structures used in every node in the CAN library
 * 
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>

#include "common/rtos/rtos.h"
#if defined(STM32F407xx)
#include "common/phal_F4/can/can.h"
#elif defined(STM32G474xx)
#include "common/phal_G4/fdcan/fdcan.h"
#else
#error "Unsupported CAN target"
#endif

typedef struct {
    uint32_t rx_overflow; // software queue overflow
    uint32_t tx_overflow; // software queue overflow
    uint32_t tx_enqueue_count;
    uint32_t tx_task_wake_count;
    uint32_t tx_sent_count;
    uint32_t tx_callback_count;
    // todo: track hardware stats
    // todo: per-peripheral stats
} can_stats_t;

extern volatile can_stats_t can_stats;
extern volatile uint32_t last_can_rx_time_ms;
// todo: last tx time

extern QueueHandle_t can_rx_queue;

void CAN_tx_update(void);
void CAN_rx_update(void);
bool CAN_init(void);
void CAN_enable_IRQs(void);
void CAN_rx_init(void);
void CAN_tx_init(void);

#define DEFINE_CAN_TASKS() \
    RTOS_DEFINE_TASK(CAN_rx_update, 0, TASK_PRIORITY_HIGH, STACK_2048); \
    RTOS_DEFINE_TASK(CAN_tx_update, 0, TASK_PRIORITY_HIGH, STACK_1024);

#define START_CAN_TASKS() \
    RTOS_START_TASK(CAN_rx_update); \
    RTOS_START_TASK(CAN_tx_update); \
    CAN_enable_IRQs();

#define CAN_TX_QUEUE_LENGTH (32) // Length of software queue for each CAN peripheral
#define CAN_RX_QUEUE_LENGTH (32) // Length of software queue for received messages

#define NVIC_RX_IRQ_PRIO (6)
#define NVIC_TX_IRQ_PRIO (7)
static_assert(
    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY < NVIC_RX_IRQ_PRIO,
    "Do not set an IRQ priority higher (numerically lower) than FreeRTOS to prevent corruption of kernel data structures"
);
static_assert(
    NVIC_RX_IRQ_PRIO < NVIC_TX_IRQ_PRIO,
    "RX priority should be higher (numerically lower) than TX"
);

#endif // CAN_COMMON_H
