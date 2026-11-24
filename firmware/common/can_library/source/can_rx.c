/**
 * @file can_rx.c
 * @brief Common CAN RX queue, task, and callback handling.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "can_library/can_common.h"
#include "can_library/generated/can_router.h"

volatile can_data_t can_data;
volatile uint32_t last_can_rx_time_ms;

RTOS_DEFINE_QUEUE(can_rx_queue, CanMsgTypeDef_t, CAN_RX_QUEUE_LENGTH);

void CAN_rx_init(void) {
    RTOS_INIT_QUEUE(can_rx_queue);
}

void CAN_rx_update(void) {
    CanMsgTypeDef_t rx_msg;

    if (xQueueReceive(can_rx_queue, &rx_msg, portMAX_DELAY) == pdPASS) {
        last_can_rx_time_ms = xTaskGetTickCount();

        CAN_rx_dispatcher(
            rx_msg.IDE == 0 ? rx_msg.StdId : rx_msg.ExtId,
            rx_msg.Data,
            rx_msg.DLC,
            rx_msg.Bus
        );
    }
}

void PHAL_FDCAN_rxCallback(CanMsgTypeDef_t *msg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueSendFromISR(can_rx_queue, msg, &xHigherPriorityTaskWoken) != pdPASS) {
        can_stats.rx_overflow++;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
