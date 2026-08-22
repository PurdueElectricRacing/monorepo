/**
 * @file can_init.c
 * @brief CAN library initialization, IRQ setup, and filter setup.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "can_library/can_common.h"
#include "can_library/generated/can_router.h"

// Guard against bad arch defines
#if !defined(STM32F407xx) && !defined(STM32G474xx)
#error "Unsupported architecture"
#elif defined(STM32F407xx) && defined(STM32G474xx)
#error "Multiple architectures defined"
#endif

static void CAN_setup_IRQs(void) {
#ifdef USE_CAN1
    NVIC_SetPriority(CAN1_RX0_IRQn, NVIC_RX_IRQ_PRIO);
    NVIC_SetPriority(CAN1_RX1_IRQn, NVIC_RX_IRQ_PRIO);
    NVIC_SetPriority(CAN1_TX_IRQn,  NVIC_TX_IRQ_PRIO);
#endif

#ifdef USE_CAN2
    NVIC_SetPriority(CAN2_RX0_IRQn, NVIC_RX_IRQ_PRIO);
    NVIC_SetPriority(CAN2_RX1_IRQn, NVIC_RX_IRQ_PRIO);
    NVIC_SetPriority(CAN2_TX_IRQn,  NVIC_TX_IRQ_PRIO);
#endif

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
#ifdef USE_CAN1
    NVIC_EnableIRQ(CAN1_RX0_IRQn);
    NVIC_EnableIRQ(CAN1_RX1_IRQn);
    NVIC_EnableIRQ(CAN1_TX_IRQn);
#endif

#ifdef USE_CAN2
    NVIC_EnableIRQ(CAN2_RX0_IRQn);
    NVIC_EnableIRQ(CAN2_RX1_IRQn);
    NVIC_EnableIRQ(CAN2_TX_IRQn);
#endif

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

#if defined(STM32F407xx)

static bool CAN_prepare_filter_config(CAN_TypeDef *can) {
    can->MCR |= CAN_MCR_INRQ;

    uint32_t timeout = 0;
    while (!(can->MSR & CAN_MSR_INAK) && ++timeout < PHAL_CAN_INIT_TIMEOUT)
        ;

    if (timeout == PHAL_CAN_INIT_TIMEOUT) {
        return false;
    }

    can->FMR |= CAN_FMR_FINIT;
    return true;
}

static bool CAN_exit_filter_config(CAN_TypeDef *can) {
    can->FMR &= ~CAN_FMR_FINIT;

    can->MCR &= ~CAN_MCR_INRQ;

    uint32_t timeout = 0;
    while ((can->MSR & CAN_MSR_INAK) && ++timeout < PHAL_CAN_INIT_TIMEOUT)
        ;

    return timeout != PHAL_CAN_INIT_TIMEOUT;
}

static bool CAN_setup_filters(void) {
#if defined(USE_CAN1) || defined(USE_CAN2)
    // bxCAN filters are owned by CAN1, even when using CAN2.
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

    if (!CAN_prepare_filter_config(CAN1)) {
        return false;
    }

    bxcan_set_filters();

    if (!CAN_exit_filter_config(CAN1)) {
        return false;
    }
#endif

    return true;
}

#elif defined(STM32G474xx)

static bool CAN_setup_filters(void) {
#ifdef USE_FDCAN1
    FDCAN1_set_filters();
#endif

#ifdef USE_FDCAN2
    FDCAN2_set_filters();
#endif

#ifdef USE_FDCAN3
    FDCAN3_set_filters();
#endif

    return true;
}

#else
#error "Unsupported CAN architecture"
#endif

bool CAN_init(void) {
    CAN_rx_init();
    CAN_tx_init();

    can_stats = (can_stats_t){0};
    last_can_rx_time_ms = 0;

    CAN_data_init(); // from node_header.h

    CAN_setup_IRQs();

    // todo: remove this check and suppress once F4 CAN support is dropped
    // cppcheck-suppress knownConditionTrueFalse
    if (!CAN_setup_filters()) {
        return false;
    }

    return true;
}
