/**
 * @file can.c
 * @brief F4 bxCAN initialization, filters, and interrupt-driven frame transport
 *
 * @author Chris McGalliard
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "common/phal_F4/can/can.h"

extern uint32_t APB1ClockRateHz;

// CAN1/2 each own half of 28 shared banks; CAN3 owns 14 independent banks.
static constexpr uint32_t PHAL_CAN_BANKS_PER_BUS  = 14U;
static constexpr uint32_t PHAL_CAN_IDS_PER_BANK   = 2U;
static constexpr uint32_t PHAL_CAN_MAX_FILTER_IDS = PHAL_CAN_BANKS_PER_BUS * PHAL_CAN_IDS_PER_BANK;
static constexpr uint32_t PHAL_CAN_MAX_STD_ID     = 0x7FFU;
static constexpr uint32_t PHAL_CAN_MAX_EXT_ID     = 0x1FFFFFFFU;

typedef struct {
    uint32_t clock_hz;
    PHAL_FDCAN_BaudRate_t bit_rate;
    uint32_t timing;
} PHAL_CAN_bit_timing_t;

static bool is_valid_bus(const CAN_TypeDef *bus) {
    return bus == CAN1 || bus == CAN2 || bus == CAN3;
}

static bool get_bit_timing(PHAL_FDCAN_BaudRate_t bit_rate, uint32_t *timing) {
    static constexpr PHAL_CAN_bit_timing_t timings[] = {
        {16'000'000, FDCAN_BAUD_250K, PHAL_CAN_16MHZ_250K},
        {24'000'000, FDCAN_BAUD_250K, PHAL_CAN_24MHZ_250K},
        {36'000'000, FDCAN_BAUD_250K, PHAL_CAN_36MHZ_250K},
        {16'000'000, FDCAN_BAUD_500K, PHAL_CAN_16MHZ_500K},
        {24'000'000, FDCAN_BAUD_500K, PHAL_CAN_24MHZ_500K},
        {36'000'000, FDCAN_BAUD_500K, PHAL_CAN_36MHZ_500K},
        {42'000'000, FDCAN_BAUD_500K, PHAL_CAN_42MHZ_500K},
        {16'000'000, FDCAN_BAUD_1M, PHAL_CAN_16MHZ_1M},
        {24'000'000, FDCAN_BAUD_1M, PHAL_CAN_24MHZ_1M},
        {36'000'000, FDCAN_BAUD_1M, PHAL_CAN_36MHZ_1M},
        {42'000'000, FDCAN_BAUD_1M, PHAL_CAN_42MHZ_1M},
    };

    if (timing == nullptr) {
        return false;
    }

    for (uint32_t i = 0; i < sizeof(timings) / sizeof(timings[0]); ++i) {
        if (timings[i].clock_hz == APB1ClockRateHz && timings[i].bit_rate == bit_rate) {
            *timing = timings[i].timing;
            return true;
        }
    }
    return false;
}

static void configure_filters(CAN_TypeDef *bus,
                                      const uint32_t *sid_list,
                                      uint32_t num_sid,
                                      const uint32_t *xid_list,
                                      uint32_t num_xid) {
    // Internal callers validate the bus, list pointers, counts, and IDs first.
    // CAN3 has its own filter block; CAN1/2 share the block accessed via CAN1.
    CAN_TypeDef *filters = (bus == CAN3) ? CAN3 : CAN1;
    // Filter configuration uses FINIT; no controller needs an INRQ transition.
    uint32_t bank_offset = (bus == CAN2) ? PHAL_CAN_BANKS_PER_BUS : 0U;
    uint32_t bank_mask   = ((1U << PHAL_CAN_BANKS_PER_BUS) - 1U) << bank_offset;
    filters->FMR |= CAN_FMR_FINIT;
    if (bus != CAN3) {
        filters->FMR = (filters->FMR & ~CAN_FMR_CAN2SB) | (PHAL_CAN_BANKS_PER_BUS << CAN_FMR_CAN2SB_Pos);
    }
    filters->FA1R &= ~bank_mask;
    filters->FFA1R &= ~bank_mask; // Route this bus's banks to FIFO0.
    filters->FS1R |= bank_mask;   // Two 32-bit filter words per bank.
    filters->FM1R |= bank_mask;   // ID-list mode.

    uint32_t count = num_sid + num_xid;
    if (count == 0) {
        filters->FM1R &= ~(1U << bank_offset); // Mask mode: accept all.
        filters->sFilterRegister[bank_offset].FR1 = 0;
        filters->sFilterRegister[bank_offset].FR2 = 0;
        filters->FA1R |= 1U << bank_offset;
    } else {
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t word = i < num_sid
                ? sid_list[i] << CAN_TI0R_STID_Pos
                : (xid_list[i - num_sid] << CAN_TI0R_EXID_Pos) | CAN_TI0R_IDE;
            uint32_t bank = bank_offset + i / PHAL_CAN_IDS_PER_BANK;
            if ((i % PHAL_CAN_IDS_PER_BANK) == 0) {
                filters->sFilterRegister[bank].FR1 = word;
                // Duplicate the ID until the second word is filled (odd counts stay exact).
                filters->sFilterRegister[bank].FR2 = word;
                filters->FA1R |= 1U << bank;
            } else {
                filters->sFilterRegister[bank].FR2 = word;
            }
        }
    }
    filters->FMR &= ~CAN_FMR_FINIT;
}

bool PHAL_CAN_init(CAN_TypeDef *bus, PHAL_FDCAN_BaudRate_t bit_rate) {
    uint32_t timing = 0;
    if (!is_valid_bus(bus) || !get_bit_timing(bit_rate, &timing)) {
        return false;
    }

    if (bus == CAN3) {
        RCC->APB1ENR |= RCC_APB1ENR_CAN3EN;
        (void)RCC->APB1ENR; // Clock-enable settling (ES0372 section 2.2.7).
    } else {
        // CAN2 depends on CAN1's clock, including for the shared filter registers.
        RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
        (void)RCC->APB1ENR; // Clock-enable settling (ES0372 section 2.2.7).
        if (bus == CAN2) {
            RCC->APB1ENR |= RCC_APB1ENR_CAN2EN;
            (void)RCC->APB1ENR; // Clock-enable settling (ES0372 section 2.2.7).
        }
    }
    bus->MCR &= ~CAN_MCR_SLEEP;
    bus->MCR |= CAN_MCR_INRQ;
    while (!(bus->MSR & CAN_MSR_INAK)) {
        // Wait for the peripheral to acknowledge initialization mode.
    }

    bus->BTR = timing;
    bus->MCR |= CAN_MCR_ABOM;
    configure_filters(bus, nullptr, 0, nullptr, 0);
    bus->IER &= ~CAN_IER_FMPIE1;
    bus->IER |= CAN_IER_FMPIE0 | CAN_IER_TMEIE;

    bus->MCR &= ~CAN_MCR_INRQ;
    while (bus->MSR & CAN_MSR_INAK) {
        // Wait for the peripheral to synchronize with the bus.
    }
    return true;
}

bool PHAL_CAN_setFilters(CAN_TypeDef *bus,
                         const uint32_t *sid_list,
                         uint32_t num_sid,
                         const uint32_t *xid_list,
                         uint32_t num_xid) {
    if (!is_valid_bus(bus) || num_sid > PHAL_CAN_MAX_NUM_SID_FILTER
        || num_xid > PHAL_CAN_MAX_NUM_XID_FILTER || num_sid + num_xid > PHAL_CAN_MAX_FILTER_IDS
        || (num_sid > 0 && sid_list == nullptr) || (num_xid > 0 && xid_list == nullptr)) {
        return false;
    }
    for (uint32_t i = 0; i < num_sid; ++i) {
        if (sid_list[i] > PHAL_CAN_MAX_STD_ID) {
            return false;
        }
    }
    for (uint32_t i = 0; i < num_xid; ++i) {
        if (xid_list[i] > PHAL_CAN_MAX_EXT_ID) {
            return false;
        }
    }

    configure_filters(bus, sid_list, num_sid, xid_list, num_xid);
    return true;
}

bool PHAL_CAN_send(CanMsgTypeDef_t *msg) {
    if (msg == nullptr || !is_valid_bus(msg->Bus) || msg->DLC > sizeof(msg->Data)
        || (msg->IDE ? msg->ExtId > PHAL_CAN_MAX_EXT_ID : msg->StdId > PHAL_CAN_MAX_STD_ID)) {
        return false;
    }
    uint32_t transmit_status = msg->Bus->TSR;
    uint32_t mailbox_index   = 0;
    if (transmit_status & CAN_TSR_TME0) {
        mailbox_index = 0;
    } else if (transmit_status & CAN_TSR_TME1) {
        mailbox_index = 1;
    } else if (transmit_status & CAN_TSR_TME2) {
        mailbox_index = 2;
    } else {
        return false;
    }

    if (!msg->IDE) {
        msg->Bus->sTxMailBox[mailbox_index].TIR = ((uint32_t)msg->StdId << CAN_TI0R_STID_Pos);
    } else {
        msg->Bus->sTxMailBox[mailbox_index].TIR = (msg->ExtId << CAN_TI0R_EXID_Pos) | CAN_TI0R_IDE;
    }
    msg->Bus->sTxMailBox[mailbox_index].TDTR = (msg->DLC << CAN_TDT0R_DLC_Pos);
    msg->Bus->sTxMailBox[mailbox_index].TDLR = ((uint32_t)msg->Data[3] << 24)
        | ((uint32_t)msg->Data[2] << 16) | ((uint32_t)msg->Data[1] << 8) | ((uint32_t)msg->Data[0]);
    msg->Bus->sTxMailBox[mailbox_index].TDHR = ((uint32_t)msg->Data[7] << 24)
        | ((uint32_t)msg->Data[6] << 16) | ((uint32_t)msg->Data[5] << 8) | ((uint32_t)msg->Data[4]);

    msg->Bus->sTxMailBox[mailbox_index].TIR |= CAN_TI0R_TXRQ;

    msg->Bus->IER |= CAN_IER_TMEIE;
    return true;
}

bool PHAL_CAN_txFifoFree(const CAN_TypeDef *bus) {
    return is_valid_bus(bus) && (bus->TSR & (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2));
}

static void rx_irq_handler(CAN_TypeDef *bus) {
    while (bus->RF0R & CAN_RF0R_FMP0) {
        CanMsgTypeDef_t msg = {.Bus = bus};
        uint32_t identifier = bus->sFIFOMailBox[0].RIR;
        msg.IDE             = (identifier & CAN_RI0R_IDE) != 0;
        if (msg.IDE) {
            msg.ExtId = identifier >> CAN_RI0R_EXID_Pos;
        } else {
            msg.StdId = identifier >> CAN_RI0R_STID_Pos;
        }
        msg.DLC = (bus->sFIFOMailBox[0].RDTR & CAN_RDT0R_DLC) >> CAN_RDT0R_DLC_Pos;
        // Classic CAN DLC values above eight still describe an eight-byte payload.
        if (msg.DLC > sizeof(msg.Data)) {
            msg.DLC = sizeof(msg.Data);
        }
        uint32_t low  = bus->sFIFOMailBox[0].RDLR;
        uint32_t high = bus->sFIFOMailBox[0].RDHR;
        for (uint32_t i = 0; i < 4; ++i) {
            msg.Data[i]     = low >> (8 * i);
            msg.Data[i + 4] = high >> (8 * i);
        }
        bus->RF0R = CAN_RF0R_RFOM0;
        PHAL_CAN_rxCallback(&msg);
    }
}

static void tx_irq_handler(CAN_TypeDef *bus) {
    bus->IER &= ~CAN_IER_TMEIE;
    uint32_t transmit_status = bus->TSR;
    bus->TSR                 = transmit_status & (CAN_TSR_RQCP0 | CAN_TSR_RQCP1 | CAN_TSR_RQCP2);
    const uint32_t empty_mailboxes = CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2;
    if ((transmit_status & empty_mailboxes) != empty_mailboxes) {
        bus->IER |= CAN_IER_TMEIE;
    }
    PHAL_CAN_txCallback(bus);
}

void CAN1_RX0_IRQHandler(void) {
    rx_irq_handler(CAN1);
}

void CAN1_TX_IRQHandler(void) {
    tx_irq_handler(CAN1);
}

void CAN2_RX0_IRQHandler(void) {
    rx_irq_handler(CAN2);
}

void CAN2_TX_IRQHandler(void) {
    tx_irq_handler(CAN2);
}

void CAN3_RX0_IRQHandler(void) {
    rx_irq_handler(CAN3);
}

void CAN3_TX_IRQHandler(void) {
    tx_irq_handler(CAN3);
}

[[gnu::weak]]
void PHAL_CAN_txCallback(CAN_TypeDef *can) {
    (void)can;
}

[[gnu::weak]]
void PHAL_CAN_rxCallback(CanMsgTypeDef_t *msg) {
    (void)msg;
}
