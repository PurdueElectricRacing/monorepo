/**
 * @file can_init.c
 * @brief CAN library initialization, IRQ setup, and filter setup.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "can_library/can_common.h"
#include "can_library/generated/can_router.h"

static void CAN_setup_IRQs(void) {
#ifdef USE_FDCAN1
    NVIC_SetPriority(FDCAN1_IT0_IRQn, NVIC_RX_IRQ_PRIO);
    NVIC_SetPriority(FDCAN1_IT1_IRQn, NVIC_TX_IRQ_PRIO);
#endif

#ifdef USE_FDCAN2
    NVIC_SetPriority(FDCAN2_IT0_IRQn, NVIC_RX_IRQ_PRIO);
    NVIC_SetPriority(FDCAN2_IT1_IRQn, NVIC_TX_IRQ_PRIO);
#endif

#ifdef USE_FDCAN3
    NVIC_SetPriority(FDCAN3_IT0_IRQn, NVIC_RX_IRQ_PRIO);
    NVIC_SetPriority(FDCAN3_IT1_IRQn, NVIC_TX_IRQ_PRIO);
#endif
}

void CAN_enable_IRQs(void) {
#ifdef USE_FDCAN1
    NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    NVIC_EnableIRQ(FDCAN1_IT1_IRQn);
#endif

#ifdef USE_FDCAN2
    NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
    NVIC_EnableIRQ(FDCAN2_IT1_IRQn);
#endif

#ifdef USE_FDCAN3
    NVIC_EnableIRQ(FDCAN3_IT0_IRQn);
    NVIC_EnableIRQ(FDCAN3_IT1_IRQn);
#endif
}

static void CAN_setup_filters(void) {
#ifdef USE_FDCAN1
    FDCAN1_set_filters();
#endif

#ifdef USE_FDCAN2
    FDCAN2_set_filters();
#endif

#ifdef USE_FDCAN3
    FDCAN3_set_filters();
#endif
}

void CAN_init(void) {
    CAN_rx_init();
    CAN_tx_init();

    can_stats = (can_stats_t){0};
    last_can_rx_time_ms = 0;

    CAN_data_init(); // from node_header.h

    CAN_setup_IRQs();

    CAN_init_peripherals();
    CAN_setup_filters();
}
