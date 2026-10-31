#include "pll.h"

bool PHAL_findPLLFactors(uint32_t input_hz, uint32_t vco_hz, PHAL_PLLFactors_t* factors) {
    if (!factors || !input_hz || vco_hz < RCC_MIN_VCO_RATE_HZ || vco_hz > RCC_MAX_VCO_RATE_HZ) {
        return false;
    }

    for (uint32_t m = RCC_MIN_PLL_INPUT_DIVISOR; m <= RCC_MAX_PLL_INPUT_DIVISOR; ++m) {
        /* Compare rational rates directly; input/M need not be integral Hz. */
        if ((uint64_t)input_hz < 1000000ULL * m || (uint64_t)input_hz > 2000000ULL * m) {
            continue;
        }

        const uint64_t numerator = (uint64_t)vco_hz * m;
        if (numerator % input_hz) {
            continue;
        }

        const uint64_t n = numerator / input_hz;
        if (n < RCC_MIN_PLL_OUTPUT_MULTIPLIER || n > RCC_MAX_PLL_OUTPUT_MULTIPLIER) {
            continue;
        }

        factors->m = m;
        factors->n = (uint32_t)n;
        return true;
    }
    return false;
}
