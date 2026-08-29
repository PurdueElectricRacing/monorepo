
#ifndef __G4_TESTING_MAIN_H__
#define __G4_TESTING_MAIN_H__

#include <stdint.h>

// G4 devboard LEDs

#define LED_GREEN_PORT  GPIOB
#define LED_GREEN_PIN   7
#define LED_RED_PORT    GPIOB
#define LED_RED_PIN     5
#define LED_BLUE_PORT   GPIOA
#define LED_BLUE_PIN    15
#define LED_ORANGE_PORT GPIOB
#define LED_ORANGE_PIN  1

#define LED_NUCLEO_GREEN_PORT GPIOA
#define LED_NUCLEO_GREEN_PIN  5

typedef struct __attribute__((packed)) {
    // Do not modify this struct
    // unless you modify the ADC DMA config in main.h to match
    uint16_t val1;
    uint16_t val2;
    uint16_t val3;
    uint16_t val4;
} raw_adc_values_t;

volatile extern raw_adc_values_t raw_adc_values;
#define ADC_NUM_CHANNELS (sizeof(raw_adc_values) / sizeof(uint16_t))

#endif // __G4_TESTING_MAIN_H__
