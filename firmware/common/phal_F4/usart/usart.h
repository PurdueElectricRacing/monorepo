/**
 * @file usart.h
 * @author Aditya Anand (anand89@purdue.edu) - Redesign of L4 USART HAL by Dawson Moore (moore800@purdue.edu)
 * @brief
 * @version 0.1
 * @date 2024-1-4
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef _PHAL_USART_H_
#define _PHAL_USART_H_

// Includes
#if defined(STM32F423xx)
#define TOTAL_NUM_UART 8

// Active Transfer list indexes (Add to this list if updating TOTAL_NUM_UART)
#define USART1_ACTIVE_IDX 0
#define USART2_ACTIVE_IDX 1
#define USART3_ACTIVE_IDX 2
#define USART4_ACTIVE_IDX 3
#define USART5_ACTIVE_IDX 4
#define USART6_ACTIVE_IDX 5
#define USART7_ACTIVE_IDX 6
#define USART8_ACTIVE_IDX 7
#endif

#include "common/phal_F4/dma/dma.h"
#include "common/phal_F4/phal_F4.h"

typedef uint32_t ptr_int;

// See USART section of Family reference manual (RM0430 for F4, or RM0431 for F7) for configuration information

// Enumerations
// See Table 146 in RM. 0090
// By enabling Parity, you sacrifice one bit from your total word length
// (eg. if WORD_8 is selected, you now have 7 data bits and 1 parity bit)
typedef enum {
    PT_EVEN = 0b010, //!< Even Parity
    PT_ODD  = 0b100, //!< Odd Parity
    PT_NONE = 0b000 //!< Disable Parity bits
} parity_t;

typedef enum {
    WORD_8, //!< 8-Bit word length
    WORD_9, //!< 9-Bit word length
} word_length_t;

typedef enum {
    SB_ONE      = 0b00, //!< One Stop bit
    SB_TWO      = 0b01, //!< Two Stop bits
    SB_HALF     = 0b10, //!< One half stop bit
    SB_ONE_HALF = 0b11 //!< 1.5 Stop bits
} stop_bits_t;

typedef enum {
    HW_DISABLE, //!< Hardware flow control disable
    CTS, //!< Enable Clear to send control
    RTS, //!< Enable Request to send control
    CTS_RTS //!< Enable both CTS and RTS
} hw_flow_ctl_t;

typedef enum {
    OV_16 = 0, //!< Oversample by 16
    OV_8  = 1 //!< Oversample by 8
} ovsample_t;

typedef enum {
    OB_DISABLE, //!< Disable one bit sampling
    OB_ENABLE //!< Enable one bit sampling
} obsample_t;

typedef struct
{
    bool overrun; //!< USART unable to parse data in time
    bool noise_detected; //!< Oversampling detected a possible bit flip due to noise in usart frame
    bool framing_error; //!< Unable to understand USART frame
    bool parity_error; //!< USART Parity bit incorrect (Only when parity is enabled)

    // DMA Errors
    bool dma_transfer_error; //!< DMA transfer error
    bool dma_direct_mode_error; //!< DMA error while attempting to operate in direct mode
    bool dma_fifo_overrun; //!< DMA FIFO has been overrun - apparently this can be ignored on USART peripherals AS LONG AS YOU AREN'T USING THE FIFO
} usart_rx_errors_t;

typedef struct
{
    bool dma_transfer_error; //!< DMA transfer error
    bool dma_direct_mode_error; //!< DMA error while attempting to operate in direct mode
    bool dma_fifo_overrun; //!< DMA FIFO has been overrun - apparently this can be ignored on USART peripherals AS LONG AS YOU AREN'T USING THE FIFO
} usart_tx_errors_t;

// Structures
typedef struct
{
    // Required parameters
    uint32_t baud_rate; //!< Baud rate for communication
    word_length_t word_length; //!< Word length for tx/rx (8 default)
    stop_bits_t stop_bits; //!< Number of stop bits to use (1 default)
    parity_t parity; //!< Parity of communication (none default)
    hw_flow_ctl_t hw_flow_ctl; //!< Special hardware modes (none default)
    ovsample_t ovsample; //!< 8x or 16x oversample (16x default)
    obsample_t obsample; //!< One bit sampling enable (off default)
    bool wake_addr; //!< Wake up when given a specific address
    uint8_t address; //!< Address to wake up to when addr_mode is enabled
    uint8_t usart_active_num; //!< Index of USART in active array (see USARTx_ACTIVE_IDX)

    // DMA configurations
    dma_init_t* tx_dma_cfg; //!< TX configuration
    dma_init_t* rx_dma_cfg; //!< RX configuration
    USART_TypeDef* periph; //!< USART Peripheral to be used

    // Structs to communicate errors to user
    volatile usart_tx_errors_t tx_errors; //!< Any TX error flags set during transmission
    volatile usart_rx_errors_t rx_errors; //!< Any RX error flags set during reception
} usart_init_t;

// Function Prototypes

/**
  * @brief Initialize a USART Peripheral with desired settings
  *
  * @param handle Handle containing settings for USART peripheral
  * @param fck Clock rate going to USART peripheral (APBx bus)
  * @return true Successfully initialized USART peripheral
  * @return false Failed to initialize USART peripheral
  */
bool PHAL_initUSART(usart_init_t* handle, const uint32_t fck);

/**
  * @brief           TX using no DMA (blocks until complete)
  * @param handle    The handle for the usart configuration
  * @param data      The address of the data to send
  * @param len       Number of u8s
  */
void PHAL_usartTxBl(usart_init_t* handle, uint8_t* data, uint32_t len);

/**
  * @brief           RX using no DMA (blocks until complete)
  * @param handle    The handle for the usart configuration
  * @param data      The address of the data to receive
  * @param len       Number of u8s expected to receive. Will block if not received.
  */
void PHAL_usartRxBl(usart_init_t* handle, uint8_t* data, uint32_t len);

/**
  * @brief           Starts a tx using dma, use PHAL_usartTxDmaComplete
  *                  to ensure the previous transmission is complete
  * @param handle    The handle for the usart configuration
  * @param data      The address of the data to send, ensure a cast to (uint16_t *), even if 8 bits
  * @param len       Number of units of data, depending on the configured word length
  */
bool PHAL_usartTxDma(usart_init_t* handle, uint16_t* data, uint32_t len);

/**
  * @brief           Starts an rx using dma of a specific length
  *
  * @param handle    The handle for the usart configuration
  * @param data      The address to put the received data, ensure a cast to (uint16_t *), even if 8 bits
  * @param len       Number of units of data, depending on the configured word length
  * @param cont      Enable Continous RX using the idle line interrupt (only need to call this function once, and HAL will keep recieving messages of the same length)
  */
bool PHAL_usartRxDma(usart_init_t* handle, uint16_t* data, uint32_t len, bool cont);

/**
  * @brief Disables the Continous RX that was previously used
  *
  * @param handle The handle for the usart configuration
  */
bool PHAL_disableContinousRxDMA(usart_init_t* handle);

/**
  * @brief Returns whether USART peripheral is currently transmitting data
  *
  * @param handle Handle of USART peripheral to check
  * @return true USART peripheral is currently sending a message
  * @return false USART peripheal is not currently sending a message
  */
bool PHAL_usartTxBusy(usart_init_t* handle);

/**
  * @brief Returns whether USART peripheral is currently receiving data
  *
  * @param handle Handle of USART peripheral check
  * @return true USART peripheral is currently receiving a message
  * @return false USART peripheal is not currently receiving a message
  */
bool PHAL_usartRxBusy(usart_init_t* handle);

/**
  * @brief Callback function called immediately after reception of a USART RX message
  * Uses USART IDLE line interrupt
  *
  * NOTE: this is executed during an interrupt handler call, so keep code in this function light
  *
  * @param handle Handle of USART peripheral that just recieved a message
  */
extern void usart_recieve_complete_callback(usart_init_t* handle);

#ifdef STM32F423xx
// 4,5,7,8 are UART, rest are USART
#define PHAL_USART1_RXDMA_STREAM DMA2_Stream5
#define PHAL_USART1_TXDMA_STREAM DMA2_Stream7

#define PHAL_USART2_RXDMA_STREAM DMA1_Stream5
#define PHAL_USART2_TXDMA_STREAM DMA1_Stream6

#define PHAL_USART3_RXDMA_STREAM DMA1_Stream1
#define PHAL_USART3_TXDMA_STREAM DMA1_Stream3

#define PHAL_USART4_RXDMA_STREAM DMA1_Stream2 // UART
#define PHAL_USART4_TXDMA_STREAM DMA1_Stream4

#define PHAL_USART5_RXDMA_STREAM DMA1_Stream0 // UART
#define PHAL_USART5_TXDMA_STREAM DMA1_Stream7

#define PHAL_USART6_RXDMA_STREAM DMA2_Stream1
#define PHAL_USART6_TXDMA_STREAM DMA2_Stream6

#define PHAL_USART7_RXDMA_STREAM DMA1_Stream3 // UART
#define PHAL_USART7_TXDMA_STREAM DMA1_Stream1

#define PHAL_USART8_RXDMA_STREAM DMA1_Stream6 // UART
#define PHAL_USART8_TXDMA_STREAM DMA1_Stream0
// dm00031020 311
#define _DEF_USART_RXDMA_CONFIG(rx_addr_, priority_, UXART, dmanum, streamnum, channum) \
    { \
        .periph_addr      = (uint32_t)&((UXART)->DR), \
        .mem_addr         = (uint32_t)(rx_addr_), \
        .tx_size          = 1, \
        .increment        = false, \
        .circular         = false, \
        .dir              = 0b0, \
        .mem_inc          = true, \
        .periph_inc       = false, \
        .mem_to_mem       = false, \
        .priority         = (priority_), \
        .mem_size         = 0b00, \
        .periph_size      = 0b00, \
        .tx_isr_en        = true, \
        .dma_chan_request = channum, \
        .stream_idx       = streamnum, \
        .periph           = DMA##dmanum, \
        .stream           = DMA##dmanum##_Stream##streamnum, \
    }

#define _DEF_USART_TXDMA_CONFIG(tx_addr_, priority_, UXART, dmanum, streamnum, channum) \
    { \
        .periph_addr      = (uint32_t)&((UXART)->DR), \
        .mem_addr         = (uint32_t)(tx_addr_), \
        .tx_size          = 1, \
        .increment        = false, \
        .circular         = false, \
        .dir              = 0b1, \
        .mem_inc          = true, \
        .periph_inc       = false, \
        .mem_to_mem       = false, \
        .priority         = (priority_), \
        .mem_size         = 0b00, \
        .periph_size      = 0b00, \
        .tx_isr_en        = true, \
        .dma_chan_request = channum, \
        .stream_idx       = streamnum, \
        .periph           = DMA##dmanum, \
        .stream           = DMA##dmanum##_Stream##streamnum, \
    }

#define USART1_RXDMA_CONT_CONFIG(a, p) _DEF_USART_RXDMA_CONFIG(a, p, USART1, 2, 5, 4)
#define USART1_TXDMA_CONT_CONFIG(a, p) _DEF_USART_TXDMA_CONFIG(a, p, USART1, 2, 7, 4)
#define USART2_RXDMA_CONT_CONFIG(a, p) _DEF_USART_RXDMA_CONFIG(a, p, USART2, 1, 5, 4)
#define USART2_TXDMA_CONT_CONFIG(a, p) _DEF_USART_TXDMA_CONFIG(a, p, USART2, 1, 6, 4)
#define USART3_RXDMA_CONT_CONFIG(a, p) _DEF_USART_RXDMA_CONFIG(a, p, USART3, 1, 1, 4)
#define USART3_TXDMA_CONT_CONFIG(a, p) _DEF_USART_TXDMA_CONFIG(a, p, USART3, 1, 3, 4)
#define USART4_RXDMA_CONT_CONFIG(a, p) _DEF_USART_RXDMA_CONFIG(a, p, UART4, 1, 2, 4)
#define USART4_TXDMA_CONT_CONFIG(a, p) _DEF_USART_TXDMA_CONFIG(a, p, UART4, 1, 4, 4)
#define USART5_RXDMA_CONT_CONFIG(a, p) _DEF_USART_RXDMA_CONFIG(a, p, UART5, 1, 0, 4)
#define USART5_TXDMA_CONT_CONFIG(a, p) _DEF_USART_TXDMA_CONFIG(a, p, UART5, 1, 7, 8)
#define USART6_RXDMA_CONT_CONFIG(a, p) _DEF_USART_RXDMA_CONFIG(a, p, USART6, 2, 1, 5)
#define USART6_TXDMA_CONT_CONFIG(a, p) _DEF_USART_TXDMA_CONFIG(a, p, USART6, 2, 6, 5)

#else

#define USART1_RXDMA_CONT_CONFIG(rx_addr_, priority_) \
    { \
        .periph_addr      = (uint32_t)&(USART1->RDR), \
        .mem_addr         = (uint32_t)(rx_addr_), \
        .tx_size          = 1, \
        .increment        = false, \
        .circular         = false, \
        .dir              = 0b0, \
        .mem_inc          = true, \
        .periph_inc       = false, \
        .mem_to_mem       = false, \
        .priority         = (priority_), \
        .mem_size         = 0b00, \
        .periph_size      = 0b00, \
        .tx_isr_en        = true, \
        .dma_chan_request = 0b0100, \
        .stream_idx       = 5, \
        .periph           = DMA2, \
        .stream           = DMA2_Stream5}

#define USART1_TXDMA_CONT_CONFIG(tx_addr_, priority_) \
    { \
        .periph_addr      = (uint32_t)&(USART1->TDR), \
        .mem_addr         = (uint32_t)(tx_addr_), \
        .tx_size          = 1, \
        .increment        = false, \
        .circular         = false, \
        .dir              = 0b1, \
        .mem_inc          = true, \
        .periph_inc       = false, \
        .mem_to_mem       = false, \
        .priority         = (priority_), \
        .mem_size         = 0b00, \
        .periph_size      = 0b00, \
        .tx_isr_en        = true, \
        .dma_chan_request = 0b0100, \
        .stream_idx       = 7, \
        .periph           = DMA2, \
        .stream           = DMA2_Stream7}

#define USART4_RXDMA_CONT_CONFIG(rx_addr_, priority_) \
    { \
        .periph_addr      = (uint32_t)&(UART4->RDR), \
        .mem_addr         = (uint32_t)(rx_addr_), \
        .tx_size          = 1, \
        .increment        = false, \
        .circular         = false, \
        .dir              = 0b0, \
        .mem_inc          = true, \
        .periph_inc       = false, \
        .mem_to_mem       = false, \
        .priority         = (priority_), \
        .mem_size         = 0b00, \
        .periph_size      = 0b00, \
        .tx_isr_en        = true, \
        .dma_chan_request = 0b0100, \
        .stream_idx       = 2, \
        .periph           = DMA1, \
        .stream           = DMA1_Stream2}

#define USART4_TXDMA_CONT_CONFIG(tx_addr_, priority_) \
    { \
        .periph_addr      = (uint32_t)&(UART4->TDR), \
        .mem_addr         = (uint32_t)(tx_addr_), \
        .tx_size          = 1, \
        .increment        = false, \
        .circular         = false, \
        .dir              = 0b1, \
        .mem_inc          = true, \
        .periph_inc       = false, \
        .mem_to_mem       = false, \
        .priority         = (priority_), \
        .mem_size         = 0b00, \
        .periph_size      = 0b00, \
        .tx_isr_en        = true, \
        .dma_chan_request = 0b0100, \
        .stream_idx       = 4, \
        .periph           = DMA1, \
        .stream           = DMA1_Stream4}

#define UART5_RXDMA_CONT_CONFIG(rx_addr_, priority_) \
    { \
        .periph_addr      = (uint32_t)&(UART5->RDR), \
        .mem_addr         = (uint32_t)(rx_addr_), \
        .tx_size          = 1, \
        .increment        = false, \
        .circular         = false, \
        .dir              = 0b0, \
        .mem_inc          = true, \
        .periph_inc       = false, \
        .mem_to_mem       = false, \
        .priority         = (priority_), \
        .mem_size         = 0b00, \
        .periph_size      = 0b00, \
        .tx_isr_en        = true, \
        .dma_chan_request = 0b0100, \
        .stream_idx       = 0, \
        .periph           = DMA1, \
        .stream           = DMA1_Stream0}
#define UART5_TXDMA_CONT_CONFIG(tx_addr_, priority_) \
    { \
        .periph_addr      = (uint32_t)&(UART5->TDR), \
        .mem_addr         = (uint32_t)(tx_addr_), \
        .tx_size          = 1, \
        .increment        = false, \
        .circular         = false, \
        .dir              = 0b1, \
        .mem_inc          = true, \
        .periph_inc       = false, \
        .mem_to_mem       = false, \
        .priority         = (priority_), \
        .mem_size         = 0b00, \
        .periph_size      = 0b00, \
        .tx_isr_en        = true, \
        .dma_chan_request = 0b0100, \
        .stream_idx       = 7, \
        .periph           = DMA1, \
        .stream           = DMA1_Stream7}
#endif
#endif
