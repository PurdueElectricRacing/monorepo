/**
 * @file adc.c
 * @brief G4 ADC driver: pin-to-channel mapping, ADC assignment, DMA setup.
 * @author Daniel Proano (dproano@purdue.edu) && Ronak Jain (jain717@purdue.edu)
 */

#include "common/phal_G4/adc/adc.h"

#include "common/phal_G4/adc/adc_priv.h"
#include "common/phal_G4/dma/dma.h"
#include "common/phal_G4/gpio/gpio.h"

typedef enum : uint8_t {
    ADC1_IDX,
    ADC2_IDX,
    ADC3_IDX,
    ADC4_IDX,
    ADC_COUNT,
    ADC_NONE = UINT8_MAX,
} adc_idx_t;

static constexpr uint8_t ADC_NOT_WIRED = 0U;

typedef struct {
    GPIO_TypeDef *bank;
    uint8_t pin;
    uint8_t channel_on[ADC_COUNT];
} adc_pin_t;

typedef struct {
    bool configured;
    adc_idx_t adc;
    uint8_t rank;
} adc_pin_state_t;

#define ADC_PIN(gpio_bank, pin_num, ...)                                       \
    {                                                                          \
        .bank = gpio_bank,                                                     \
        .pin = pin_num,                                                        \
        .channel_on = {__VA_ARGS__}                                            \
    }

// References pg. 55 in DS12288 for the ADC mappings
// References pg. 50 for the pinout to known what pins are available
static const adc_pin_t ADC_PINS[] = {
    ADC_PIN(GPIOA, 0,  [ADC1_IDX] = 1,  [ADC2_IDX] = 1),
    ADC_PIN(GPIOA, 1,  [ADC1_IDX] = 2,  [ADC2_IDX] = 2),
    ADC_PIN(GPIOA, 2,  [ADC1_IDX] = 3),
    ADC_PIN(GPIOA, 3,  [ADC1_IDX] = 4),
    ADC_PIN(GPIOA, 4,  [ADC2_IDX] = 17),
    ADC_PIN(GPIOA, 5,  [ADC2_IDX] = 13),
    ADC_PIN(GPIOA, 6,  [ADC2_IDX] = 3),
    ADC_PIN(GPIOA, 7,  [ADC2_IDX] = 4),
    ADC_PIN(GPIOB, 0,  [ADC1_IDX] = 15, [ADC3_IDX] = 12),
    ADC_PIN(GPIOB, 1,  [ADC1_IDX] = 12, [ADC3_IDX] = 1),
    ADC_PIN(GPIOB, 2,  [ADC2_IDX] = 12),
    ADC_PIN(GPIOB, 11, [ADC1_IDX] = 14, [ADC2_IDX] = 14),
    ADC_PIN(GPIOB, 12, [ADC1_IDX] = 11, [ADC4_IDX] = 3),
    ADC_PIN(GPIOB, 13, [ADC3_IDX] = 5),
    ADC_PIN(GPIOB, 14, [ADC1_IDX] = 5,  [ADC4_IDX] = 4),
    ADC_PIN(GPIOB, 15, [ADC2_IDX] = 15, [ADC4_IDX] = 5),
    ADC_PIN(GPIOC, 0,  [ADC1_IDX] = 6,  [ADC2_IDX] = 6),
    ADC_PIN(GPIOC, 1,  [ADC1_IDX] = 7,  [ADC2_IDX] = 7),
    ADC_PIN(GPIOC, 2,  [ADC1_IDX] = 8,  [ADC2_IDX] = 8),
    ADC_PIN(GPIOC, 3,  [ADC1_IDX] = 9,  [ADC2_IDX] = 9),
    ADC_PIN(GPIOC, 4,  [ADC2_IDX] = 5),
    ADC_PIN(GPIOC, 5,  [ADC2_IDX] = 11),
    ADC_PIN(GPIOF, 0,  [ADC1_IDX] = 10),
    ADC_PIN(GPIOF, 1,  [ADC2_IDX] = 10),
};
static constexpr size_t ADC_PIN_COUNT = sizeof(ADC_PINS) / sizeof(ADC_PINS[0]);

static ADC_TypeDef *const ADC_INSTANCES[ADC_COUNT] = {ADC1, ADC2, ADC3, ADC4};

static const PHAL_DMA_Wiring_t *const ADC_DMA_WIRINGS[ADC_COUNT] = {
    &ADC1_DMA_WIRING,
    &ADC2_DMA_WIRING,
    &ADC3_DMA_WIRING,
    &ADC4_DMA_WIRING,
};

// global ADC conversion readings
static volatile uint16_t g_samples[ADC_COUNT][PHAL_ADC_MAX_CHANNEL_COUNT];

// global dma buffer, one for each ADC to move readings around
static PHAL_DMA_Handle_t g_dma[ADC_COUNT];

// global state to track whether if pins are used and where its reading is stored
static adc_pin_state_t g_pin_states[ADC_PIN_COUNT];

// indicate if initialization has already occured or not,
// trying to re-try init could result in lost sampling data
static bool g_initialized;

static size_t adc_find_pin_idx(const GPIO_TypeDef *bank, uint8_t pin) {
    for (size_t i = 0U; i < ADC_PIN_COUNT; i++) {
        if (ADC_PINS[i].bank == bank && ADC_PINS[i].pin == pin) {
            return i;
        }
    }
    return ADC_PIN_COUNT;
}

static adc_idx_t adc_index_of(const ADC_TypeDef *instance) {
    for (adc_idx_t adc = ADC1_IDX; adc < ADC_COUNT; adc++) {
        if (ADC_INSTANCES[adc] == instance) {
            return adc;
        }
    }
    return ADC_NONE;
}

static bool adc_is_wired(const adc_pin_t *pin, adc_idx_t adc) {
    return pin->channel_on[adc] != ADC_NOT_WIRED;
}

static uint8_t adc_wired_count(const adc_pin_t *pin) {
    uint8_t count = 0U;
    for (adc_idx_t adc = ADC1_IDX; adc < ADC_COUNT; adc++) {
        if (adc_is_wired(pin, adc)) {
            count++;
        }
    }
    return count;
}

static adc_idx_t adc_least_loaded(const adc_pin_t *pin, const uint8_t load[]) {
    adc_idx_t best = ADC_NONE;
    for (adc_idx_t adc = ADC1_IDX; adc < ADC_COUNT; adc++) {
        if (!adc_is_wired(pin, adc)) {
            continue;
        }
        if (best == ADC_NONE || load[adc] < load[best]) {
            best = adc;
        }
    }
    return best;
}

static bool adc_validate_pins(const PHAL_ADC_InitConfig_t config[], size_t count) {
    bool seen[ADC_PIN_COUNT] = {0};
    for (size_t i = 0U; i < count; i++) {
        size_t pin_idx = adc_find_pin_idx(config[i].bank, config[i].pin);
        if (pin_idx == ADC_PIN_COUNT || seen[pin_idx]) {
            return false;
        }
        seen[pin_idx]         = true;
        g_pin_states[pin_idx] = (adc_pin_state_t){.adc = ADC_NONE};
    }
    return true;
}

static bool adc_assign_fixed_pins(
    const PHAL_ADC_InitConfig_t config[],
    size_t count,
    uint8_t load[]
) {
    for (size_t i = 0U; i < count; i++) {
        size_t pin_idx         = adc_find_pin_idx(config[i].bank, config[i].pin);
        const adc_pin_t *pin   = &ADC_PINS[pin_idx];
        adc_pin_state_t *state = &g_pin_states[pin_idx];

        if (config[i].instance != nullptr) {
            adc_idx_t forced = adc_index_of(config[i].instance);
            if (forced == ADC_NONE || !adc_is_wired(pin, forced)) {
                return false;
            }
            state->adc = forced;
        } else if (adc_wired_count(pin) == 1U) {
            state->adc = adc_least_loaded(pin, load);
        } else {
            continue;
        }
        load[state->adc]++;
    }
    return true;
}

static void adc_assign_flexible_pins(
    const PHAL_ADC_InitConfig_t config[],
    size_t count,
    uint8_t load[]
) {
    for (size_t i = 0U; i < count; i++) {
        size_t pin_idx         = adc_find_pin_idx(config[i].bank, config[i].pin);
        adc_pin_state_t *state = &g_pin_states[pin_idx];
        if (state->adc != ADC_NONE) {
            continue;
        }
        state->adc = adc_least_loaded(&ADC_PINS[pin_idx], load);
        load[state->adc]++;
    }
}

static bool adc_set_pins_analog(const PHAL_ADC_InitConfig_t config[], size_t count) {
    for (size_t i = 0U; i < count; i++) {
        PHAL_GPIO_InitConfig_t gpio = PHAL_GPIO_INIT_ANALOG(config[i].bank, config[i].pin);
        if (!PHAL_GPIO_init(&gpio, 1U)) {
            return false;
        }
    }
    return true;
}

static uint8_t adc_build_sequence(
    adc_idx_t adc,
    const PHAL_ADC_InitConfig_t config[],
    size_t count,
    uint8_t channels[]
) {
    uint8_t channel_count = 0U;
    for (size_t i = 0U; i < count; i++) {
        size_t pin_idx         = adc_find_pin_idx(config[i].bank, config[i].pin);
        adc_pin_state_t *state = &g_pin_states[pin_idx];
        if (state->adc != adc) {
            continue;
        }
        state->rank             = channel_count;
        channels[channel_count] = ADC_PINS[pin_idx].channel_on[adc];
        channel_count++;
    }
    return channel_count;
}

static bool adc_start(adc_idx_t adc, const uint8_t channels[], uint8_t channel_count) {
    ADC_TypeDef *instance = ADC_INSTANCES[adc];
    if (!ADC_PRIV_configure(instance, channels, channel_count)) {
        return false;
    }
    if (!ADC_PRIV_enable(instance)) {
        ADC_PRIV_disable(instance);
        return false;
    }

    PHAL_DMA_Handle_t *dma = &g_dma[adc];
    dma->wiring = ADC_DMA_WIRINGS[adc];
    dma->params = (PHAL_DMA_Params_t){
        .mem_addr  = (uint32_t)&g_samples[adc][0],
        .tx_size   = channel_count,
        .priority  = DMA_PRIORITY_HIGH,
        .mode      = DMA_MODE_CIRCULAR,
        .mem_inc   = true,
        .tx_isr_en = false,
    };
    if (!PHAL_DMA_init(dma)) {
        ADC_PRIV_disable(instance);
        return false;
    }

    PHAL_DMA_start(dma);
    ADC_PRIV_prepare_transfer(instance, true);
    ADC_PRIV_start_conversion(instance);
    return true;
}

static void adc_stop(adc_idx_t adc) {
    ADC_PRIV_disable(ADC_INSTANCES[adc]);
    PHAL_DMA_deinit(&g_dma[adc]);
}

static bool adc_start_all(const PHAL_ADC_InitConfig_t config[], size_t count) {
    bool started[ADC_COUNT] = {false};

    for (adc_idx_t adc = ADC1_IDX; adc < ADC_COUNT; adc++) {
        uint8_t channels[PHAL_ADC_MAX_CHANNEL_COUNT];
        uint8_t channel_count = adc_build_sequence(adc, config, count, channels);
        if (channel_count == 0U) {
            continue;
        }

        if (!adc_start(adc, channels, channel_count)) {
            for (adc_idx_t prev = ADC1_IDX; prev < adc; prev++) {
                if (started[prev]) {
                    adc_stop(prev);
                }
            }
            return false;
        }
        started[adc] = true;
    }
    return true;
}

bool PHAL_ADC_init(const PHAL_ADC_InitConfig_t config[], size_t config_len) {
    if (g_initialized || config == nullptr || config_len == 0U) {
        return false;
    }

    uint8_t load[ADC_COUNT] = {0};

    // Ensure pins exist and setup globals
    if (!adc_validate_pins(config, config_len)) {
        return false;
    }
    
    // Assign pins with only one possible ADC
    if (!adc_assign_fixed_pins(config, config_len, load)) {
        return false;
    }

    adc_assign_flexible_pins(config, config_len, load);

    if (!adc_set_pins_analog(config, config_len)) {
        return false;
    }

    // Configure each ADC, enable them, and arms DMA for beginning of conversions
    if (!adc_start_all(config, config_len)) {
        return false;
    }

    // Mark each ADC that was configured as such, allowing a ADC_read
    for (size_t i = 0U; i < config_len; i++) {
        g_pin_states[adc_find_pin_idx(config[i].bank, config[i].pin)].configured = true;
    }
    g_initialized = true;
    return true;
}

uint16_t PHAL_ADC_read(const GPIO_TypeDef *bank, uint8_t pin) {
    size_t pin_idx = adc_find_pin_idx(bank, pin);
    if (pin_idx == ADC_PIN_COUNT || !g_pin_states[pin_idx].configured) {
        return PHAL_ADC_INVALID_READING;
    }

    const adc_pin_state_t *state = &g_pin_states[pin_idx];
    return g_samples[state->adc][state->rank];
}
