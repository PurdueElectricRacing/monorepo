/**
 * @file adc.h
 * @author Chris McGalliard - port of L4 HAL by Luke Oxley (lcoxley@purdue.edu)
 * @brief
 * @version 0.1
 * @date 2023-09-17
 */

#ifndef _PHAL_ADC_H
#define _PHAL_ADC_H

#include "common/phal_F4/phal_F4.h"

typedef enum {
    ADC_RES_12_BIT = 0b00,
    ADC_RES_10_BIT = 0b01,
    ADC_RES_8_BIT  = 0b10,
    ADC_RES_6_BIT  = 0b11
} ADCResolution_t;

typedef enum {
    ADC_CLK_PRESC_2 = 0b00,
    ADC_CLK_PRESC_4 = 0b01,
    ADC_CLK_PRESC_6 = 0b10,
    ADC_CLK_PRESC_8 = 0b11,
} ADCClkPrescaler_t;

typedef enum {
    ADC_DMA_OFF      = 0b00,
    ADC_DMA_ONE_SHOT = 0b01,
    ADC_DMA_CIRCULAR = 0b11
} ADCDMAMode_t;

typedef enum {
    ADC_DATA_ALIGN_RIGHT = 0b0,
    ADC_DATA_ALIGN_LEFT  = 0b1
} ADCDataAlign_t;

typedef struct {
    ADCClkPrescaler_t clock_prescaler;
    ADCResolution_t resolution;
    ADCDataAlign_t data_align;
    bool cont_conv_mode;
    ADCDMAMode_t dma_mode;
} ADCInitConfig_t;

typedef enum {
    ADC_CHN_SMP_CYCLES_3   = 0b000,
    ADC_CHN_SMP_CYCLES_15  = 0b001,
    ADC_CHN_SMP_CYCLES_28  = 0b010,
    ADC_CHN_SMP_CYCLES_56  = 0b011,
    ADC_CHN_SMP_CYCLES_84  = 0b100,
    ADC_CHN_SMP_CYCLES_112 = 0b101,
    ADC_CHN_SMP_CYCLES_144 = 0b110,
    ADC_CHN_SMP_CYCLES_480 = 0b111,
} ADCChannelSampleCycles_t;

typedef struct {
    uint32_t channel; // not the GPIO channel, use the ADC channel
    uint32_t rank; // order at which the channels will be polled, starting at 1
    ADCChannelSampleCycles_t sampling_time;
} ADCChannelConfig_t;

// TODO DMA CONFIGS FOR ADC
#define ADC1_DMA_CONT_CONFIG(mem_addr_, tx_size_, priority_) \
    {.periph_addr = (uint32_t)&(ADC1->DR), .mem_addr = mem_addr_, .tx_size = tx_size_, .increment = true, .circular = true, .dir = 0b0, .mem_inc = true, .periph_inc = false, .mem_to_mem = false, .priority = priority_, .mem_size = 0b01, .periph_size = 0b01, .tx_isr_en = false, .dma_chan_request = 0b0000, .stream_idx = 0, .periph = DMA2, .stream = DMA2_Stream0}

/**
 * @brief Initializes ADC1, requires GPIO config prior
 *
 * @param config ADC initial config settings
 * @param channels List of channel configurations
 * @param num_channels Number of channels in the channel configuration list
 * @return false if config is null.
**/
bool PHAL_initADC(ADCInitConfig_t* config, ADCChannelConfig_t channels[], uint8_t num_channels);

/**
 * @brief Starts ADC1 conversions, requires PHAL_initADC to be called prior
 *
**/
bool PHAL_startADC(void);
/**
 * @brief Stops ADC1 conversions, requires PHAL_initADC to be called prior
 *
**/
bool PHAL_stopADC(void);

/**
 * @brief Reads the ADC1 data register
 *
 * @return contents of the data register
**/
uint16_t PHAL_readADC(void);

#endif
