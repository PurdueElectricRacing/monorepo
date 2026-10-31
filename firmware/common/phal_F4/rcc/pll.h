#ifndef PHAL_F4_PLL_H
#define PHAL_F4_PLL_H

#include <stdbool.h>
#include <stdint.h>

/* STM32F423 main PLL limits, RM0430 Rev 9, section 6.3.2. */
#define RCC_MAX_VCO_RATE_HZ           (432000000U)
#define RCC_MIN_VCO_RATE_HZ           (100000000U)
#define RCC_MIN_PLL_INPUT_DIVISOR     (2U)
#define RCC_MAX_PLL_INPUT_DIVISOR     (63U)
#define RCC_MIN_PLL_OUTPUT_MULTIPLIER (50U)
#define RCC_MAX_PLL_OUTPUT_MULTIPLIER (432U)

typedef struct {
    uint32_t m;
    uint32_t n;
} PHAL_PLLFactors_t;

/* Find exact M/N without touching hardware. Prefer the smallest M (highest
 * valid VCO input frequency). On failure, leave factors unchanged. */
bool PHAL_findPLLFactors(uint32_t input_hz, uint32_t vco_hz, PHAL_PLLFactors_t* factors);

#endif
