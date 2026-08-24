#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_USART)

#include <stdint.h>
#include <string.h>

#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/usart/usart.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/utils/countof.h"
#include "main.h"

/**
 * @file usart_test.c
 * @brief Self-checking bench test for the USART HAL.
 *
 * Please plug in each of the following TX pins into
 * their corresponding RX pins:
 *   USART1: PA9  (TX) -> PA10 (RX)
 *   USART3: PC10 (TX) -> PC11 (RX)
 */

void HardFault_Handler(void);

PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_RED_PORT, LED_RED_PIN, GPIO_OUTPUT_LOW_SPEED),

    PHAL_PIN_DEFS_USART1_TX_PA9,
    PHAL_PIN_DEFS_USART1_RX_PA10,

    // Skip USART2, conflicts with Nucleo

    PHAL_PIN_DEFS_USART3_TX_PC10,
    PHAL_PIN_DEFS_USART3_RX_PC11,
};

// USART1 on APB2; USART2/3 on APB1
static constexpr uint32_t USART1_TEST_BAUD = 115200u;
static constexpr uint32_t USART3_TEST_BAUD = 9600u;

static constexpr uint16_t FRAME_LEN = 16;
static uint8_t tx_buf[FRAME_LEN];
static uint8_t rx_buf[FRAME_LEN];
static volatile uint32_t rx_frame_success_count;
static volatile uint16_t rx_frame_len; //!< length reported by the last rxCallback

// Every subtest below except the per-peripheral roundtrips runs on USART3
static constexpr PHAL_USART_Idx_t TEST_PERIPH = USART3_IDX;

typedef enum {
    SUBTEST_USART1_ROUNDTRIP,
    SUBTEST_USART3_ROUNDTRIP,
    SUBTEST_TXBL,
    SUBTEST_RXBL,
    SUBTEST_TXBUSY,
    SUBTEST_ONESHOT,
    SUBTEST_CONTINUOUS,
    NUM_SUBTESTS
} usart_subtest_id_t;

volatile uint32_t usart_failed_subtest = NUM_SUBTESTS;
volatile uint32_t usart_failed_detail;
volatile uint32_t usart_failed_expected;
volatile uint32_t usart_failed_actual;

static constexpr uint32_t TIMEOUT_MARKER = 0xFFFFFFFFu;
// Distinguishes "wrong frame length" from a byte-index mismatch in usart_failed_detail.
static constexpr uint32_t LENGTH_MARKER = 0xFFFFFFFEu;

// These spin loops are raw instruction counts, not a hardware timer, so their
// real-world duration is inversely proportional to core clock. Scale the
// iteration count from the actual AHB clock (read at startup) instead of a
// fixed constant, so the timeout doesn't silently shrink if PHAL_RCC_init is
// ever switched to a different clock preset (e.g. PHAL_RCC_HSI_170MHZ).
static constexpr uint32_t TIMEOUT_TARGET_MS = 200u;
static constexpr uint32_t WAIT_LOOP_CYCLES_PER_ITER = 4u; // deliberately low estimate - biases toward waiting longer, never shorter

static uint32_t usart_test_timeout_iters;

static uint32_t compute_timeout_iters(uint32_t ahb_clock_hz) {
    return (ahb_clock_hz / WAIT_LOOP_CYCLES_PER_ITER) * TIMEOUT_TARGET_MS / 1000u;
}

// Spin until a new frame lands or the clock-scaled timeout elapses (~200ms,
// comfortably longer than one FRAME_LEN frame at the slowest baud rate used
// here (9600)) so a genuine HAL bug times out instead of hanging the board.
static bool wait_for_frame(uint32_t before) {
    uint32_t i = usart_test_timeout_iters;
    while (rx_frame_success_count == before && --i) {
        __NOP();
    }
    return i != 0;
}

// Same idea, for the one case that isn't waiting on rx_frame_success_count.
static bool wait_for_tx_free(PHAL_USART_Idx_t periph) {
    uint32_t i = usart_test_timeout_iters;
    while (PHAL_USART_txBusy(periph) && --i) {
        __NOP();
    }
    return i != 0;
}

static void delay_iters(uint32_t n) {
    while (n--) {
        __NOP();
    }
}

static void fill_pattern(uint8_t *buf, uint32_t len, uint8_t seed) {
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(seed + i);
    }
}

// tx_buf gets a fresh known pattern, rx_buf is zeroed so a leftover byte from
// a prior subtest can't masquerade as a successful receive.
static void prep_frame(uint8_t seed) {
    fill_pattern(tx_buf, FRAME_LEN, seed);
    memset(rx_buf, 0, FRAME_LEN);
    rx_frame_len = 0;
}

static bool record_failure(usart_subtest_id_t id, uint32_t detail, uint32_t expected, uint32_t actual) {
    usart_failed_subtest = id;
    usart_failed_detail = detail;
    usart_failed_expected = expected;
    usart_failed_actual = actual;
    return false;
}

static bool buffers_equal(usart_subtest_id_t id, const uint8_t *expected, const uint8_t *actual, uint16_t len) {
    // The HAL reports how many bytes actually landed, so a short frame (or one
    // shifted by a stale byte) is caught here instead of silently comparing
    // against leftovers from the previous subtest.
    if (rx_frame_len != len) {
        return record_failure(id, LENGTH_MARKER, len, rx_frame_len);
    }

    for (uint16_t i = 0; i < len; i++) {
        if (expected[i] != actual[i]) {
            return record_failure(id, i, expected[i], actual[i]);
        }
    }
    return true;
}

/**
 * @brief Send one known frame on `periph` and verify it arrives intact on its
 * own loopback wire. Exercises the HW map (RCC bit, IRQ, DMA channel) and the
 * baud-rate divisor for whichever peripheral/clock-domain `periph` belongs to.
 */
static bool run_roundtrip(PHAL_USART_Idx_t periph, usart_subtest_id_t id, uint8_t seed) {
    prep_frame(seed);

    PHAL_USART_rx(periph, rx_buf, FRAME_LEN, false);

    uint32_t before = rx_frame_success_count;
    if (!PHAL_USART_tx(periph, tx_buf, FRAME_LEN)) {
        return record_failure(id, TIMEOUT_MARKER, 0, 1); // txDMA failed to start
    }
    if (!wait_for_frame(before)) {
        return record_failure(id, TIMEOUT_MARKER, 0, 2); // frame never arrived
    }

    return buffers_equal(id, tx_buf, rx_buf, FRAME_LEN);
}

static bool test_usart1_roundtrip(void) { return run_roundtrip(USART1_IDX, SUBTEST_USART1_ROUNDTRIP, 0xA0); }
static bool test_usart3_roundtrip(void) { return run_roundtrip(USART3_IDX, SUBTEST_USART3_ROUNDTRIP, 0xC0); }

/**
 * @brief Exercise PHAL_USART_txBlocking specifically. RX is armed non-blocking
 * first; starting txBl afterwards is safe because a UART bit time (tens of
 * microseconds) vastly exceeds the handful of instructions between the two
 * calls, so the frame can't arrive before the receiver is armed.
 */
static bool test_txBl_blocking_send(void) {
    prep_frame(0xD0);

    PHAL_USART_rx(TEST_PERIPH, rx_buf, FRAME_LEN, false);

    uint32_t before = rx_frame_success_count;
    if (!PHAL_USART_txBlocking(TEST_PERIPH, tx_buf, FRAME_LEN)) {
        return record_failure(SUBTEST_TXBL, TIMEOUT_MARKER, 0, 1); // txBl reported failure to start
    }
    if (!wait_for_frame(before)) {
        return record_failure(SUBTEST_TXBL, TIMEOUT_MARKER, 0, 2); // txBl returned before data was actually sent
    }

    return buffers_equal(SUBTEST_TXBL, tx_buf, rx_buf, FRAME_LEN);
}

/**
 * @brief Exercise PHAL_USART_rxBlocking specifically. TX is started non-blocking
 * first (same timing argument as above), then rxBl arms RX and blocks until
 * the IDLE line signals the frame is complete.
 */
static bool test_rxBl_blocking_receive(void) {
    prep_frame(0xE0);

    if (!PHAL_USART_tx(TEST_PERIPH, tx_buf, FRAME_LEN)) {
        return record_failure(SUBTEST_RXBL, TIMEOUT_MARKER, 0, 0);
    }
    PHAL_USART_rxBlocking(TEST_PERIPH, rx_buf, FRAME_LEN);

    return buffers_equal(SUBTEST_RXBL, tx_buf, rx_buf, FRAME_LEN);
}

//! Verify txBusy() is false at rest, true immediately after txDMA starts, and
//! false again once the TX-DMA-complete ISR runs.
static bool test_txBusy_tracks_transfer(void) {
    if (PHAL_USART_txBusy(TEST_PERIPH)) {
        return record_failure(SUBTEST_TXBUSY, 0, 0, 1); // busy before any transfer was started
    }

    fill_pattern(tx_buf, FRAME_LEN, 0xF0);
    if (!PHAL_USART_tx(TEST_PERIPH, tx_buf, FRAME_LEN)) {
        return record_failure(SUBTEST_TXBUSY, TIMEOUT_MARKER, 0, 2);
    }

    if (!PHAL_USART_txBusy(TEST_PERIPH)) {
        return record_failure(SUBTEST_TXBUSY, 1, 1, 0); // must be busy right after starting
    }

    if (!wait_for_tx_free(TEST_PERIPH)) {
        return record_failure(SUBTEST_TXBUSY, TIMEOUT_MARKER, 0, 1); // never cleared - TX DMA ISR bug
    }

    return true;
}

/**
 * @brief Verify one-shot RX (cont=false) captures exactly one frame and then
 * stops receiving until explicitly re-armed.
 */
static bool test_oneshot_rx_does_not_rearm(void) {
    prep_frame(0x10);
    PHAL_USART_rx(TEST_PERIPH, rx_buf, FRAME_LEN, false);

    uint32_t before = rx_frame_success_count;
    if (!PHAL_USART_tx(TEST_PERIPH, tx_buf, FRAME_LEN)) {
        return record_failure(SUBTEST_ONESHOT, TIMEOUT_MARKER, 0, 1);
    }

    if (!wait_for_frame(before)) {
        return record_failure(SUBTEST_ONESHOT, TIMEOUT_MARKER, 0, 2);
    }

    if (!buffers_equal(SUBTEST_ONESHOT, tx_buf, rx_buf, FRAME_LEN)) {
        return false;
    }

    // Receiver should now be disabled (PHAL_USART_priv_stopRx) - a second frame
    // must be dropped, not silently captured into the stale rx_buf.
    uint32_t after_first = rx_frame_success_count;
    fill_pattern(tx_buf, FRAME_LEN, 0x11);
    if (!PHAL_USART_tx(TEST_PERIPH, tx_buf, FRAME_LEN)) {
        return record_failure(SUBTEST_ONESHOT, TIMEOUT_MARKER, 0, 3);
    }
    delay_iters(usart_test_timeout_iters);
    if (rx_frame_success_count != after_first) {
        return record_failure(SUBTEST_ONESHOT, 4, after_first, rx_frame_success_count); // captured a frame it shouldn't have
    }

    // Re-arming should cleanly capture the next frame.
    memset(rx_buf, 0, FRAME_LEN);
    PHAL_USART_rx(TEST_PERIPH, rx_buf, FRAME_LEN, false);

    fill_pattern(tx_buf, FRAME_LEN, 0x12);
    if (!PHAL_USART_tx(TEST_PERIPH, tx_buf, FRAME_LEN)) {
        return record_failure(SUBTEST_ONESHOT, TIMEOUT_MARKER, 0, 6);
    }
    if (!wait_for_frame(after_first)) {
        return record_failure(SUBTEST_ONESHOT, TIMEOUT_MARKER, 0, 7); // re-arm after stop failed to start receiving again
    }

    return buffers_equal(SUBTEST_ONESHOT, tx_buf, rx_buf, FRAME_LEN);
}

static constexpr uint32_t CONT_RX_FRAMES = 8;

/**
 * @brief Send several distinct frames back-to-back with continuous RX
 * (cont=true) active. Regression test for the DMA re-arm path - the HAL
 * comments call out a bug where a stale TEIF flag from a prior transfer
 * could stall re-arm on the *second* frame, so a single frame isn't enough.
 */
static bool test_continuous_rx_multiframe(void) {
    memset(rx_buf, 0, FRAME_LEN);
    PHAL_USART_rx(TEST_PERIPH, rx_buf, FRAME_LEN, true);

    for (uint32_t frame = 0; frame < CONT_RX_FRAMES; frame++) {
        uint32_t before = rx_frame_success_count;
        fill_pattern(tx_buf, FRAME_LEN, (uint8_t)(0x20 + frame));

        if (!PHAL_USART_tx(TEST_PERIPH, tx_buf, FRAME_LEN)) {
            return record_failure(SUBTEST_CONTINUOUS, frame, 0, 1);
        }
        if (!wait_for_frame(before)) {
            return record_failure(SUBTEST_CONTINUOUS, frame, 0, 2); // re-arm stalled on this frame
        }

        if (!buffers_equal(SUBTEST_CONTINUOUS, tx_buf, rx_buf, FRAME_LEN)) {
            return false;
        }

        // don't overwrite tx_buf until this frame's TX is done
        while (PHAL_USART_txBusy(TEST_PERIPH)) {
            __NOP();
        }
    }
    return true;
}

typedef struct {
    const char *name;
    bool (*fn)(void);
} usart_subtest_t;

static const usart_subtest_t subtests[NUM_SUBTESTS] = {
    [SUBTEST_USART1_ROUNDTRIP] = {"usart1 roundtrip (APB2, 115200 baud)", test_usart1_roundtrip},
    [SUBTEST_USART3_ROUNDTRIP] = {"usart3 roundtrip (APB1, 9600 baud)", test_usart3_roundtrip},
    [SUBTEST_TXBL] = {"txBl blocking send", test_txBl_blocking_send},
    [SUBTEST_RXBL] = {"rxBl blocking receive", test_rxBl_blocking_receive},
    [SUBTEST_TXBUSY] = {"txBusy tracks transfer", test_txBusy_tracks_transfer},
    [SUBTEST_ONESHOT] = {"one-shot RX does not re-arm", test_oneshot_rx_does_not_rearm},
    [SUBTEST_CONTINUOUS] = {"continuous RX multi-frame re-arm", test_continuous_rx_multiframe},
};

int main(void) {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);
    usart_test_timeout_iters = compute_timeout_iters(PHAL_RCC_getAHBClockHz());

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, 1);
    PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, 0);

    if (!PHAL_USART_init(USART1_IDX, USART1_TEST_BAUD, PHAL_RCC_getAPB2ClockHz())
        || !PHAL_USART_init(USART3_IDX, USART3_TEST_BAUD, PHAL_RCC_getAPB1ClockHz())
    ) {
        HardFault_Handler();
    }

    bool pass = true;
    for (uint32_t i = 0; i < countof(subtests) && pass; i++) {
        usart_failed_subtest = i;
        pass = subtests[i].fn();
    }

    PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, pass ? 1 : 0);
    PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, pass ? 0 : 1);

    while (1) {
        __NOP();
    }

    return 0;
}

void PHAL_USART_rxCallback(PHAL_USART_Idx_t periph, uint16_t len) {
    (void)periph;
    rx_frame_len = len;
    rx_frame_success_count++;
}

void HardFault_Handler(void) {
    while (1) {
        __NOP();
    }
}

#endif // G4_TESTING_CHOSEN == TEST_USART
