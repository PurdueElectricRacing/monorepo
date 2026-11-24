/**
 * @file dma.h
 * @author Chris McGalliard - Port of L4 HAL by Dawson Moore (moore800@purdue.edu)
 * @brief
 * @version 0.1
 * @date 2023-08-19
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef _DMA_H_
#define _DMA_H_

#include "phal_F4/phal_F4.h"

typedef struct {
    uint32_t periph_addr;
    uint32_t mem_addr;
    uint16_t tx_size;
    uint8_t mem_size;

    bool increment;
    bool circular;
    uint8_t dir;
    bool mem_inc;
    bool periph_inc;
    bool mem_to_mem;
    uint8_t priority;
    uint8_t periph_size;
    bool tx_isr_en;
    uint8_t dma_chan_request;
    uint8_t stream_idx;

    DMA_TypeDef* periph;
    DMA_Stream_TypeDef* stream; // Example DMA1_Stream0 or DMA2_Stream7
} dma_init_t;

/*
 * @brief Initialize DMA peripheral to set m2m, p2p, or p2m with set size
 *        and length of txfer
 *
 * @param init -> Address of initialization structure
 * @return true -> Successful init (no clashing params)
 * @return false -> Init not complete (parameters clash)
 */
bool PHAL_initDMA(dma_init_t* init);

/*
 * @brief Start txfer after sucessful DMA peripheral initialization
 *
 * @param init -> Address of initialization structure
 */
void PHAL_DMA_startTxfer(dma_init_t* init);

/*
 * @brief Stop txfer
 *
 * @param init -> Address of initialization structure
 */
void PHAL_DMA_stopTxfer(dma_init_t* init);

/*
 * @brief Re-enable DMA txfer after error ISR fires
 *
 * @param init -> Address of initialization structure
 */
void PHAL_DMA_reEnable(dma_init_t* init);

/*
 * @brief Set memory address for DMA transfer. In Mem to Mem this acts as the source address
 *
 * @param init -> Address of initialization structure
 */
void PHAL_DMA_setMemAddress(dma_init_t* init, const uint32_t address);

/*
 * @brief Set transfer length for DMA transaction
 *
 * @param init -> Address of initialization structure
 */
void PHAL_DMA_setTxferLength(dma_init_t* init, const uint32_t length);

#endif