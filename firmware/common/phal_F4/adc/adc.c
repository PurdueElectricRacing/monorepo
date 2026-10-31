/**
 * @file adc.c
 * @author Aditya Anand, Chris McGalliard - port of L4 HAL by Luke Oxley (lcoxley@purdue.edu)
 * @brief
 * @version 0.1
 * @date 2023-09-17
 */

#include "common/phal_F4/adc/adc.h"

bool PHAL_initADC(ADCInitConfig_t* config, ADCChannelConfig_t channels[], uint8_t num_channels) {
    // F423 provides only ADC1; other ADC clock bits are reserved.
    if (config == nullptr) {
        return false;
    }
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR; // Clock-enable settling (ES0372 section 2.2.7).

    // Set prescaler (todo maintain acceptable bounds)
    ADC1_COMMON->CCR &= ~(ADC_CCR_ADCPRE_Msk);
    ADC1_COMMON->CCR |= (config->clock_prescaler << ADC_CCR_ADCPRE_Pos) & ADC_CCR_ADCPRE_Msk;
    ADC1_COMMON->CCR |= ADC_CCR_TSVREFE; // Enable internal temperature sensor

    // Set conversion mode on regular channels
    ADC1->CR2 &= ~(ADC_CR2_CONT);
    ADC1->CR1 &= ~(ADC_CR1_DISCEN);
    config->cont_conv_mode ? (ADC1->CR2 |= (ADC_CR2_CONT)) : (ADC1->CR1 |= (ADC_CR1_DISCEN));

    // Set resolution
    ADC1->CR1 &= ~(ADC_CR1_RES);
    ADC1->CR1 |= (config->resolution << ADC_CR1_RES_Pos) & ADC_CR1_RES_Msk;

    // Set data alignment
    ADC1->CR2 &= ~(ADC_CR2_ALIGN);
    ADC1->CR2 |= (config->data_align << ADC_CR2_ALIGN_Pos) & ADC_CR2_ALIGN_Msk;

    // Regular channel sequence length
    ADC1->SQR1 &= ~(ADC_SQR1_L);
    ADC1->SQR1 |= ((num_channels - 1) << ADC_SQR1_L_Pos) & ADC_SQR1_L_Msk;

    // DMA configuration
    if (config->dma_mode != ADC_DMA_OFF) {
        // Circular or one shot
        ADC1->CR2 |= (ADC_CR2_DMA) | (((config->dma_mode == ADC_DMA_CIRCULAR) << ADC_CR2_DDS_Pos) & ADC_CR2_DDS_Msk);
    } else {
        // Disable ADC DMA Mode
        ADC1->CR2 &= ~(ADC_CR2_DMA);
    }

    // Channel configuration
    for (int i = 0; i < num_channels; i++) {
        // Configure sample time: https://controllerstech.com/adc-conversion-time-frequency-calculation-in-stm32/
        if (channels[i].channel < 10) {
            ADC1->SMPR2 &= ~(ADC_SMPR2_SMP0_Msk << (ADC_SMPR2_SMP1_Pos * channels[i].channel));
            ADC1->SMPR2 |= (channels[i].sampling_time & ADC_SMPR2_SMP0_Msk) << (ADC_SMPR2_SMP1_Pos * channels[i].channel);
        } else if (channels[i].channel > 9 && channels[i].channel < 19) {
            ADC1->SMPR1 &= ~(ADC_SMPR1_SMP10_Msk << (ADC_SMPR1_SMP11_Pos * (channels[i].channel - 10)));
            ADC1->SMPR1 |= (channels[i].sampling_time & ADC_SMPR1_SMP10_Msk) << (ADC_SMPR1_SMP11_Pos * (channels[i].channel - 10));
        }

        // Sequence rank
        if (channels[i].rank < 7) {
            ADC1->SQR3 &= ~(ADC_SQR3_SQ1_Msk << ((channels[i].rank - 1) * 5));
            ADC1->SQR3 |= (channels[i].channel & ADC_SQR3_SQ1_Msk) << ((channels[i].rank - 1) * 5);
        } else if (channels[i].rank < 13) {
            ADC1->SQR2 &= ~(ADC_SQR2_SQ7_Msk << ((channels[i].rank - 7) * 5));
            ADC1->SQR2 |= (channels[i].channel & ADC_SQR2_SQ7_Msk) << ((channels[i].rank - 7) * 5);
        } else if (channels[i].rank < 17) {
            ADC1->SQR1 &= ~(ADC_SQR1_SQ13_Msk << ((channels[i].rank - 13) * 5));
            ADC1->SQR1 |= (channels[i].channel & ADC_SQR1_SQ13_Msk) << ((channels[i].rank - 13) * 5);
        }
    }
    // Enable scan mode to read multiple channels
    if (num_channels > 1) {
        ADC1->CR1 |= ADC_CR1_SCAN;
    }

    // Wake up from power down if necessary
    if (!(ADC1->CR2 & ADC_CR2_ADON_Msk)) {
        ADC1->CR2 |= (ADC_CR2_ADON);
    }

    return true;
}

bool PHAL_startADC(void) {
    ADC1->CR2 |= ADC_CR2_SWSTART;
    return true;
}

bool PHAL_stopADC(void) {
    ADC1->CR2 &= ~(ADC_CR2_SWSTART);
    return true;
}

uint16_t PHAL_readADC(void) {
    return (uint16_t)(ADC1->DR & ADC_DR_DATA_Msk);
}
