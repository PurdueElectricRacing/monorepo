#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_GPIO)

#include <stdbool.h>
#include <stdint.h>

#include "common/rtos/rtos.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/utils/countof.h"
#include "main.h"

/**
 * PC2 (out) -> PC3 (in)
 * PC4 (out) -> PC5 (in)
 * PC6 (out) -> PC7 (in)
 * PC8 (out) -> PC9 (in)
 *
 * Pairs 0, 1: PHAL_GPIO_write with an alternating low/high pattern
 * Pairs 2, 3: PHAL_GPIO_toggle
 */

void HardFault_Handler(void);

static constexpr uint32_t NUM_PAIRS = 4;
static GPIO_TypeDef *const PAIR_BANK          = GPIOC;
static const uint8_t PAIR_OUT_PIN[NUM_PAIRS]  = {2, 4, 6, 8};
static const uint8_t PAIR_IN_PIN[NUM_PAIRS]   = {3, 5, 7, 9};

static PHAL_GPIO_InitConfig_t gpio_config[] = {
	PHAL_GPIO_INIT_OUTPUT(GPIOC, 2, GPIO_OUTPUT_LOW_SPEED),
	PHAL_GPIO_INIT_INPUT(GPIOC, 3, GPIO_INPUT_OPEN_DRAIN),
	PHAL_GPIO_INIT_OUTPUT(GPIOC, 4, GPIO_OUTPUT_MED_SPEED),
	PHAL_GPIO_INIT_INPUT(GPIOC, 5, GPIO_INPUT_OPEN_DRAIN),
	PHAL_GPIO_INIT_OUTPUT(GPIOC, 6, GPIO_OUTPUT_HIGH_SPEED),
	PHAL_GPIO_INIT_INPUT(GPIOC, 7, GPIO_INPUT_OPEN_DRAIN),
	PHAL_GPIO_INIT_OUTPUT(GPIOC, 8, GPIO_OUTPUT_LOW_SPEED),
	PHAL_GPIO_INIT_INPUT(GPIOC, 9, GPIO_INPUT_OPEN_DRAIN),
	PHAL_GPIO_INIT_OUTPUT(LED_NUCLEO_GREEN_PORT, LED_NUCLEO_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
};

// Per-pair state/counters
typedef struct {
    volatile bool expected;            // last value this pair's task drove/expects
    volatile uint32_t iterations;      // total write-and-verify cycles run
    volatile uint32_t failures;        // total mismatches seen
    volatile uint32_t last_fail_iter;  // iteration number of the most recent mismatch
    volatile bool last_fail_expected;  // expected value at the most recent mismatch
    volatile bool last_fail_actual;    // actual value read at the most recent mismatch
} pair_state_t;

static pair_state_t pair_state[NUM_PAIRS];
volatile uint32_t g_total_failures = 0;

static void run_pair_write(uint32_t idx) {
    pair_state_t *st = &pair_state[idx];

    bool next = (st->iterations & 1U) != 0U;

    PHAL_GPIO_write(PAIR_BANK, PAIR_OUT_PIN[idx], next);
    st->expected = next;

    bool actual = PHAL_GPIO_read(PAIR_BANK, PAIR_IN_PIN[idx]);
    st->iterations++;

    if (actual != next) {
        st->failures++;
        st->last_fail_iter     = st->iterations;
        st->last_fail_expected = next;
        st->last_fail_actual   = actual;
    }
}

static void run_pair_toggle(uint32_t idx) {
    pair_state_t *st = &pair_state[idx];

    PHAL_GPIO_toggle(PAIR_BANK, PAIR_OUT_PIN[idx]);
    st->expected = !st->expected;

    bool actual = PHAL_GPIO_read(PAIR_BANK, PAIR_IN_PIN[idx]);
    st->iterations++;

    if (actual != st->expected) {
        st->failures++;
        st->last_fail_iter     = st->iterations;
        st->last_fail_expected = st->expected;
        st->last_fail_actual   = actual;
    }
}

static void writer_pair0(void)  { run_pair_write(0);  }
static void writer_pair1(void)  { run_pair_write(1);  }
static void toggler_pair2(void) { run_pair_toggle(2); }
static void toggler_pair3(void) { run_pair_toggle(3); }

// Aggregates every pair's counters and update the status LEDs
static void monitor_task(void) {
    uint32_t total_failures = 0;
    for (uint32_t i = 0; i < NUM_PAIRS; i++) {
        total_failures += pair_state[i].failures;
    }
    g_total_failures = total_failures;

	PHAL_GPIO_toggle(LED_NUCLEO_RED_PORT, LED_NUCLEO_RED_PIN);
}

RTOS_DEFINE_TASK(writer_pair0,  1,   TASK_PRIORITY_HIGH, STACK_256);
RTOS_DEFINE_TASK(writer_pair1,  1,   TASK_PRIORITY_HIGH, STACK_256);
RTOS_DEFINE_TASK(toggler_pair2, 1,   TASK_PRIORITY_HIGH, STACK_256);
RTOS_DEFINE_TASK(toggler_pair3, 1,   TASK_PRIORITY_HIGH, STACK_256);
RTOS_DEFINE_TASK(monitor_task,  200, TASK_PRIORITY_LOW, STACK_256);

int main() {
    // Fast core clock:
	// more ticks per second = more chances per second for the tasks to overlap on GPIOC
    PHAL_RCC_init(PHAL_RCC_HSI_170MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

	// Start all GPIOs low:
	for (uint32_t i = 0; i < NUM_PAIRS; i++) {
		PHAL_GPIO_write(PAIR_BANK, PAIR_OUT_PIN[i], false);
		pair_state[i].expected = false;
	}

    RTOS_START_TASK(writer_pair0);
    RTOS_START_TASK(writer_pair1);
    RTOS_START_TASK(toggler_pair2);
    RTOS_START_TASK(toggler_pair3);
    RTOS_START_TASK(monitor_task);

    vTaskStartScheduler();

    // Unreachable
    HardFault_Handler();

    return 0;
}

void HardFault_Handler(void) {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_GPIO_LOOPBACK