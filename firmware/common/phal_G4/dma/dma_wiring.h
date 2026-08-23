/**
 * @file dma_wiring.h
 * @brief G4 DMA Peripheral wiring configuration and definitions
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#ifndef PHAL_G4_DMA_WIRING_H
#define PHAL_G4_DMA_WIRING_H

#include "stm32g474xx.h"

/// Width of each DMA data element
typedef enum {
    DMA_SIZE_8BIT  = 0,
    DMA_SIZE_16BIT = 1,
    DMA_SIZE_32BIT = 2,
} PHAL_DMA_Size_t;

/**
 * @brief Direction of a peripheral <-> memory transfer
 * 
 * Ignored for DMA_MODE_MEM2MEM, which moves data between two memory
 * addresses with no peripheral or DMAMUX request involved.
 */
typedef enum {
    DMA_PERIPH_TO_MEMORY = 0,
    DMA_MEMORY_TO_PERIPH = 1,
} PHAL_DMA_Direction_t;

/**
 * @brief DMAMUX request IDs (RM0440 Table 91)
 * 
 * Not every peripheral is listed yet
 */
typedef enum : uint8_t {
    DMA_REQUEST_ADC1 = 5U,
    DMA_REQUEST_ADC2 = 36U,
    DMA_REQUEST_ADC3 = 37U,
    DMA_REQUEST_ADC4 = 38U,

    DMA_REQUEST_SPI1_RX = 10U,
    DMA_REQUEST_SPI1_TX = 11U,
    DMA_REQUEST_SPI2_RX = 12U,
    DMA_REQUEST_SPI2_TX = 13U,
    DMA_REQUEST_SPI3_RX = 14U,
    DMA_REQUEST_SPI3_TX = 15U,

    DMA_REQUEST_USART1_RX = 24U,
    DMA_REQUEST_USART1_TX = 25U,
    DMA_REQUEST_USART2_RX = 26U,
    DMA_REQUEST_USART2_TX = 27U,
    DMA_REQUEST_USART3_RX = 28U,
    DMA_REQUEST_USART3_TX = 29U,
} PHAL_DMA_Request_t;

/**
 * @brief Fixed hardware wiring for one peripheral + direction's DMA use
 *
 * Define exactly one of these per peripheral + direction combo as a static/global constant.
 * - Ex: one for SPI1 RX, one for SPI1 TX
 * Don't construct inline in a PHAL_DMA_Handle_t usage. The fields are a direct representation
 * about how the MCU is internally connected, not a user choice.
 */
typedef struct {
    DMA_TypeDef *periph;            /*!< DMA1 or DMA2 */
    uint8_t channel_idx;            /*!< Channel number, 1-8 */
    PHAL_DMA_Request_t mux_request; /*!< DMAMUX request ID */
    volatile void *periph_reg;      /*!< Peripheral data register DMA reads/writes */
    union {
        PHAL_DMA_Direction_t dir;   /*!< Ignored for DMA_MODE_MEM2MEM */
    };
    PHAL_DMA_Size_t data_size;      /*!< Used for both the memory and peripheral side */
} PHAL_DMA_Wiring_t;


// USUART ----------------------------------------------------------------------

static const PHAL_DMA_Wiring_t USART1_RX_DMA_WIRING = {
    .periph      = DMA1,
    .channel_idx = 5,
    .mux_request = DMA_REQUEST_USART1_RX,
    .periph_reg  = &USART1->RDR,
    .dir         = DMA_PERIPH_TO_MEMORY,
    .data_size   = DMA_SIZE_8BIT,
};

static const PHAL_DMA_Wiring_t USART1_TX_DMA_WIRING = {
    .periph      = DMA1,
    .channel_idx = 7,
    .mux_request = DMA_REQUEST_USART1_TX,
    .periph_reg  = &USART1->TDR,
    .dir         = DMA_MEMORY_TO_PERIPH,
    .data_size   = DMA_SIZE_8BIT,
};

static const PHAL_DMA_Wiring_t USART2_RX_DMA_WIRING = {
    .periph      = DMA1,
    .channel_idx = 3,
    .mux_request = DMA_REQUEST_USART2_RX,
    .periph_reg  = &USART2->RDR,
    .dir         = DMA_PERIPH_TO_MEMORY,
    .data_size   = DMA_SIZE_8BIT,
};

static const PHAL_DMA_Wiring_t USART2_TX_DMA_WIRING = {
    .periph      = DMA1,
    .channel_idx = 4,
    .mux_request = DMA_REQUEST_USART2_TX,
    .periph_reg  = &USART2->TDR,
    .dir         = DMA_MEMORY_TO_PERIPH,
    .data_size   = DMA_SIZE_8BIT,
};

static const PHAL_DMA_Wiring_t USART3_RX_DMA_WIRING = {
    .periph      = DMA1,
    .channel_idx = 1,
    .mux_request = DMA_REQUEST_USART3_RX,
    .periph_reg  = &USART3->RDR,
    .dir         = DMA_PERIPH_TO_MEMORY,
    .data_size   = DMA_SIZE_8BIT,
};

static const PHAL_DMA_Wiring_t USART3_TX_DMA_WIRING = {
    .periph      = DMA1,
    .channel_idx = 2,
    .mux_request = DMA_REQUEST_USART3_TX,
    .periph_reg  = &USART3->TDR,
    .dir         = DMA_MEMORY_TO_PERIPH,
    .data_size   = DMA_SIZE_8BIT,
};


// SPI -------------------------------------------------------------------------

static const PHAL_DMA_Wiring_t SPI1_RX_DMA_WIRING = {
    .periph      = DMA1,
    .channel_idx = 2,
    .mux_request = DMA_REQUEST_SPI1_RX,
    .periph_reg  = &SPI1->DR,
    .dir         = DMA_PERIPH_TO_MEMORY,
    .data_size   = DMA_SIZE_8BIT,
};

static const PHAL_DMA_Wiring_t SPI1_TX_DMA_WIRING = {
    .periph      = DMA1,
    .channel_idx = 3,
    .mux_request = DMA_REQUEST_SPI1_TX,
    .periph_reg  = &SPI1->DR,
    .dir         = DMA_MEMORY_TO_PERIPH,
    .data_size   = DMA_SIZE_8BIT,
};

static const PHAL_DMA_Wiring_t SPI2_RX_DMA_WIRING = {
    .periph      = DMA1,
    .channel_idx = 4,
    .mux_request = DMA_REQUEST_SPI2_RX,
    .periph_reg  = &SPI2->DR,
    .dir         = DMA_PERIPH_TO_MEMORY,
    .data_size   = DMA_SIZE_8BIT,
};

static const PHAL_DMA_Wiring_t SPI2_TX_DMA_WIRING = {
    .periph      = DMA1,
    .channel_idx = 5,
    .mux_request = DMA_REQUEST_SPI2_TX,
    .periph_reg  = &SPI2->DR,
    .dir         = DMA_MEMORY_TO_PERIPH,
    .data_size   = DMA_SIZE_8BIT,
};

static const PHAL_DMA_Wiring_t SPI3_RX_DMA_WIRING = {
    .periph      = DMA2,
    .channel_idx = 2,
    .mux_request = DMA_REQUEST_SPI3_RX,
    .periph_reg  = &SPI3->DR,
    .dir         = DMA_PERIPH_TO_MEMORY,
    .data_size   = DMA_SIZE_8BIT,
};

static const PHAL_DMA_Wiring_t SPI3_TX_DMA_WIRING = {
    .periph      = DMA2,
    .channel_idx = 3,
    .mux_request = DMA_REQUEST_SPI3_TX,
    .periph_reg  = &SPI3->DR,
    .dir         = DMA_MEMORY_TO_PERIPH,
    .data_size   = DMA_SIZE_8BIT,
};


// ADC -------------------------------------------------------------------------

static const PHAL_DMA_Wiring_t ADC1_DMA_WIRING = {
    .periph      = DMA1,
    .channel_idx = 1,
    .mux_request = DMA_REQUEST_ADC1,
    .periph_reg  = &ADC1->DR,
    .dir         = DMA_PERIPH_TO_MEMORY,
    .data_size   = DMA_SIZE_16BIT,
};

static const PHAL_DMA_Wiring_t ADC2_DMA_WIRING = {
    .periph      = DMA2,
    .channel_idx = 1,
    .mux_request = DMA_REQUEST_ADC2,
    .periph_reg  = &ADC2->DR,
    .dir         = DMA_PERIPH_TO_MEMORY,
    .data_size   = DMA_SIZE_16BIT,
};

static const PHAL_DMA_Wiring_t ADC3_DMA_WIRING = {
    .periph      = DMA2,
    .channel_idx = 2,
    .mux_request = DMA_REQUEST_ADC3,
    .periph_reg  = &ADC3->DR,
    .dir         = DMA_PERIPH_TO_MEMORY,
    .data_size   = DMA_SIZE_16BIT,
};

static const PHAL_DMA_Wiring_t ADC4_DMA_WIRING = {
    .periph      = DMA2,
    .channel_idx = 3,
    .mux_request = DMA_REQUEST_ADC4,
    .periph_reg  = &ADC4->DR,
    .dir         = DMA_PERIPH_TO_MEMORY,
    .data_size   = DMA_SIZE_16BIT,
};


#endif // PHAL_G4_DMA_WIRING_H