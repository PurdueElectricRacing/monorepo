/**
 * @file can_tx.c
 * @brief Common CAN TX queue, task, and callback handling.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "can_library/can_common.h"
#include "can_library/generated/can_router.h"

extern TaskHandle_t CAN_tx_update_handle;

volatile can_stats_t can_stats;

#if !defined(STM32F407xx) && !defined(STM32G474xx)
#error "Unsupported architecture"
#elif defined(STM32F407xx) && defined(STM32G474xx)
#error "Multiple architectures defined"
#endif

#if defined(STM32F407xx)
#define CAN_BUS_TYPE CAN_TypeDef
#define PHAL_TX_CALLBACK PHAL_CAN_txCallback
#else // STM32G474xx
#define CAN_BUS_TYPE FDCAN_GlobalTypeDef
#define PHAL_TX_CALLBACK PHAL_FDCAN_txCallback
#endif

// Allocate TX queues
#ifdef USE_CAN1
RTOS_DEFINE_QUEUE(can1_tx_queue, CanMsgTypeDef_t, CAN_TX_QUEUE_LENGTH);
#endif
#ifdef USE_CAN2
RTOS_DEFINE_QUEUE(can2_tx_queue, CanMsgTypeDef_t, CAN_TX_QUEUE_LENGTH);
#endif
#ifdef USE_FDCAN1
RTOS_DEFINE_QUEUE(can1_tx_queue, CanMsgTypeDef_t, CAN_TX_QUEUE_LENGTH);
#endif
#ifdef USE_FDCAN2
RTOS_DEFINE_QUEUE(can2_tx_queue, CanMsgTypeDef_t, CAN_TX_QUEUE_LENGTH);
#endif
#ifdef USE_FDCAN3
RTOS_DEFINE_QUEUE(can3_tx_queue, CanMsgTypeDef_t, CAN_TX_QUEUE_LENGTH);
#endif


void CAN_tx_init(void) {
#ifdef USE_CAN1
    RTOS_INIT_QUEUE(can1_tx_queue);
    CAN1->IER &= ~CAN_IER_TMEIE;
#endif
#ifdef USE_CAN2
    RTOS_INIT_QUEUE(can2_tx_queue);
    CAN2->IER &= ~CAN_IER_TMEIE;
#endif
#ifdef USE_FDCAN1
    RTOS_INIT_QUEUE(can1_tx_queue);
#endif
#ifdef USE_FDCAN2
    RTOS_INIT_QUEUE(can2_tx_queue);
#endif
#ifdef USE_FDCAN3
    RTOS_INIT_QUEUE(can3_tx_queue);
#endif
}

static void CAN_enqueue_tx_to_queue(QueueHandle_t queue, CanMsgTypeDef_t *msg) {
    can_stats.tx_enqueue_count++;

    if (xQueueSendToBack(queue, msg, 0) != pdPASS) {
        can_stats.tx_overflow++;
        return;
    }

    if (CAN_tx_update_handle != NULL) {
        xTaskNotifyGive(CAN_tx_update_handle);
    }
}

#ifdef USE_CAN1
void CAN_enqueue_tx_CAN1(CanMsgTypeDef_t *msg) {
    CAN_enqueue_tx_to_queue(can1_tx_queue, msg);
}
#endif

#ifdef USE_CAN2
void CAN_enqueue_tx_CAN2(CanMsgTypeDef_t *msg) {
    CAN_enqueue_tx_to_queue(can2_tx_queue, msg);
}
#endif

#ifdef USE_FDCAN1
void CAN_enqueue_tx_FDCAN1(CanMsgTypeDef_t *msg) {
    CAN_enqueue_tx_to_queue(can1_tx_queue, msg);
}
#endif

#ifdef USE_FDCAN2
void CAN_enqueue_tx_FDCAN2(CanMsgTypeDef_t *msg) {
    CAN_enqueue_tx_to_queue(can2_tx_queue, msg);
}
#endif

#ifdef USE_FDCAN3
void CAN_enqueue_tx_FDCAN3(CanMsgTypeDef_t *msg) {
    CAN_enqueue_tx_to_queue(can3_tx_queue, msg);
}
#endif

static inline void CAN_wake_tx_from_ISR(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (CAN_tx_update_handle != NULL) {
        vTaskNotifyGiveFromISR(CAN_tx_update_handle, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

#if defined(USE_CAN1) || defined(USE_CAN2)
static void CAN_drain_tx_bus(CAN_TypeDef *bus, QueueHandle_t tx_queue) {
    CanMsgTypeDef_t tx_msg;
    uint8_t free_index;

    while (PHAL_getFreeTxMailbox(bus, &free_index)) {
        if (xQueueReceive(tx_queue, &tx_msg, 0) != pdPASS) {
            bus->IER &= ~CAN_IER_TMEIE;
            return;
        }

        PHAL_txCANMessage(&tx_msg, free_index);
        can_stats.tx_sent_count++;
    }

    if (uxQueueMessagesWaiting(tx_queue) > 0) {
        bus->IER |= CAN_IER_TMEIE;
    } else {
        bus->IER &= ~CAN_IER_TMEIE;
    }
}
#endif

#if defined(USE_FDCAN1) || defined(USE_FDCAN2) || defined(USE_FDCAN3)
static void CAN_drain_tx_bus(const FDCAN_GlobalTypeDef *bus, QueueHandle_t tx_queue) {
    CanMsgTypeDef_t tx_msg;

    while (PHAL_FDCAN_txFifoFree(bus)) {
        if (xQueueReceive(tx_queue, &tx_msg, 0) != pdPASS) {
            return;
        }

        PHAL_FDCAN_send(&tx_msg);
        can_stats.tx_sent_count++;
    }

    // todo arm the TX irq only when uxQueueMessagesWaiting(tx_queue) > 0
}
#endif

void CAN_tx_update(void) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    can_stats.tx_task_wake_count++;

#ifdef USE_CAN1
    CAN_drain_tx_bus(CAN1, can1_tx_queue);
#endif
#ifdef USE_CAN2
    CAN_drain_tx_bus(CAN2, can2_tx_queue);
#endif
#ifdef USE_FDCAN1
    CAN_drain_tx_bus(FDCAN1, can1_tx_queue);
#endif
#ifdef USE_FDCAN2
    CAN_drain_tx_bus(FDCAN2, can2_tx_queue);
#endif
#ifdef USE_FDCAN3
    CAN_drain_tx_bus(FDCAN3, can3_tx_queue);
#endif
}

void PHAL_TX_CALLBACK(CAN_BUS_TYPE *bus) {
    (void)bus;
    CAN_wake_tx_from_ISR();
    can_stats.tx_callback_count++;
}
