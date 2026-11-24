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

// Allocate TX queues
#ifdef USE_FDCAN1
RTOS_DEFINE_QUEUE(fdcan1_tx_queue, CanMsgTypeDef_t, CAN_TX_QUEUE_LENGTH);
#endif
#ifdef USE_FDCAN2
RTOS_DEFINE_QUEUE(fdcan2_tx_queue, CanMsgTypeDef_t, CAN_TX_QUEUE_LENGTH);
#endif
#ifdef USE_FDCAN3
RTOS_DEFINE_QUEUE(fdcan3_tx_queue, CanMsgTypeDef_t, CAN_TX_QUEUE_LENGTH);
#endif

void CAN_tx_init(void) {
#ifdef USE_FDCAN1
    RTOS_INIT_QUEUE(fdcan1_tx_queue);
#endif
#ifdef USE_FDCAN2
    RTOS_INIT_QUEUE(fdcan2_tx_queue);
#endif
#ifdef USE_FDCAN3
    RTOS_INIT_QUEUE(fdcan3_tx_queue);
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

#ifdef USE_FDCAN1
void CAN_enqueue_tx_FDCAN1(CanMsgTypeDef_t *msg) {
    CAN_enqueue_tx_to_queue(fdcan1_tx_queue, msg);
}
#endif

#ifdef USE_FDCAN2
void CAN_enqueue_tx_FDCAN2(CanMsgTypeDef_t *msg) {
    CAN_enqueue_tx_to_queue(fdcan2_tx_queue, msg);
}
#endif

#ifdef USE_FDCAN3
void CAN_enqueue_tx_FDCAN3(CanMsgTypeDef_t *msg) {
    CAN_enqueue_tx_to_queue(fdcan3_tx_queue, msg);
}
#endif

static inline void CAN_wake_tx_from_ISR(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (CAN_tx_update_handle != NULL) {
        vTaskNotifyGiveFromISR(CAN_tx_update_handle, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void CAN_drain_tx_bus(const FDCAN_GlobalTypeDef *bus, QueueHandle_t tx_queue) {
    CanMsgTypeDef_t tx_msg;

    while (PHAL_FDCAN_txFifoFree(bus)) {
        if (xQueueReceive(tx_queue, &tx_msg, 0) != pdPASS) {
            return;
        }

        PHAL_FDCAN_send(&tx_msg);
        can_stats.tx_sent_count++;
    }
}

void CAN_tx_update(void) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    can_stats.tx_task_wake_count++;

#ifdef USE_FDCAN1
    CAN_drain_tx_bus(FDCAN1, fdcan1_tx_queue);
#endif
#ifdef USE_FDCAN2
    CAN_drain_tx_bus(FDCAN2, fdcan2_tx_queue);
#endif
#ifdef USE_FDCAN3
    CAN_drain_tx_bus(FDCAN3, fdcan3_tx_queue);
#endif
}

void PHAL_FDCAN_txCallback(FDCAN_GlobalTypeDef *bus) {
    (void)bus;
    CAN_wake_tx_from_ISR();
    can_stats.tx_callback_count++;
}
