/**
 * @file hal_can_f4.c
 * @author Chris McGalliard - Port of L4 HAL by Adam Busch (busch8@purdue.edu)
 * @brief Basic CAN Peripheral HAL library for setting up CAN peripheral and sending messages
 * @version 0.1
 * @date 2023-09-18
 *
 * @copyright Copyright (c) 2021
 *
 */
#include "common/phal_F4/can/can.h"

extern uint32_t APB1ClockRateHz;

bool PHAL_initCAN(CAN_TypeDef* bus, bool test_mode, uint32_t bit_rate) {
    uint32_t timeout = 0;

    // Enable CAN Clock
    if (bus == CAN1) {
        RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
    }
#ifdef CAN2
    else if (bus == CAN2) {
        RCC->APB1ENR |= RCC_APB1ENR_CAN2EN;
    }
#endif /* CAN2 */
    else {
        return false;
    }

    // Leave SLEEP state
    bus->MCR &= ~CAN_MCR_SLEEP;
    // Enter INIT state
    bus->MCR |= CAN_MCR_INRQ;
    while ((bus->MSR & CAN_MSR_SLAK) && ++timeout < PHAL_CAN_INIT_TIMEOUT)
        ;
    if (timeout == PHAL_CAN_INIT_TIMEOUT)
        return false;
    timeout = 0;

    bus->MCR |= CAN_MCR_INRQ;
    while (!(bus->MSR & CAN_MSR_INAK) && ++timeout < PHAL_CAN_INIT_TIMEOUT)
        ;
    if (timeout == PHAL_CAN_INIT_TIMEOUT)
        return false;
    timeout = 0;

    // Bit timing recovered from http://www.bittiming.can-wiki.info/
    if (bit_rate == 1'000'000) {
        switch (APB1ClockRateHz) {
            case 16'000'000:
                bus->BTR = PHAL_CAN_16MHz_1M;  
                break;
            case 24'000'000:
                bus->BTR = PHAL_CAN_24MHz_1M;
                break;
            case 36'000'000:
                bus->BTR = PHAL_CAN_36MHz_1M;
                break;
            case 42'000'000:
                bus->BTR = PHAL_CAN_42MHz_1M;
                break;
            default:
                return false;
        }
    } else if (bit_rate == 500'000) {
        switch (APB1ClockRateHz) {
            case 16'000'000:
                bus->BTR = PHAL_CAN_16MHz_500k;
                break;
            case 24'000'000:
                bus->BTR = PHAL_CAN_24MHz_500k;
                break;
            case 36'000'000:
                bus->BTR = PHAL_CAN_36MHz_500k;
                break;
            case 42'000'000:
                bus->BTR = PHAL_CAN_42MHz_500k;
                break;
            default:
                return false;
        }
    } else if (bit_rate == 250'000) {
        switch (APB1ClockRateHz) {
            case 16'000'000:
                bus->BTR = PHAL_CAN_16MHz_250k;
                break;
            case 24'000'000:
                bus->BTR = PHAL_CAN_24MHz_250k;
                break;
            case 36'000'000:
                bus->BTR = PHAL_CAN_36MHz_250k;
                break;
            default:
                return false;
        }

    } 
    else {
        return false;
    }

    // Keep the bus active
    bus->MCR |= CAN_MCR_ABOM;

    // Loopback mode in testing mode
    if (test_mode) {
        bus->BTR |= CAN_BTR_LBKM;
    }

    // Setup filters for all IDs
    if (bus == CAN1) {
        bus->FMR |= CAN_FMR_FINIT; // Enter init mode for filter banks
        bus->FM1R &= ~(CAN_FM1R_FBM0_Msk); // Set bank 0 to mask mode
        bus->FS1R &= ~(1U << CAN_FS1R_FSC0_Pos); // Set bank 0 to 16bit mode
        bus->FA1R |= (1U << CAN_FA1R_FACT0_Pos); // Activate bank 0
        bus->sFilterRegister[0].FR1 = 0; // Set mask to 0
        bus->sFilterRegister[0].FR2 = 0;
#ifdef CAN2
        bus->FMR &= ~(CAN_FMR_CAN2SB);
        bus->FMR |= (27 << CAN_FMR_CAN2SB_Pos);
        bus->FM1R &= ~(CAN_FM1R_FBM27_Msk); // Set bank 27 to mask mode
        bus->FS1R &= ~(1U << CAN_FS1R_FSC27_Pos); // Set bank 27 to 16bit mode
        bus->FA1R |= (1U << CAN_FA1R_FACT27_Pos); // Activate bank 0
        bus->sFilterRegister[27].FR1 = 0; // Set mask to 0
        bus->sFilterRegister[27].FR2 = 0;
#endif
        bus->FMR &= ~CAN_FMR_FINIT; // Enable Filters
    }

    // Enable FIFO0/1 RX message pending interrupt
    bus->IER |= CAN_IER_FMPIE0;
    bus->IER |= CAN_IER_FMPIE1;

    // Enable TX mailbox empty interrupt
    bus->IER |= CAN_IER_TMEIE;

    // Enter NORMAL mode
    bus->MCR &= ~CAN_MCR_INRQ;
    while ((bus->MSR & CAN_MSR_INAK) && ++timeout < PHAL_CAN_INIT_TIMEOUT)
        ;

    return timeout != PHAL_CAN_INIT_TIMEOUT;
}

bool PHAL_deinitCAN(CAN_TypeDef* bus) {
    if (bus == CAN1) {
        RCC->APB1RSTR |= RCC_APB1RSTR_CAN1RST;
    }
#ifdef CAN2
    else if (bus == CAN2) {
        RCC->APB1RSTR |= RCC_APB1RSTR_CAN2RST;
    }
#endif /* CAN2 */
    else
        return false;
    return true;
}

bool PHAL_txCANMessage(CanMsgTypeDef_t* msg, uint8_t txMbox) {
    if (txMbox > 2)
        return false; // invalid box

    switch (txMbox) {
        case 0:
            if (!(msg->Bus->TSR & CAN_TSR_TME0))
                return false; // mbx full
            break;
        case 1:
            if (!(msg->Bus->TSR & CAN_TSR_TME1))
                return false;
            break;
        case 2:
            if (!(msg->Bus->TSR & CAN_TSR_TME2))
                return false;
            break;
    }

    // if (msg->Bus->TSR & CAN_TSR_TME0)
    // {
    //     txMbox = 0;
    //     txOkay = CAN_TSR_TXOK0;
    // }
    // else if (msg->Bus->TSR & CAN_TSR_TME1)
    // {
    //     txMbox = 1;
    //     txOkay = CAN_TSR_TXOK1;
    // }
    // else if (msg->Bus->TSR & CAN_TSR_TME2)
    // {
    //     txMbox = 2;
    //     txOkay = CAN_TSR_TXOK2;
    // }
    // else
    //     return false;   // Unable to find Mailbox
    if (!msg->IDE) {
        msg->Bus->sTxMailBox[txMbox].TIR = ((uint32_t)msg->StdId << CAN_TI0R_STID_Pos); // Standard ID
    } else {
        msg->Bus->sTxMailBox[txMbox].TIR = (msg->ExtId << CAN_TI0R_EXID_Pos) | 4; // Extended ID
    }
    msg->Bus->sTxMailBox[txMbox].TDTR = ((uint32_t)msg->DLC << CAN_TDT0R_DLC_Pos); // Data Length
    msg->Bus->sTxMailBox[txMbox].TDLR = ((uint32_t)msg->Data[3] << 24) | ((uint32_t)msg->Data[2] << 16) | ((uint32_t)msg->Data[1] << 8) | ((uint32_t)msg->Data[0]);
    msg->Bus->sTxMailBox[txMbox].TDHR = ((uint32_t)msg->Data[7] << 24) | ((uint32_t)msg->Data[6] << 16) | ((uint32_t)msg->Data[5] << 8) | ((uint32_t)msg->Data[4]);

    msg->Bus->sTxMailBox[txMbox].TIR |= (0b1U << CAN_TI0R_TXRQ_Pos); // Request TX

    // while(!(msg->Bus->TSR & txOkay) && ++timeout < PHAL_CAN_TX_TIMEOUT)      // Wait for message to be sent within specified timeout
    //     ;

    // return timeout != PHAL_CAN_TX_TIMEOUT;
    return true;
}

bool PHAL_txMailboxFree(CAN_TypeDef* bus, uint8_t mbx) {
    switch (mbx) {
        case 0:
            return bus->TSR & CAN_TSR_TME0;
        case 1:
            return bus->TSR & CAN_TSR_TME1;
        case 2:
            return bus->TSR & CAN_TSR_TME2;
        default:
            return false;
    }
}

bool PHAL_anyTxMailboxFree(CAN_TypeDef* bus) {
    return bus->TSR & (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2);
}

bool PHAL_getFreeTxMailbox(CAN_TypeDef* bus, uint8_t* mbx) {
    if (bus->TSR & CAN_TSR_TME0) {
        *mbx = 0;
        return true;
    } else if (bus->TSR & CAN_TSR_TME1) {
        *mbx = 1;
        return true;
    } else if (bus->TSR & CAN_TSR_TME2) {
        *mbx = 2;
        return true;
    } else {
        return false; // No free mailbox
    }
}

void PHAL_txCANAbort(CAN_TypeDef* bus, uint8_t mbx) {
    switch (mbx) {
        case 0:
            bus->TSR |= CAN_TSR_ABRQ0;
            break;
        case 1:
            bus->TSR |= CAN_TSR_ABRQ1;
            break;
        case 2:
            bus->TSR |= CAN_TSR_ABRQ2;
            break;
        default:
            break;
    }
}

bool PHAL_rxCANMessage(CAN_TypeDef *bus, uint8_t fifo, CanMsgTypeDef_t *msg) {
    if (fifo > 1) return false;
    if (fifo == 0 && !(bus->RF0R & CAN_RF0R_FMP0)) return false;
    if (fifo == 1 && !(bus->RF1R & CAN_RF1R_FMP1)) return false;

    msg->Bus = bus;
    if (bus->sFIFOMailBox[fifo].RIR & CAN_RI0R_IDE) {
        msg->IDE = true;
        msg->ExtId = bus->sFIFOMailBox[fifo].RIR >> CAN_RI0R_EXID_Pos;
    } else {
        msg->IDE = false;
        msg->StdId = (uint16_t)(bus->sFIFOMailBox[fifo].RIR >> CAN_RI0R_STID_Pos);
    }

    msg->DLC = (uint8_t)((bus->sFIFOMailBox[fifo].RDTR & CAN_RDT0R_DLC) >> CAN_RDT0R_DLC_Pos);

    uint32_t low = bus->sFIFOMailBox[fifo].RDLR;
    uint32_t high = bus->sFIFOMailBox[fifo].RDHR;

    msg->Data[0] = (uint8_t)((low >> 0) & 0xFF);
    msg->Data[1] = (uint8_t)((low >> 8) & 0xFF);
    msg->Data[2] = (uint8_t)((low >> 16) & 0xFF);
    msg->Data[3] = (uint8_t)((low >> 24) & 0xFF);
    msg->Data[4] = (uint8_t)((high >> 0) & 0xFF);
    msg->Data[5] = (uint8_t)((high >> 8) & 0xFF);
    msg->Data[6] = (uint8_t)((high >> 16) & 0xFF);
    msg->Data[7] = (uint8_t)((high >> 24) & 0xFF);

    if (fifo == 0) bus->RF0R |= CAN_RF0R_RFOM0;
    else bus->RF1R |= CAN_RF1R_RFOM1;

    return true;
}

void PHAL_CAN_rxIRQ(CAN_TypeDef *bus, uint8_t fifo) {
    CanMsgTypeDef_t msg;

    while (PHAL_rxCANMessage(bus, fifo, &msg)) {
        PHAL_CAN_rxCallback(&msg);
    }
}

void __attribute__((weak)) CAN1_RX0_IRQHandler(void) {
    PHAL_CAN_rxIRQ(CAN1, 0);
}

void __attribute__((weak)) CAN1_RX1_IRQHandler(void) {
    PHAL_CAN_rxIRQ(CAN1, 1);
}

void __attribute__((weak)) CAN2_RX0_IRQHandler(void) {
    PHAL_CAN_rxIRQ(CAN2, 0);
}

void __attribute__((weak)) CAN2_RX1_IRQHandler(void) {
    PHAL_CAN_rxIRQ(CAN2, 1);
}

void PHAL_CAN_txIRQ(CAN_TypeDef *bus) {
    uint32_t tsr = bus->TSR;

    if (tsr & CAN_TSR_RQCP0) bus->TSR = CAN_TSR_RQCP0;
    if (tsr & CAN_TSR_RQCP1) bus->TSR = CAN_TSR_RQCP1;
    if (tsr & CAN_TSR_RQCP2) bus->TSR = CAN_TSR_RQCP2;

    bus->IER &= ~CAN_IER_TMEIE;

    PHAL_CAN_txCallback(bus);
}

void CAN1_TX_IRQHandler(void) {
    PHAL_CAN_txIRQ(CAN1);
}

void CAN2_TX_IRQHandler(void) {
    PHAL_CAN_txIRQ(CAN2);
}

// ! override these in your application code!
[[gnu::weak]]
void PHAL_CAN_txCallback(CAN_TypeDef *can) {
    (void)can;
}

[[gnu::weak]]
void PHAL_CAN_rxCallback(CanMsgTypeDef_t *msg) {
    (void)msg;
}