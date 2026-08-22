/**
 * @file fdcan_priv.c
 * @brief G4 FDCAN private/register level implementation
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/fdcan/fdcan_priv.h"
 
uint32_t PHAL_FDCAN_priv_ramBase(const FDCAN_GlobalTypeDef *fdcan) {
    // SRAMCAN_BASE = start address of the single block of SRAM
    // It is shared by all FDCAN instances for their Message RAM (filters/FIFOs/TX buffers)
    if (fdcan == FDCAN1) {
        return SRAMCAN_BASE;
    } else if (fdcan == FDCAN2) {
        return SRAMCAN_BASE + FDCAN_PRIV_SRAMCAN_SIZE;
    } else { // fdcan == FDCAN3
        return SRAMCAN_BASE + 2U * FDCAN_PRIV_SRAMCAN_SIZE;
    }
}
 
void PHAL_FDCAN_priv_enableClock(void) {
    // RCC = Reset and Clock Control peripheral.
    // CCIPR = Peripheral Independent Clock Configuration Register
    // - select which clock source feeds peripherals
    // FDCANSEL = FDCAN clock source Select field within CCIPR
    // FDCANSEL_Msk = bitmask covering the full FDCANSEL field
    // - (so it can be cleared before writing a new value into it)
    RCC->CCIPR &= ~RCC_CCIPR_FDCANSEL_Msk;

    // Set FDCAN clock source to PCLK1
    RCC->CCIPR |= FDCAN_PRIV_RCC_FDCANCLKSOURCE_PCLK1;

    // APB1ENR1 = Advanced Peripheral Bus 1 (APB1) (peripheral clock) Enable
    // Register 1 - each bit gates the clock to one APB1 peripheral
    // FDCANEN = FDCAN clock Enable bit within APB1ENR1
    // - until this is set the peripheral's registers aren't clocked/accessible
    RCC->APB1ENR1 |= RCC_APB1ENR1_FDCANEN;
}
 
void PHAL_FDCAN_priv_enterConfig(FDCAN_GlobalTypeDef *fdcan) {
    // CCCR = Communication Controller (CC) Control Register
    // - main mode/state register for the FDCAN core
    // CSR = Clock Stop Request bit
    // - clear to ensure we are not requesting clock stop before we configure it
    fdcan->CCCR &= ~FDCAN_CCCR_CSR;

    // CSA = Clock Stop Acknowledge bit
    // - hardware sets this once it has entered clock-stop state in response to CSR
    // - wait until the peripheral confirms it has left clock-stop state (CSA low)
    // - then it's ready to accept further configuration
    while (fdcan->CCCR & FDCAN_CCCR_CSA) {
        __asm__("nop");
    }

    // INIT = Initialization bit
    // - must be set to 1 before configuring CCCR
    // - now RAM configuration is allowed to change
    fdcan->CCCR |= FDCAN_CCCR_INIT;

    // Wait until hardware reflects INIT=1 back
    while (!(fdcan->CCCR & FDCAN_CCCR_INIT)) {
        __asm__("nop");
    }

    // CCE = Configuration Change Enable bit
    // - CCE unlocks the protected configuration registers
    //   (NBTP, TXBC, RXGFC) for writing
    fdcan->CCCR |= FDCAN_CCCR_CCE;
}
 
void PHAL_FDCAN_priv_exitConfig(FDCAN_GlobalTypeDef *fdcan) {
    // Clearing INIT (Initialization bit) tells the core to leave
    // initialization/configuration mode and resume normal bus operation
    fdcan->CCCR &= ~FDCAN_CCCR_INIT;

    // Busy-wait until hardware confirms INIT is 0
    while (fdcan->CCCR & FDCAN_CCCR_INIT) {
        __asm__("nop");
    }
}

void PHAL_FDCAN_priv_controlConfig(FDCAN_GlobalTypeDef *fdcan) {
    // FDOE = FD (Flexible Data-rate) Operation Enable bit
    // - cleared: forces classic (non-FD) CAN only
    // BRSE = Bit Rate Switch Enable bit
    // - cleared: only matters for FD frames, cleared b/c FD is disabled
    // MON = Bus Monitoring mode bit
    // - cleared: otherwise puts the core in a listen-only mode (no ACK/TX/etc)
    // ASM = Restricted Operation Mode bit
    // - cleared: otherwise puts core in  a listen-and-limited-ACK mode
    // TEST = Test Mode Enable bit
    // - cleared: not configuring TEST register and not using loopback mode
    fdcan->CCCR &= ~(FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE | FDCAN_CCCR_MON | FDCAN_CCCR_ASM | FDCAN_CCCR_TEST);

    // PXHD = Protocol Exception Handling Disable bit
    // - setting this to 1 disables "protocol exception" bus-off-avoidance handling
    // - now: unusual bit sequences are treated as ordinary form errors instead
    fdcan->CCCR |= FDCAN_CCCR_PXHD;

    // DAR = Disable Automatic Retransmission bit
    // - clearing this bit means automatic retransmission is enabled
    // - now: a failed/aborted frame will be retried automatically by hardware
    fdcan->CCCR &= ~FDCAN_CCCR_DAR;

    // TXP = Transmit Pause bit
    // - set: the core adds very short delay after each tx before starting the next
    //   to give other nodes a fairer chance at the bus
    fdcan->CCCR |= FDCAN_CCCR_TXP;

    // TEST = the FDCAN Test register
    // LBCK = Loop Back mode bit within TEST
    // - not set: does not internally routes TX back to RX inside the peripheral
    // - now: TX is sent to the bus and RX only sees what is actually on the bus
    fdcan->TEST &= ~FDCAN_TEST_LBCK; // disable loopback
}

void PHAL_FDCAN_priv_setTransmitFifoQueueModeToFifo(FDCAN_GlobalTypeDef *fdcan) {
    // TXBC = TX Buffer Configuration register
    // - controls how the TX buffer area is used
    // TFQM = Tx FIFO/Queue Mode bit
    // - clear: selects FIFO ordering (tx in the order they were queued)
    fdcan->TXBC &= ~FDCAN_TXBC_TFQM;
}

void PHAL_FDCAN_setInterruptLines(FDCAN_GlobalTypeDef *fdcan) {
    // ILS = Interrupt Line Select register
    // - chooses whether that group signals out on interrupt line 0 or 1
    // SMSG = Status Message interrupt group bit within ILS
    // - covers status-type interrupt sources (including Transmission Completed)
    // - setting this bit routes that whole group to line 1
    // Other bits left at 0, (ex: RX FIFO 0 group), stay on line 0
    fdcan->ILS = FDCAN_ILS_SMSG;
 
    // ILE = Interrupt Line Enable register
    // - master enable per physical interrupt line
    // EINT0 = Enable Interrupt line 0 bit
    // EINT1 = Enable Interrupt line 1 bit
    // Now: RX group (line 0) and the status group (line 1) can reach the NVIC
    fdcan->ILE = FDCAN_ILE_EINT0 | FDCAN_ILE_EINT1;
 
    // IR = Interrupt Register
    // - holds latched (sticky) interrupt flags
    // - writing a 1 to a bit clears that flag (write-1-to-clear)
    // RF0N = Receive FIFO 0 New message flag
    // TC = Transmission Completed flag
    // Clear both to discard any stale flags
    fdcan->IR = FDCAN_IR_RF0N | FDCAN_IR_TC;
 
    // IE = Interrupt Enable register
    // - per-flag enable controlling whether a set flag in IR asserts its interrupt line
    // RF0NE = Receive FIFO 0 New message Interrupt Enable bit
    // TCE = Transmission Completed Interrupt Enable bit
    fdcan->IE |= FDCAN_IE_RF0NE | FDCAN_IE_TCE;
 
    // TXBTIE = Tx Buffer Transmission Interrupt Enable register
    // - one bit per TX buffer/FIFO element
    // - setting all bits means every element (once tx-ed), sets the Transmission Completed
    //   regardless of which slot it was queued in
    fdcan->TXBTIE = 0xFFFFFFFFU;
}

void PHAL_FDCAN_priv_writeFilterAction(FDCAN_GlobalTypeDef *fdcan, PHAL_FDCAN_DefaultFilterAction_t action) {
    // RXGFC = Rx Global Filter Configuration register
    // - controls what happens to frames that no specific filter element matched
    // ANFS = Accept Non-matching Frames Standard field
    // - what to do with an unmatched STANDARD-ID frame
    // ANFE = Accept Non-matching Frames Extended field
    // - what to do with an unmatched EXTENDED-ID frames
    // RRFS = Reject Remote Frames Standard bit
    // - whether to reject Remote Transmission Request frames with a standard ID outright
    // RRFE = Reject Remote Frames Extended bit
    // - whether to reject Remote Transmission Request frames with an extended ID outright
    // All 4 of these fields are written identically
    fdcan->RXGFC = (action << FDCAN_RXGFC_ANFS_Pos)
        | (action << FDCAN_RXGFC_ANFE_Pos)
        | (action << FDCAN_RXGFC_RRFS_Pos)
        | (action << FDCAN_RXGFC_RRFE_Pos);
}
 
uint32_t PHAL_FDCAN_priv_getNBTP(PHAL_FDCAN_BaudRate_t bit_rate) {
    switch (bit_rate) {
        case FDCAN_BAUD_250K:
        case FDCAN_BAUD_500K:
        case FDCAN_BAUD_1M:
            // seg1/seg2/sjw are in real time-quanta (TQ) units
            // TSEG1 = Time Segment 1
            // - propagation + phase segment 1, before the sample point
            // TSEG2 = Time Segment 2
            // - phase segment 2, after the sample point
            // SJW  = Synchronization Jump Width
            // max TQ the core may lengthen/shorten a segment by to resynchronize with the bus
            // 1 + 13 (TSEG1) + 2 (TSEG2) = 16 TQ per bit with a 
            // sample point at (1+13)/16 = 87.5% into the bit
            const uint32_t seg1 = 13;
            const uint32_t seg2 = 2;
            const uint32_t sjw = 2;
            const uint32_t tq_per_bit = 1 + seg1 + seg2; 

            // BRP (Baud Rate Prescaler) = kernel_clock_Hz / (bit_rate * TQ_per_bit)
            uint32_t brp = FDCAN_PRIV_KER_CLK_HZ / ((uint32_t)bit_rate * tq_per_bit);
            
            // NBTP = Nominal Bit Timing and Prescaler register
            // - configures the bit timing used for the arbitration/nominal phase of every frame
            // - Class CAN only has this one phase
            // NBRP = Nominal Baud Rate Prescaler field
            // - divides the FDCAN kernel clock down to produce 1 time quantum (TQ) per tick
            // NTSEG1 = Nominal Time Segment 1 field
            // NTSEG2 = Nominal Time Segment 2 field
            // NSJW = Nominal (re)Synchronization Jump Width field
            // Fields require a bias-correction (-1) to match hardware's internal encoding
            return ((brp - 1U) << FDCAN_NBTP_NBRP_Pos)
                | ((seg1 - 1U) << FDCAN_NBTP_NTSEG1_Pos)
                | ((seg2 - 1U) << FDCAN_NBTP_NTSEG2_Pos)
                | ((sjw - 1U) << FDCAN_NBTP_NSJW_Pos);
        default:
            __builtin_trap();
    }
}
 
void PHAL_FDCAN_priv_writeStandardFilters(FDCAN_GlobalTypeDef *fdcan, const uint32_t *sid_list, uint32_t num_sid) {
    volatile uint32_t *ram = (volatile uint32_t *)(PHAL_FDCAN_priv_ramBase(fdcan)
                                 + FDCAN_PRIV_SRAMCAN_FLSSA);
 
    // Set up each filter element
    for (uint32_t i = 0; i < num_sid; i++) {
        uint32_t sid = sid_list[i] & 0x7FFU; // 11-bit standard ID
        // SFT=10 (classic mask), SFEC=001 (store to FIFO0), SFID1=sid, SFID2=mask (all 1s -> exact match)
        // Packed:
        // SFT = Standard Filter Type, bits[31:30]
        // - 2: selects classic filter (match SFID1 exactly under mask SFID2)
        // SFEC = Standard Filter Element Configuration, bits[29:27]
        // - 1: on match, store frame into Rx FIFO 0
        // SFID1: Standard Filter ID 1, bits[26:16]
        // - the ID to match.
        // SFID2: Standard Filter ID 2, bits[10:0]
        // - comparison mask in classic filter mode
        // - all-1s: every bit of SFID1 must match exactly
        ram[i] = (2U << 30) | (1U << 27) | (sid << 16) | 0x7FFU;
    }
 
    // Set the number of standard filters
    // RXGFC = Rx Global Filter Configuration register
    // LSS = List Size Standard field
    // - tells the core how many standard filter elements (starting at FLSSA) are valid/set
    fdcan->RXGFC &= ~FDCAN_RXGFC_LSS_Msk;
    fdcan->RXGFC |= (num_sid << FDCAN_RXGFC_LSS_Pos);
}
 
void PHAL_FDCAN_priv_writeExtendedFilters(FDCAN_GlobalTypeDef *fdcan, const uint32_t *xid_list, uint32_t num_xid) {
    volatile uint32_t *ram = (volatile uint32_t *)(PHAL_FDCAN_priv_ramBase(fdcan)
                                 + FDCAN_PRIV_SRAMCAN_FLESA);
 
    // Set up each filter element
    for (uint32_t i = 0; i < num_xid; i++) {
        uint32_t xid = xid_list[i] & 0x1FFFFFFFU; // 29-bit extended ID

        // Word 0:
        // - EFEC = Extended Filter Element Configuration, bits[31:29]
        //   - 1: on match, store frame into Rx FIFO 0
        // - EFID1 = Extended Filter ID1, bits[28:0]
        //   - the ID to match
        ram[i * 2 + 0] = (1U << 29) | xid;

        // Word 1
        // - EFT = Extended Filter Type, bits[31:30]
        //   - 2: selects classic filter: match EFID1 exactly under a mask
        // - [29:0]: the mask
        //   - all-1s means every bit of EFID1 must match exactly
        ram[i * 2 + 1] = (2U << 30) | 0x1FFFFFFFU;
    }
 
    // Set the number of extended filters
    // LSE = List Size Extended field within RXGFC
    // - how many extended filter elements (starting at FLESA) are valid/set
    fdcan->RXGFC &= ~FDCAN_RXGFC_LSE_Msk;
    fdcan->RXGFC |= (num_xid << FDCAN_RXGFC_LSE_Pos);

    // XIDAM = Extended ID And Mask register
    // - a single, global mask ANDed against every incoming extended ID before
    //   filter matching
    // - all-1s: means do not prematurely filter, only the per-filter-element masks are used
    fdcan->XIDAM = 0x1FFFFFFFU;
}
 
void PHAL_FDCAN_priv_writeTxElement(FDCAN_GlobalTypeDef *fdcan, CanMsgTypeDef_t *msg) {
    // Get the index of the next free TX FIFO slot
    // TXFQS = Tx FIFO/Queue Status register
    // - reports current state of the TX FIFO/queue area
    // TFQPI = Tx FIFO/Queue Put Index field
    // - the element index where the next frame to be queued should be written
    uint32_t put = (fdcan->TXFQS & FDCAN_TXFQS_TFQPI_Msk) >> FDCAN_TXFQS_TFQPI_Pos;
    volatile uint32_t *tx = (volatile uint32_t *)(PHAL_FDCAN_priv_ramBase(fdcan)
                                + FDCAN_PRIV_SRAMCAN_TFQSA
                                + (put * FDCAN_PRIV_SRAMCAN_TFQ_SIZE));
 
    // Word 0 of a TX element header:
    // Bit 30 = XTD (extended id flag)
    // Standard id/extended id
    if (msg->IDE) {
        // Extended-ID frames place their 29-bit ID in bits [28:0]
        tx[0] = (msg->ExtId & 0x1FFFFFFFU) | (1U << 30); // extended ID, IDE=1
    } else {
        // Standard-ID frames place their 11-bit ID in bits [28:18]
        tx[0] = ((uint32_t)(msg->StdId & 0x7FFU) << 18);  // standard ID, IDE=0
    }

    // Word 1 of a TX element header:
    // DLC (Data Length Code): bits[19:16].
    // Left at 0:
    // - BRS (Bit Rate Switch) = 0 (n/a, classic frame)
    // - FDF (FD Format flag) = 0 (classic, non-FD frame)
    // - EFC (Event FIFO Control) = 0 (don't add a TX event for this frame)
    // - MM (Message Marker) = 0 (no user tag for the TX event)
    // - RTR (Remote Transmission Request) = 0 (data frame, not remote/request frame)
    // DLC -> length: for classic can, DLC=length <= 8
    uint32_t dlc = msg->DLC > 8U ? 8U : msg->DLC;
    tx[1] = dlc << 16;
 
    // Copy payload bytes into two 32-bit words
    uint32_t d0 = 0, d1 = 0;
    for (uint32_t i = 0; i < dlc && i < 4U; i++) {
        d0 |= (uint32_t)msg->Data[i] << (8U * i);
    }
    for (uint32_t i = 4; i < dlc; i++) {
        d1 |= (uint32_t)msg->Data[i] << (8U * (i - 4U));
    }
    tx[2] = d0;
    tx[3] = d1;
 
    // Mark the element as ready to transmit
    // TXBAR = Tx Buffer Add Request register
    // - writing a 1 to the bit for this element's index tells core the element is ready for TX
    fdcan->TXBAR = (1U << put);
}
 
bool PHAL_FDCAN_priv_readRxElement(FDCAN_GlobalTypeDef *fdcan, CanMsgTypeDef_t *msg) {
    // RXF0S = Rx FIFO 0 Status register
    // - reports current state of RX FIFO 0
    // F0FL = Fill Level field
    // - how many unread elements are currently sitting in RX FIFO 0
    uint32_t f0s = fdcan->RXF0S;
    if ((f0s & FDCAN_RXF0S_F0FL_Msk) == 0) {
        return false; // FIFO0 empty
    }
 
    // Get the index of the oldest element in RX FIFO0
    // F0GI = Get Index field
    // - the element index of the oldest unread frame in RX FIFO 0
    uint32_t get = (f0s & FDCAN_RXF0S_F0GI_Msk) >> FDCAN_RXF0S_F0GI_Pos;
    volatile uint32_t *rx = (volatile uint32_t *)(PHAL_FDCAN_priv_ramBase(fdcan)
                                + FDCAN_PRIV_SRAMCAN_RF0SA
                                + (get * FDCAN_PRIV_SRAMCAN_RF0_SIZE));
 
    // Decode the RX element header into a CanMsgTypeDef_t
    uint32_t w0 = rx[0];
    uint32_t w1 = rx[1];
 
    *msg     = (CanMsgTypeDef_t) {0};
    msg->Bus = fdcan;
    msg->IDE = ((w0 & (1U << 30)) != 0); // bit 30 = XTD (extended id flag)
    
    if (msg->IDE) {
        msg->ExtId = w0 & 0x1FFFFFFFU; // 29-bit extended ID
    } else {
        msg->StdId = (uint16_t)((w0 >> 18) & 0x7FFU); // 11-bit standard id
    }
 
    // Decode DLC
    uint8_t dlc = ((w1 >> 16) & 0xF);
    uint8_t len = dlc > 8U ? 8U : dlc; // classic CAN max payload is 8
    if (len > sizeof(msg->Data)) {
        len = sizeof(msg->Data);
    }
    msg->DLC = len;
 
    // Copy payload bytes
    const uint8_t* payload = (const uint8_t *)&rx[2];
    for (uint8_t i = 0; i < len; i++) {
        msg->Data[i] = payload[i];
    }
 
    // Pop the element we just read
    // RXF0A = Rx FIFO 0 Acknowledge register
    // - writing an element's index here tells the core it can reclaim the slot
    //   and advancing F0GI
    fdcan->RXF0A = get;

    return true;
}

bool PHAL_FDCAN_priv_readTxFifoQueueStatusFullFlag(const FDCAN_GlobalTypeDef *fdcan) {
    // TXFQS = Tx FIFO/Queue Status register
    // TFQF = Tx FIFO/Queue Full flag
    // - 1 when every TX FIFO/queue element is currently occupied by a pending/queued frame
    return (fdcan->TXFQS & FDCAN_TXFQS_TFQF) != 0;
}

bool PHAL_FDCAN_priv_readReceiveFifo0NewMessageInterruptFlag(const FDCAN_GlobalTypeDef *fdcan) {
    // IR = Interrupt Register
    // - latched interrupt flags
    // RF0N = Receive FIFO 0 New message flag
    // - set by hardware whenever a new frame has been stored into RX FIFO 0
    return (fdcan->IR & FDCAN_IR_RF0N) != 0;
}

void PHAL_FDCAN_priv_clearReceiveFifo0NewMessageInterruptFlag(FDCAN_GlobalTypeDef *fdcan) {
    // IR bits are write-1-to-clear
    // - writing RF0N (Receive FIFO 0 New message flag) back to IR clears only that flag
    fdcan->IR = FDCAN_IR_RF0N;
}

bool PHAL_FDCAN_priv_readTransmitCompleteInterruptFlag(const FDCAN_GlobalTypeDef *fdcan) {
    // TC = Transmission Completed flag within IR
    // - set by hardware once a queued TX element has finished transmitting successfully
    return (fdcan->IR & FDCAN_IR_TC) != 0;
}

void PHAL_FDCAN_priv_clearTransmitCompleteInterruptFlag(FDCAN_GlobalTypeDef *fdcan) {
    // Write-1-to-clear the TC (Transmission Completed) flag only
    fdcan->IR = FDCAN_IR_TC;
}
