/**
 * @file usart.c
 * @author Aditya Anand (anand89@purdue.edu) - Redesign of L4 UART HAL by Dawson Moore (moore800@purdue.edu)
 * @brief
 * @version 0.1
 * @date 2024-1-4
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "common/phal_F4/usart/usart.h"

// Comments labeled "ADD: " indicate that code needs to be modified in order to add extra peripherals

static inline uint32_t ROUNDDOWN(uint32_t a, uint32_t n) {
    return a - (a % n);
}
static inline uint32_t ROUNDUP(uint32_t a, uint32_t n) {
    return ROUNDDOWN(a + n - 1, n);
}

// These items should not be used/modified by anybody other than the HAL
typedef enum {
    USART_DMA_TX, //!< USART is transmitting over DMA
    USART_DMA_RX //!< USART is recieving over DMA
} usart_dma_mode_t;

typedef struct {
    usart_init_t* active_handle; //!< USART handle provided on initialization
    uint8_t cont_rx; //!< Flag controlling RX rececption mode (once or continously)
    uint8_t _tx_busy; //!< Waiting on a transmission to finish
    volatile uint8_t _rx_busy; //!< Waiting on a reception to finish
    volatile uint32_t rxfer_size; //!< Size of data to receive over DMA
} usart_active_transfer_t;

volatile usart_active_transfer_t active_uarts[TOTAL_NUM_UART];

bool PHAL_initUSART(usart_init_t* handle, const uint32_t fck) {
    // Locals
    uint8_t over8;
    uint32_t div;
    uint32_t carry;
    uint32_t div_fraction;
    uint32_t div_mantissa;

    // Add Handle to active peripheral set for keeping track of activity. The same handle must be passed throughout the use of the USART peripheral
    // If a different configuration of peripheral is to be used/a different handle, peripheral must be reinitalized.
    active_uarts[handle->usart_active_num].active_handle = handle;

    // Disable peripheral until properly configured
    handle->periph->CR1 &= ~USART_CR1_UE;

// Enable peripheral clock in RCC
// ADD: When adding a new peripheral, be sure to enable the clock here
// RM0090 213
#ifdef STM32F407xx
    switch ((ptr_int)handle->periph) {
        case USART1_BASE:
            RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
            break;
        case USART2_BASE:
            RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
            break;
        case USART3_BASE:
            RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
            break;
        case UART4_BASE:
            RCC->APB1ENR |= RCC_APB1ENR_UART4EN;
            break;
        case UART5_BASE:
            RCC->APB1ENR |= RCC_APB1ENR_UART5EN;
            break;
        case USART6_BASE:
            RCC->APB2ENR |= RCC_APB2ENR_USART6EN;
            break;
        default:
            return false;
    }
#endif

    // The following FBRG configuration is based off RM0090 p. 978 - 980
    over8 = (uint8_t)(8 << !handle->ovsample);
    carry = 0;
    // Calculate USARTDIV constant for given baud rate, convert to fixed point for easy manipulation
    div = (uint32_t)(((float)fck / (float)(handle->baud_rate * over8)) * 100);

    //Calculate DIV_Fraction value, round to nearest real number, then acquire carry value if div_fraction > 4bits
    div_fraction = (over8 * (div % 100));
    div_fraction = ((div_fraction > 50) ? ROUNDUP(div_fraction, 100) : ROUNDDOWN(div_fraction, 100)) / 100;
    if (div_fraction > 15 && handle->ovsample == OV_16) {
        carry        = div_fraction - 15;
        div_fraction = 15;
    }
    // The last DIV_Fraction bit is ignored in OV8 mode
    else if (div_fraction > 7 && handle->ovsample == OV_8) {
        carry        = div_fraction - 7;
        div_fraction = 7;
    }
    //Calculate DIV_Mantissa value, accounting for any carry values from the previous DIV_Fraction calculation
    div_mantissa = (div / 100) + carry;
    // Apply prescalar
    handle->periph->BRR = 0U; //Clear any previously programmed values
    handle->periph->BRR |= div_fraction << USART_BRR_DIV_Fraction_Pos;
    handle->periph->BRR |= div_mantissa << USART_BRR_DIV_Mantissa_Pos;

    // Set CR1 parameters
    handle->periph->CR1 = 0U;
    handle->periph->CR1 |= (uint32_t)(handle->parity != PT_NONE) << USART_CR1_PCE_Pos;
    handle->periph->CR1 |= handle->word_length << USART_CR1_M_Pos;
    handle->periph->CR1 |= (handle->parity >> 2) << USART_CR1_PS_Pos;
    handle->periph->CR1 |= handle->ovsample << USART_CR1_OVER8_Pos;
    handle->periph->CR1 |= handle->wake_addr << USART_CR1_WAKE_Pos;
    handle->periph->CR1 |= USART_CR1_RXNEIE | USART_CR1_IDLEIE;

    // Set CR2 parameters
    handle->periph->CR2 = 0U;
    handle->periph->CR2 |= (handle->address & 0xF) << USART_CR2_ADD_Pos;
    handle->periph->CR2 |= handle->stop_bits << USART_CR2_STOP_Pos;

    // // Set CR3 parameters
    handle->periph->CR3 |= handle->obsample << USART_CR3_ONEBIT_Pos;
    // handle->periph->CR3 |= USART_CR3_EIE;

    // Enable peripheral for use
    handle->periph->CR1 |= USART_CR1_UE;

    // Blocking is currently not supported
    if (!handle->rx_dma_cfg || !handle->tx_dma_cfg)
        return false;
    // Configure DMA
    if (!PHAL_initDMA(handle->tx_dma_cfg)) {
        return false;
    }

    if (!PHAL_initDMA(handle->rx_dma_cfg)) {
        return false;
    }

    return true;
}

void PHAL_usartTxBl(usart_init_t* handle, uint8_t* data, uint32_t len) {
    unsigned int i;

    handle->periph->CR1 |= USART_CR1_TE;

#ifdef STM32F407xx

    for (i = 0; i < len; i++) {
        while (!(handle->periph->SR & USART_SR_TXE))
            ;
        handle->periph->DR = data[i] & 0xff;
    }

    while (!(handle->periph->SR & USART_SR_TC))
        ;

#endif

}

void PHAL_usartRxBl(usart_init_t* handle, uint8_t* data, uint32_t len) {
    unsigned int i;
    handle->periph->CR1 |= USART_CR1_RE;

#ifdef STM32F407xx

    for (i = 0; i < len; i++) {
        while (!(handle->periph->SR & USART_SR_RXNE))
            ;
        data[i] = handle->periph->DR & 0xff;
    }

#endif

}

bool PHAL_usartTxDma(usart_init_t* handle, uint16_t* data, uint32_t len) {
    if (active_uarts[handle->usart_active_num].active_handle != handle)
        return false;

// Enable All Interrupts needed to complete Tx transaction
// ADD: Ensure you enable the TX DMA interrupt for a new UART peripheral
// RM0090 PG 310
// Read table to ensure DMA streams do not overlap
#ifdef STM32F407xx
    switch ((ptr_int)handle->periph) {
        case USART1_BASE:
            NVIC_EnableIRQ(DMA2_Stream7_IRQn);
            break;
        case USART2_BASE:
            NVIC_EnableIRQ(DMA1_Stream6_IRQn);
            break;
        case USART3_BASE:
            NVIC_EnableIRQ(DMA1_Stream3_IRQn);
            break;
        case UART4_BASE:
            NVIC_EnableIRQ(DMA1_Stream4_IRQn);
            break;
        case UART5_BASE:
            NVIC_EnableIRQ(DMA1_Stream7_IRQn);
            break;
        case USART6_BASE:
            NVIC_EnableIRQ(DMA2_Stream6_IRQn);
            break;
        default:
            return false;
    }
#endif
    PHAL_DMA_setTxferLength(handle->tx_dma_cfg, len);
    PHAL_DMA_setMemAddress(handle->tx_dma_cfg, (uint32_t)data);

    active_uarts[handle->usart_active_num]._tx_busy = 1;
    // Configure DMA Peripheral, and set up USART for DMA Transactions
    handle->periph->CR3 |= USART_CR3_DMAT;
    handle->periph->CR1 |= USART_CR1_TE;
#ifdef STM32F407xx
// Ensure any RX data is not overwritten before continuing with transfer
// while ((active_uarts[handle->usart_active_num].active_handle->periph->SR & USART_SR_RXNE))
//     ;
#endif
    // Start DMA transaction
    PHAL_DMA_startTxfer(handle->tx_dma_cfg);
    return true;
}

volatile bool PHAL_usartTxBusy(usart_init_t* handle) {
    return active_uarts[handle->usart_active_num]._tx_busy;
}

bool PHAL_usartRxDma(usart_init_t* handle, uint16_t* data, uint32_t len, bool cont) {
    // Provided handle must match with the one we have
    if (active_uarts[handle->usart_active_num].active_handle != handle)
        return false;
    // Keep track of this information for later use in USART interrupt
    active_uarts[handle->usart_active_num].cont_rx    = cont;
    active_uarts[handle->usart_active_num].rxfer_size = len;
    handle->periph->CR1 |= USART_CR1_RE;
// Enable All Interrupts needed to complete Tx transaction
// ADD: Add cases to this switch statement to enable DMA and USART interrupts for RX USART peripherals
#ifdef STM32F407xx
    switch ((ptr_int)handle->periph) {
        case USART1_BASE:
            NVIC_EnableIRQ(DMA2_Stream5_IRQn);
            NVIC_EnableIRQ(USART1_IRQn);
            break;
        case USART2_BASE:
            NVIC_EnableIRQ(DMA1_Stream5_IRQn);
            NVIC_EnableIRQ(USART2_IRQn);
            break;
        case USART3_BASE:
            NVIC_EnableIRQ(DMA1_Stream1_IRQn);
            NVIC_EnableIRQ(USART3_IRQn);
            break;
        case UART4_BASE:
            NVIC_EnableIRQ(DMA1_Stream2_IRQn);
            NVIC_EnableIRQ(UART4_IRQn);
            break;
        case UART5_BASE:
            NVIC_EnableIRQ(DMA1_Stream0_IRQn);
            NVIC_EnableIRQ(UART5_IRQn);
            break;
        case USART6_BASE:
            NVIC_EnableIRQ(DMA2_Stream1_IRQn);
            NVIC_EnableIRQ(USART6_IRQn);
            break;
        default:
            return false;
    }
#endif
    // Configure parts of DMA that will not change each transaction, and enable DMA on USART
    PHAL_DMA_setMemAddress(handle->rx_dma_cfg, (uint32_t)data);
    handle->periph->CR3 |= USART_CR3_DMAR;
    return true;
}

bool PHAL_disableContinousRxDMA(usart_init_t* handle) {
    // Provided handle must match with the one we have
    if (active_uarts[handle->usart_active_num].active_handle != handle)
        return false;
    active_uarts[handle->usart_active_num].cont_rx = 0;
    handle->periph->CR1 &= ~USART_CR1_RE;
    handle->periph->CR3 &= ~USART_CR3_DMAR;
// Enable All Interrupts needed to complete Tx transaction
// ADD: Add cases to this switch statement to disable interrupts for new USART peripherals
#ifdef STM32F407xx
    switch ((ptr_int)handle->periph) {
        case USART1_BASE:
            NVIC_DisableIRQ(DMA2_Stream5_IRQn);
            NVIC_DisableIRQ(USART1_IRQn);
            break;
        case USART2_BASE:
            NVIC_DisableIRQ(DMA1_Stream5_IRQn);
            NVIC_DisableIRQ(USART2_IRQn);
            break;
        case USART3_BASE:
            NVIC_DisableIRQ(DMA1_Stream1_IRQn);
            NVIC_DisableIRQ(USART3_IRQn);
            break;
        case UART4_BASE:
            NVIC_DisableIRQ(DMA1_Stream2_IRQn);
            NVIC_DisableIRQ(UART4_IRQn);
            break;
        case UART5_BASE:
            NVIC_DisableIRQ(DMA1_Stream0_IRQn);
            NVIC_DisableIRQ(UART5_IRQn);
            break;
        case USART6_BASE:
            NVIC_DisableIRQ(DMA2_Stream1_IRQn);
            NVIC_DisableIRQ(USART6_IRQn);
            break;
        default:
            return false;
    }
#endif
    // RX will no longer be busy
    active_uarts[handle->usart_active_num]._rx_busy = 0;

    return true;
}

bool PHAL_usartRxBusy(usart_init_t* handle) {
    return active_uarts[handle->usart_active_num]._rx_busy;
}

static void handleUsartIRQ(USART_TypeDef* handle, uint8_t idx) {
#ifdef STM32F407xx
    uint32_t sr = handle->SR;
    // USART RX Not Empty interrupt flag
    if (sr & USART_SR_RXNE) {
        // Rx transaction is beginning, so set rx to busy and enable DMA to recieve this message
        active_uarts[idx]._rx_busy = 1;
        PHAL_DMA_setTxferLength(active_uarts[idx].active_handle->rx_dma_cfg, active_uarts[idx].rxfer_size);
        PHAL_DMA_reEnable(active_uarts[idx].active_handle->rx_dma_cfg);
        active_uarts[idx].active_handle->periph->SR &= ~USART_SR_RXNE; // Clear RXNE interrupt flag
        // We only need to enable DMA immediately after the reception of the first bit
        // We also do not want this interrupt activating for every single bit recieved on the rx buffer
        active_uarts[idx].active_handle->periph->CR1 &= ~USART_CR1_RXNEIE;
        // Clear any errors that may have been set in the previous Rx
        active_uarts[idx].active_handle->rx_errors.framing_error  = 0;
        active_uarts[idx].active_handle->rx_errors.noise_detected = 0;
        active_uarts[idx].active_handle->rx_errors.overrun        = 0;
        active_uarts[idx].active_handle->rx_errors.parity_error   = 0;
    }
    // Clear to send flag
    if (sr & USART_SR_CTS) {
        sr &= ~USART_SR_CTS; // We currently do not plan on using CTS or LIN mode in the near future, so this is placeholder code for the future
    }
    // Lin Break Detection flag
    if (sr & USART_SR_LBD) {
        sr &= ~USART_SR_LBD; // We currently do not plan on using CTS or LIN mode in the near future, so this is placeholder code for the future
    }
    // Overrun Error Flag
    if (sr & USART_SR_ORE) {
        // Communicate these errors to the user, so we can clear this bit but leave USART clear for future transactions
        active_uarts[idx].active_handle->rx_errors.overrun = 1;
    }
    // Noise Error Flag
    if (sr & USART_SR_NE) {
        // Communicate these errors to the user, so we can clear this bit for future transactions
        active_uarts[idx].active_handle->rx_errors.noise_detected = 1;
    }
    // Framing Error Flag
    if (sr & USART_SR_FE) {
        // Communicate these errors to the user, so we can clear this bit for future transactions
        active_uarts[idx].active_handle->rx_errors.framing_error = 1;
    }
    // Parity Error Flag
    if (sr & USART_SR_PE) {
        // Communicate these errors to the user, so we can clear this bit for future transactions
        active_uarts[idx].active_handle->rx_errors.parity_error = 1;
    }

    // Idle line is detected upon the completion of a USART reception
    // This is the last flag handled, so that important info can be updated before callback function
    if (sr & USART_SR_IDLE) {
        // Stop DMA Transaction
        PHAL_DMA_stopTxfer(active_uarts[idx].active_handle->rx_dma_cfg);
        if (active_uarts[idx].cont_rx) {
            // Re-enable the RX not empty interrupt to accept the next message
            active_uarts[idx].active_handle->periph->CR1 |= USART_CR1_RXNEIE;
        } else {
            // No more messages should be recieved, so disable RX mode for the peripheral
            active_uarts[idx].active_handle->periph->CR1 &= ~USART_CR1_RE;
        }
        active_uarts[idx]._rx_busy = 0;
        // Clear error + idle flags (Per RM 0090 Register map, a read to the SR followed by a read to the DR)
        (void)active_uarts[idx].active_handle->periph->SR;
        (void)active_uarts[idx].active_handle->periph->DR;
        usart_recieve_complete_callback(active_uarts[idx].active_handle);
    }
//NOTE: According to RM0090, it is not safe to clear the TC bit unless Multibuffer mode is enabled.
#endif

#ifdef STM32F732xx
    uint32_t sr = handle->ISR;
    // USART RX Not Empty interrupt flag
    if (sr & USART_ISR_RXNE) {
        // Rx transaction is beginning, so set rx to busy and enable DMA to recieve this message
        active_uarts[idx]._rx_busy = 1;
        PHAL_DMA_setTxferLength(active_uarts[idx].active_handle->rx_dma_cfg, active_uarts[idx].rxfer_size);
        PHAL_DMA_reEnable(active_uarts[idx].active_handle->rx_dma_cfg);
        // QUESTION:
        // active_uarts[idx].active_handle->periph->ICR |= USART_ICR_RXNECF; // Clear RXNE interrupt flag
        // (void)active_uarts[idx].active_handle->periph->; // Clear RXNE interrupt flag
        // We only need to enable DMA immediately after the reception of the first bit
        // We also do not want this interrupt activating for every single bit recieved on the rx buffer
        active_uarts[idx].active_handle->periph->CR1 &= ~USART_CR1_RXNEIE;
        // Clear any errors that may have been set in the previous Rx
        active_uarts[idx].active_handle->rx_errors.framing_error  = 0;
        active_uarts[idx].active_handle->rx_errors.noise_detected = 0;
        active_uarts[idx].active_handle->rx_errors.overrun        = 0;
        active_uarts[idx].active_handle->rx_errors.parity_error   = 0;
    }
    // Clear to send flag
    if (sr & USART_ISR_CTS) {
        active_uarts[idx].active_handle->periph->ICR |= USART_ICR_CMCF; // We currently do not plan on using CTS or LIN mode in the near future, so this is placeholder code for the future
    }
    // Lin Break Detection flag
    if (sr & USART_ISR_LBDF) {
        active_uarts[idx].active_handle->periph->ICR |= USART_ICR_LBDCF; // We currently do not plan on using CTS or LIN mode in the near future, so this is placeholder code for the future
    }
    // Overrun Error Flag
    if (sr & USART_ISR_ORE) {
        // Communicate these errors to the user, so we can clear this bit but leave USART clear for future transactions
        active_uarts[idx].active_handle->rx_errors.overrun = 1;
        active_uarts[idx].active_handle->periph->ICR |= USART_ICR_ORECF;
    }
    // Noise Error Flag
    if (sr & USART_ISR_NE) {
        // Communicate these errors to the user, so we can clear this bit for future transactions
        active_uarts[idx].active_handle->rx_errors.noise_detected = 1;
        active_uarts[idx].active_handle->periph->ICR |= USART_ICR_NCF;
    }
    // Framing Error Flag
    if (sr & USART_ISR_FE) {
        // Communicate these errors to the user, so we can clear this bit for future transactions
        active_uarts[idx].active_handle->rx_errors.framing_error = 1;
        active_uarts[idx].active_handle->periph->ICR |= USART_ICR_FECF;
    }
    // Parity Error Flag
    if (sr & USART_ISR_PE) {
        // Communicate these errors to the user, so we can clear this bit for future transactions
        active_uarts[idx].active_handle->rx_errors.parity_error = 1;
        active_uarts[idx].active_handle->periph->ICR |= USART_ICR_PECF;
    }

    // Idle line is detected upon the completion of a USART reception
    // This is the last flag handled, so that important info can be updated before callback function
    if (sr & USART_ISR_IDLE) {
        // Stop DMA Transaction
        PHAL_DMA_stopTxfer(active_uarts[idx].active_handle->rx_dma_cfg);
        if (active_uarts[idx].cont_rx) {
            // Re-enable the RX not empty interrupt to accept the next message
            active_uarts[idx].active_handle->periph->CR1 |= USART_CR1_RXNEIE;
        } else {
            // No more messages should be recieved, so disable RX mode for the peripheral
            active_uarts[idx].active_handle->periph->CR1 &= ~USART_CR1_RE;
        }
        active_uarts[idx]._rx_busy = 0;
        // Clear idle flag
        // (void)active_uarts[idx].active_handle->periph->ISR;
        // (void)active_uarts[idx].active_handle->periph->DR;
        active_uarts[idx].active_handle->periph->ICR |= USART_ICR_IDLECF;
        usart_recieve_complete_callback(active_uarts[idx].active_handle);
    }
//NOTE: According to 0431, there is no point to clearing the TC bit (we do not handle transmissions here anyway)
#endif
}

/**
 * @brief Handle DMA interrupts
 *
 */
static void handleDMAxComplete(uint8_t idx, uint32_t irq, uint8_t dma_type) {
    // Bitmask for each DMA interrupt flag
    uint32_t teif_flag;
    uint32_t tcif_flag;
    uint32_t htif_flag;
    uint32_t feif_flag;
    uint32_t dmeif_flag;

    // Clear register for DMA Stream
    volatile uint32_t* sr_reg;
    volatile uint32_t* csr_reg;
    uint8_t dma_num;

    // Populate values based on whether DMA Rx or Tx is being handled (Different configurations, streams)
    if (dma_type == USART_DMA_TX) {
        dma_num = active_uarts[idx].active_handle->tx_dma_cfg->stream_idx;

        // Select appropriate flag and clear registers for Stream
        sr_reg  = (dma_num <= 3) ? &active_uarts[idx].active_handle->tx_dma_cfg->periph->LISR : &active_uarts[idx].active_handle->tx_dma_cfg->periph->HISR;
        csr_reg = (dma_num <= 3) ? &active_uarts[idx].active_handle->tx_dma_cfg->periph->LIFCR : &active_uarts[idx].active_handle->tx_dma_cfg->periph->HIFCR;
    } else // DMA RX is ongoing
    {
        dma_num = active_uarts[idx].active_handle->rx_dma_cfg->stream_idx;

        // Select appropriate flag and clear registers for Stream
        sr_reg  = (dma_num <= 3) ? &active_uarts[idx].active_handle->rx_dma_cfg->periph->LISR : &active_uarts[idx].active_handle->rx_dma_cfg->periph->HISR;
        csr_reg = (dma_num <= 3) ? &active_uarts[idx].active_handle->rx_dma_cfg->periph->LIFCR : &active_uarts[idx].active_handle->rx_dma_cfg->periph->HIFCR;
    }
    // Populate Flag Bitmasks, along with Flag and Clear registers for active DMA Stream
    switch (dma_num) {
        case 0:
            // Populate Flag Bitmasks
            teif_flag  = DMA_LISR_TEIF0;
            tcif_flag  = DMA_LISR_TCIF0;
            htif_flag  = DMA_LISR_HTIF0;
            feif_flag  = DMA_LISR_FEIF0;
            dmeif_flag = DMA_LISR_DMEIF0;
            break;
        case 1:
            // Populate Flag Bitmasks
            teif_flag  = DMA_LISR_TEIF1;
            tcif_flag  = DMA_LISR_TCIF1;
            htif_flag  = DMA_LISR_HTIF1;
            feif_flag  = DMA_LISR_FEIF1;
            dmeif_flag = DMA_LISR_DMEIF1;
            break;
        case 2:
            // Populate Flag Bitmasks
            teif_flag  = DMA_LISR_TEIF2;
            tcif_flag  = DMA_LISR_TCIF2;
            htif_flag  = DMA_LISR_HTIF2;
            feif_flag  = DMA_LISR_FEIF2;
            dmeif_flag = DMA_LISR_DMEIF2;
            break;
        case 3:
            // Populate Flag Bitmasks
            teif_flag  = DMA_LISR_TEIF3;
            tcif_flag  = DMA_LISR_TCIF3;
            htif_flag  = DMA_LISR_HTIF3;
            feif_flag  = DMA_LISR_FEIF3;
            dmeif_flag = DMA_LISR_DMEIF3;
            break;
        case 4:
            // Populate Flag Bitmasks
            teif_flag  = DMA_HISR_TEIF4;
            tcif_flag  = DMA_HISR_TCIF4;
            htif_flag  = DMA_HISR_HTIF4;
            feif_flag  = DMA_HISR_FEIF4;
            dmeif_flag = DMA_HISR_DMEIF4;
            break;
        case 5:
            // Populate Flag Bitmasks
            teif_flag  = DMA_HISR_TEIF5;
            tcif_flag  = DMA_HISR_TCIF5;
            htif_flag  = DMA_HISR_HTIF5;
            feif_flag  = DMA_HISR_FEIF5;
            dmeif_flag = DMA_HISR_DMEIF5;
            break;
        case 6:
            // Populate Flag Bitmasks
            teif_flag  = DMA_HISR_TEIF6;
            tcif_flag  = DMA_HISR_TCIF6;
            htif_flag  = DMA_HISR_HTIF6;
            feif_flag  = DMA_HISR_FEIF6;
            dmeif_flag = DMA_HISR_DMEIF6;
            break;
        case 7:
            // Populate Flag Bitmasks
            teif_flag  = DMA_HISR_TEIF7;
            tcif_flag  = DMA_HISR_TCIF7;
            htif_flag  = DMA_HISR_HTIF7;
            feif_flag  = DMA_HISR_FEIF7;
            dmeif_flag = DMA_HISR_DMEIF4;
            break;
        default:
            // Invalid stream selected, do not attempt to service interrupt
            return;
    }

    // Transfer Error interrupt
    if (*sr_reg & teif_flag) {
        // Communicate these errors to the user, so we can clear this bit for future transactions
        // Using the DMA ISR for Tx is easier than using the usart interrupt, as you don't need to worry about which transaction (tx or rx) is occuring
        if (dma_type == USART_DMA_TX) {
            active_uarts[idx].active_handle->tx_errors.dma_transfer_error = 1;
        } else {
            active_uarts[idx].active_handle->rx_errors.dma_transfer_error = 1;
        }
        *csr_reg |= teif_flag;
    } else {
        // There was no error, so clear the bit if it was originally set
        if (dma_type == USART_DMA_TX) {
            active_uarts[idx].active_handle->tx_errors.dma_transfer_error = 0;
        } else {
            active_uarts[idx].active_handle->rx_errors.dma_transfer_error = 0;
        }
    }
    // Transfer Complete interrupt flag
    if (*sr_reg & tcif_flag) {
        if (dma_type == USART_DMA_TX) {
            // TX is complete, so we no longer need this DMA stream active
            PHAL_DMA_stopTxfer(active_uarts[idx].active_handle->tx_dma_cfg);
#ifdef STM32F732xx
// Wait for the transfer complete bit to be set, indicating the completion of USART transaction
// while (!(active_uarts[idx].active_handle->periph->ISR & USART_ISR_TC))
//     ;
#endif
            // TX is no longer busy, so communicate this and disable TX part of USART
            active_uarts[idx]._tx_busy = 0;
        }
        //Clear interrupt flag
        *csr_reg |= tcif_flag;
    }
    // Half transfer complete flag
    if ((*sr_reg) & htif_flag) {
        *csr_reg |= htif_flag;
    }
    // FIFO Overrun Error flag
    // Apparently FIFO errors can safely be ignored in the configuration that we use (no FIFO)
    if (*sr_reg & feif_flag) {
        // Communicate these errors to the user, so we can clear this bit for future transactions
        if (dma_type == USART_DMA_TX) {
            active_uarts[idx].active_handle->tx_errors.dma_fifo_overrun = 1;
        } else {
            active_uarts[idx].active_handle->rx_errors.dma_fifo_overrun = 1;
        }
        *csr_reg |= feif_flag;
    } else {
        // There was no error, so clear the bit if it was originally set
        if (dma_type == USART_DMA_TX) {
            active_uarts[idx].active_handle->tx_errors.dma_fifo_overrun = 0;
        } else {
            active_uarts[idx].active_handle->rx_errors.dma_fifo_overrun = 0;
        }
    }
    // Direct Mode Error flag
    if (*sr_reg & dmeif_flag) {
        // Communicate these errors to the user, so we can clear this bit for future transactions
        if (dma_type == USART_DMA_TX) {
            active_uarts[idx].active_handle->tx_errors.dma_direct_mode_error = 1;
        } else {
            active_uarts[idx].active_handle->rx_errors.dma_direct_mode_error = 1;
        }
        *csr_reg |= dmeif_flag;
    } else {
        // There was no error, so clear the bit if it was originally set
        if (dma_type == USART_DMA_TX) {
            active_uarts[idx].active_handle->tx_errors.dma_direct_mode_error = 0;
        } else {
            active_uarts[idx].active_handle->rx_errors.dma_direct_mode_error = 0;
        }
    }
}

__WEAK void usart_recieve_complete_callback(usart_init_t* handle) {
    return;
}

/*
 * USART TX and RX interrupts - need to modify when adding a usart peripheral
 * Add new DMA interrupt handlers here, passing in new active struct index, along with
 * the correct DMA IRQ, and corresponding transfer mode (TX or Rx)
 */

//ADD:
#ifdef STM32F407xx
void DMA2_Stream5_IRQHandler() {
    handleDMAxComplete(USART1_ACTIVE_IDX, DMA2_Stream5_IRQn, USART_DMA_RX);
} // USART1 RX

void DMA2_Stream7_IRQHandler() {
    handleDMAxComplete(USART1_ACTIVE_IDX, DMA2_Stream7_IRQn, USART_DMA_TX);
} // USART1 TX

void DMA1_Stream5_IRQHandler() {
    handleDMAxComplete(USART2_ACTIVE_IDX, DMA1_Stream5_IRQn, USART_DMA_RX);
} // USART2 RX

void DMA1_Stream6_IRQHandler() {
    handleDMAxComplete(USART2_ACTIVE_IDX, DMA1_Stream6_IRQn, USART_DMA_TX);
} // USART2 TX

void DMA1_Stream1_IRQHandler() {
    handleDMAxComplete(USART3_ACTIVE_IDX, DMA1_Stream1_IRQn, USART_DMA_RX);
} // USART3 RX

void DMA1_Stream3_IRQHandler() {
    handleDMAxComplete(USART3_ACTIVE_IDX, DMA1_Stream3_IRQn, USART_DMA_TX);
} // USART3 TX

void DMA1_Stream2_IRQHandler() {
    handleDMAxComplete(USART4_ACTIVE_IDX, DMA1_Stream2_IRQn, USART_DMA_RX);
} // USART4 RX

void DMA1_Stream4_IRQHandler() {
    handleDMAxComplete(USART4_ACTIVE_IDX, DMA1_Stream4_IRQn, USART_DMA_TX);
} // USART4 TX

void DMA1_Stream0_IRQHandler() {
    handleDMAxComplete(USART5_ACTIVE_IDX, DMA1_Stream0_IRQn, USART_DMA_RX);
} // USART5 RX

void DMA1_Stream7_IRQHandler() {
    handleDMAxComplete(USART5_ACTIVE_IDX, DMA1_Stream7_IRQn, USART_DMA_TX);
} // USART5 TX

void DMA2_Stream1_IRQHandler() {
    handleDMAxComplete(USART6_ACTIVE_IDX, DMA2_Stream1_IRQn, USART_DMA_RX);
} // USART6 RX

//void DMA2_Stream6_IRQHandler() { handleDMAxComplete(USART6_ACTIVE_IDX, DMA2_Stream6_IRQn, USART_DMA_TX); } // USART6 TX

// Add new USART Interrupts as new peripherals are needed,
// feeding in the new USART peripheral, along with active array index
void USART1_IRQHandler() {
    handleUsartIRQ(USART1, USART1_ACTIVE_IDX);
}

void USART2_IRQHandler() {
    handleUsartIRQ(USART2, USART2_ACTIVE_IDX);
}

void USART3_IRQHandler() {
    handleUsartIRQ(USART3, USART3_ACTIVE_IDX);
}

void UART4_IRQHandler() {
    handleUsartIRQ(UART4, USART4_ACTIVE_IDX);
}

void UART5_IRQHandler() {
    handleUsartIRQ(UART5, USART5_ACTIVE_IDX);
}

//void USART6_IRQHandler() { handleUsartIRQ(USART6, USART6_ACTIVE_IDX); }

#endif

#ifdef STM32F732xx
// USART1:
void DMA2_Stream7_IRQHandler() //TX
{
    handleDMAxComplete(USART1_ACTIVE_IDX, DMA2_Stream7_IRQn, USART_DMA_TX);
}

void DMA2_Stream5_IRQHandler() //RX
{
    handleDMAxComplete(USART1_ACTIVE_IDX, DMA2_Stream5_IRQn, USART_DMA_RX);
}

// USART2:

// Add new DMA interrupt handlers here, passing in new active struct index, along with
// the correct DMA IRQ, and corresponding transfer mode (TX or Rx)
void DMA1_Stream4_IRQHandler() //TX
{
    handleDMAxComplete(USART4_ACTIVE_IDX, DMA1_Stream4_IRQn, USART_DMA_TX);
}

// Add new DMA interrupt handlers here, passing in new active struct index, along with
// the correct DMA IRQ, and corresponding transfer mode (TX or Rx)
void DMA1_Stream2_IRQHandler() //RX
{
    handleDMAxComplete(USART4_ACTIVE_IDX, DMA1_Stream2_IRQn, USART_DMA_RX);
}

// Add new USART Interrupts as new peripherals are needed,
// feeding in the new USART peripheral, along with active array index

void USART1_IRQHandler() {
    handleUsartIRQ(USART1, USART1_ACTIVE_IDX);
}

void UART4_IRQHandler() {
    handleUsartIRQ(UART4, USART4_ACTIVE_IDX);
}

// USART3:

#endif
//...etc