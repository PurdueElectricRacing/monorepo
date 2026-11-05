#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_PEER)

/**
 * @file peer_test.c
 * @brief Two-board clock interoperability test (UART, CAN, SPI).
 *
 * Proves boards running different system clocks still agree on bit rates:
 * flash Nucleo A with PEER_NODE_ID = 0 / PEER_RCC_MODE = PHAL_RCC_HSI_16MHZ and
 * Nucleo B with PEER_NODE_ID = 1 / PEER_RCC_MODE = PHAL_RCC_HSI_170MHZ.
 * Every PEER_PERIOD_MS each board sends a sequence-numbered UART frame, CAN
 * frame and SPI transfer, and checks what it receives from the other board.
 * A loopback test on one board can't catch a wrong bit rate (both ends are
 * wrong the same way); two boards at different clocks can.
 *
 * Wiring (A <-> B), only for the links enabled below (PEER_*_ENABLED):
 *   GND <-> GND
 *   USART1: A.PA9 (TX) -> B.PA10 (RX), B.PA9 (TX) -> A.PA10 (RX)
 *   FDCAN2: PB13 (TX) / PB12 (RX) -> a 3.3 V CAN transceiver on each board,
 *           transceivers on one CANH/CANL pair with 120 ohm at each end
 *   SPI1:   PA5 SCK, PA6 MISO, PA7 MOSI, PA4 NSS straight through
 *
 * SPI is synchronous (the slave follows the master's clock), so it only
 * proves each board works as master/slave at its clock, not the SCK rate.
 * Run once with each board as master (PEER_SPI_MASTER_NODE_ID).
 * Reset the SPI slave first, or reset both together.
 *
 * Result: green LED toggles every period while no errors have been seen; red
 * LED latches on any error. Read peer_results with the debugger.
 */

#include <stdint.h>
#include <string.h>

#include "common/phal_G4/dma/dma.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/spi/spi.h"
#include "common/phal_G4/usart/usart.h"
#include "common/rtos/rtos.h"
#include "common/utils/countof.h"
#include "main.h"

// ---- Per-board configuration: change these for each Nucleo ----
static constexpr uint32_t PEER_NODE_ID            = 1U;
static constexpr PHAL_RCC_Mode_t PEER_RCC_MODE    = PHAL_RCC_HSI_170MHZ;
static constexpr uint32_t PEER_SPI_MASTER_NODE_ID = 1U;
static constexpr PHAL_FDCAN_BaudRate_t PEER_CAN_BAUD = FDCAN_BAUD_500K;
// Links under test; set identically on both boards. CAN is off by default
// because it needs a transceiver on each board (otherwise it only reports errors)
static constexpr bool PEER_UART_ENABLED = true;
static constexpr bool PEER_SPI_ENABLED  = true;
static constexpr bool PEER_CAN_ENABLED  = false;
// ----------------------------------------------------------------

static_assert(PEER_NODE_ID <= 1U && PEER_SPI_MASTER_NODE_ID <= 1U, "the peer test has exactly two nodes: 0 and 1");
static constexpr uint32_t PEER_OTHER_NODE_ID = 1U - PEER_NODE_ID;
static constexpr bool PEER_IS_SPI_MASTER     = (PEER_NODE_ID == PEER_SPI_MASTER_NODE_ID);

static constexpr uint32_t PEER_PERIOD_MS     = 100U;
// Fast enough to timestamp each UART frame and re-arm the SPI slave promptly
static constexpr uint32_t PEER_FAST_SERVICE_MS = 1U;
static_assert(configTICK_RATE_HZ == 1000U, "UART timestamps assume 1 RTOS tick = 1 ms");
static constexpr uint32_t PEER_UART_BAUD     = 115'200U;
static constexpr uint32_t PEER_SPI_RATE      = 1'000'000U;
static constexpr uint16_t PEER_CAN_BASE_ID   = 0x400U;

// UART frame: magic, sender node id, 32-bit little-endian sequence number, end marker
static constexpr uint8_t PEER_UART_MAGIC     = 0xA5U;
static constexpr uint8_t PEER_UART_END       = 0x5AU;
static constexpr uint16_t PEER_UART_FRAME_LEN = 7U;
// Larger than one frame so the IDLE line, not a full buffer, ends each frame
static constexpr uint16_t PEER_UART_RX_BUF_LEN = 2U * PEER_UART_FRAME_LEN;

static constexpr uint8_t PEER_CAN_DLC = 4U;

static constexpr uint32_t PEER_SPI_XFER_LEN = 16U;
// Patterns are fixed so neither side needs to know the other's sequence
static constexpr uint8_t PEER_SPI_MASTER_PATTERN_XOR = 0x3CU;
static constexpr uint8_t PEER_SPI_SLAVE_PATTERN_XOR  = 0xC3U;

void HardFault_Handler(void);

typedef struct {
    uint32_t good;
    uint32_t bad;
    uint32_t last_seq;
    bool synced; ///< first valid message seen; sequence checks start after it
} peer_link_stats_t;

typedef struct {
    peer_link_stats_t uart;
    peer_link_stats_t can;
    uint32_t spi_good;
    uint32_t spi_bad;
    uint32_t can_tx_dropped;  ///< TX FIFO full: frames not being acknowledged
    uint32_t can_max_tec;     ///< worst FDCAN transmit error count seen
    uint32_t can_max_rec;     ///< worst FDCAN receive error count seen
    uint32_t uart_timed_frames; ///< frames timestamped by the fast service task
    uint32_t uart_first_rx_ms;
    uint32_t uart_last_rx_ms;
    uint32_t uart_rx_interval_avg_ms; ///< ~PEER_PERIOD_MS when both boards' clocks and RTOS ticks are right
} peer_results_t;

volatile peer_results_t peer_results;

static void peer_periodic(void);
static void peer_fast_service(void);

RTOS_DEFINE_TASK(peer_periodic, PEER_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_512);
RTOS_DEFINE_TASK(peer_fast_service, PEER_FAST_SERVICE_MS, TASK_PRIORITY_NORMAL, STACK_512);

PHAL_GPIO_InitConfig_t gpio_config_leds[] = {
    PHAL_GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_RED_PORT, LED_RED_PIN, GPIO_OUTPUT_LOW_SPEED),
};

PHAL_GPIO_InitConfig_t gpio_config_uart[] = {
    PHAL_PIN_DEFS_USART1_TX_PA9,
    PHAL_PIN_DEFS_USART1_RX_PA10,
};

PHAL_GPIO_InitConfig_t gpio_config_can[] = {
    PHAL_PIN_DEFS_FDCAN2_RX_PB12,
    PHAL_PIN_DEFS_FDCAN2_TX_PB13,
};

PHAL_GPIO_InitConfig_t gpio_config_spi[] = {
    PHAL_PIN_DEFS_SPI1_SCK_PA5,
    PHAL_PIN_DEFS_SPI1_MISO_PA6,
    PHAL_PIN_DEFS_SPI1_MOSI_PA7,
};

PHAL_GPIO_InitConfig_t gpio_config_spi_master[] = {
    PHAL_GPIO_INIT_OUTPUT(GPIOA, 4, GPIO_OUTPUT_ULTRA_SPEED), // software CS
};

PHAL_GPIO_InitConfig_t gpio_config_spi_slave[] = {
    PHAL_PIN_DEFS_SPI1_NSS_PA4, // hardware NSS
};

static PHAL_DMA_Handle_t spi1_rx_dma = {
    .wiring = &SPI1_RX_DMA_WIRING,
    .params = {.priority = DMA_PRIORITY_HIGH, .mode = DMA_MODE_NORMAL, .mem_inc = true, .tx_isr_en = true},
};

static PHAL_DMA_Handle_t spi1_tx_dma = {
    .wiring = &SPI1_TX_DMA_WIRING,
    .params = {.priority = DMA_PRIORITY_HIGH, .mode = DMA_MODE_NORMAL, .mem_inc = true, .tx_isr_en = true},
};

static SPI_InitConfig_t spi1 = {
    .periph        = SPI1,
    .data_rate     = PEER_SPI_RATE,
    .data_len      = 8,
    .mode          = PEER_IS_SPI_MASTER ? SPI_MODE_MASTER : SPI_MODE_SLAVE,
    .nss_sw        = PEER_IS_SPI_MASTER,
    .nss_gpio_port = GPIOA,
    .nss_gpio_pin  = 4,
    .cpol          = SPI_CPOL_IDLE_LOW,
    .cpha          = SPI_CPHA_FIRST_EDGE,
    .rx_dma        = &spi1_rx_dma,
    .tx_dma        = &spi1_tx_dma,
};

static uint8_t uart_tx_buf[PEER_UART_FRAME_LEN];
static uint8_t uart_rx_buf[PEER_UART_RX_BUF_LEN];
static uint8_t spi_tx_buf[PEER_SPI_XFER_LEN];
static uint8_t spi_rx_buf[PEER_SPI_XFER_LEN];

static uint32_t peer_read_u32_le(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 0U) | ((uint32_t)bytes[1] << 8U)
        | ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static void peer_write_u32_le(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value >> 0U);
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

/// Count a received sequence number as good only if it follows the previous one
static void peer_record_seq(volatile peer_link_stats_t *link, uint32_t seq) {
    if (link->synced && seq != link->last_seq + 1U) {
        link->bad++;
    } else {
        link->good++;
    }
    link->synced   = true;
    link->last_seq = seq;
}

void PHAL_USART_rxCallback(PHAL_USART_Idx_t periph_idx, uint16_t len) {
    if (periph_idx != USART1_IDX) {
        return;
    }
    if (len != PEER_UART_FRAME_LEN || uart_rx_buf[0] != PEER_UART_MAGIC
        || uart_rx_buf[1] != PEER_OTHER_NODE_ID || uart_rx_buf[6] != PEER_UART_END) {
        peer_results.uart.bad++;
        return;
    }
    // No timestamp here: this ISR's priority is above what FreeRTOS calls allow,
    // so peer_fast_service timestamps new frames instead
    peer_record_seq(&peer_results.uart, peer_read_u32_le(&uart_rx_buf[2]));
}

void PHAL_FDCAN_rxCallback(CanMsgTypeDef_t *msg) {
    // The filter only admits the other node's ID; anything else is a fault
    if (msg->IDE || msg->StdId != PEER_CAN_BASE_ID + PEER_OTHER_NODE_ID || msg->DLC != PEER_CAN_DLC) {
        peer_results.can.bad++;
        return;
    }
    peer_record_seq(&peer_results.can, peer_read_u32_le(msg->Data));
}

static void peer_send_uart(uint32_t seq) {
    if (PHAL_USART_txBusy(USART1_IDX)) {
        peer_results.uart.bad++; // previous frame still on the wire: baud far too slow
        return;
    }
    uart_tx_buf[0] = PEER_UART_MAGIC;
    uart_tx_buf[1] = (uint8_t)PEER_NODE_ID;
    peer_write_u32_le(&uart_tx_buf[2], seq);
    uart_tx_buf[6] = PEER_UART_END;
    if (!PHAL_USART_tx(USART1_IDX, uart_tx_buf, PEER_UART_FRAME_LEN)) {
        peer_results.uart.bad++;
    }
}

static void peer_send_can(uint32_t seq) {
    CanMsgTypeDef_t msg = {
        .Bus   = FDCAN2,
        .IDE   = false,
        .StdId = (uint16_t)(PEER_CAN_BASE_ID + PEER_NODE_ID),
        .DLC   = PEER_CAN_DLC,
    };
    peer_write_u32_le(msg.Data, seq);
    if (!PHAL_FDCAN_txFifoFree(FDCAN2) || !PHAL_FDCAN_send(&msg)) {
        peer_results.can_tx_dropped++;
    }
}

static void peer_track_can_errors(void) {
    const uint32_t ecr = FDCAN2->ECR;
    const uint32_t tec = (ecr & FDCAN_ECR_TEC_Msk) >> FDCAN_ECR_TEC_Pos;
    const uint32_t rec = (ecr & FDCAN_ECR_REC_Msk) >> FDCAN_ECR_REC_Pos;
    if (tec > peer_results.can_max_tec) {
        peer_results.can_max_tec = tec;
    }
    if (rec > peer_results.can_max_rec) {
        peer_results.can_max_rec = rec;
    }
}

static void peer_fill_spi_tx(void) {
    const uint8_t pattern_xor = PEER_IS_SPI_MASTER ? PEER_SPI_MASTER_PATTERN_XOR : PEER_SPI_SLAVE_PATTERN_XOR;
    for (uint32_t i = 0U; i < PEER_SPI_XFER_LEN; i++) {
        spi_tx_buf[i] = (uint8_t)i ^ pattern_xor;
    }
}

static bool peer_spi_rx_matches_other(void) {
    const uint8_t other_xor = PEER_IS_SPI_MASTER ? PEER_SPI_SLAVE_PATTERN_XOR : PEER_SPI_MASTER_PATTERN_XOR;
    for (uint32_t i = 0U; i < PEER_SPI_XFER_LEN; i++) {
        if (spi_rx_buf[i] != ((uint8_t)i ^ other_xor)) {
            return false;
        }
    }
    return true;
}

static void peer_check_spi_rx(void) {
    if (peer_spi_rx_matches_other()) {
        peer_results.spi_good++;
    } else if (peer_results.spi_good > 0U) {
        // Failures before the first good exchange are boot-order skew
        peer_results.spi_bad++;
    }
    memset(spi_rx_buf, 0, sizeof(spi_rx_buf));
}

/// Master: one transfer per period. Slave: keep a transfer armed for the master
static void peer_service_spi(bool period_elapsed) {
    if (PHAL_SPI_busy(&spi1)) {
        return;
    }
    if (PEER_IS_SPI_MASTER) {
        if (period_elapsed) {
            PHAL_SPI_transferBlocking(&spi1, spi_tx_buf, spi_rx_buf, PEER_SPI_XFER_LEN);
            peer_check_spi_rx();
        }
        return;
    }

    static bool slave_armed = false;
    if (slave_armed) {
        peer_check_spi_rx();
    }
    PHAL_SPI_transfer(&spi1, spi_tx_buf, spi_rx_buf, PEER_SPI_XFER_LEN);
    slave_armed = true;
}

/// Timestamp each newly received good UART frame (to 1 ms) and update the average interval
static void peer_timestamp_uart_rx(void) {
    const uint32_t good_frames = peer_results.uart.good;
    if (good_frames == peer_results.uart_timed_frames) {
        return;
    }
    const uint32_t now_ms = xTaskGetTickCount();
    if (peer_results.uart_timed_frames == 0U) {
        peer_results.uart_first_rx_ms = now_ms;
    }
    peer_results.uart_last_rx_ms   = now_ms;
    peer_results.uart_timed_frames = good_frames;
    if (good_frames >= 2U) {
        peer_results.uart_rx_interval_avg_ms =
            (peer_results.uart_last_rx_ms - peer_results.uart_first_rx_ms) / (good_frames - 1U);
    }
}

static bool peer_any_errors(void) {
    return peer_results.uart.bad != 0U || peer_results.can.bad != 0U || peer_results.spi_bad != 0U
        || peer_results.can_tx_dropped != 0U || peer_results.can_max_tec != 0U
        || peer_results.can_max_rec != 0U;
}

static void peer_init_uart(void) {
    if (!PHAL_GPIO_init(gpio_config_uart, countof(gpio_config_uart))
        || !PHAL_USART_init(USART1_IDX, PEER_UART_BAUD, PHAL_RCC_getAPB2ClockHz())) {
        HardFault_Handler();
    }
    PHAL_USART_rx(USART1_IDX, uart_rx_buf, PEER_UART_RX_BUF_LEN, true);
}

static void peer_init_spi(void) {
    const bool cs_ok = PEER_IS_SPI_MASTER
        ? PHAL_GPIO_init(gpio_config_spi_master, countof(gpio_config_spi_master))
        : PHAL_GPIO_init(gpio_config_spi_slave, countof(gpio_config_spi_slave));
    if (!cs_ok || !PHAL_GPIO_init(gpio_config_spi, countof(gpio_config_spi)) || !PHAL_SPI_init(&spi1)) {
        HardFault_Handler();
    }
    peer_fill_spi_tx();
}

static void peer_init_can(void) {
    if (!PHAL_GPIO_init(gpio_config_can, countof(gpio_config_can))) {
        HardFault_Handler();
    }
    PHAL_FDCAN_init(FDCAN2, PEER_CAN_BAUD);
    const uint32_t other_can_id = PEER_CAN_BASE_ID + PEER_OTHER_NODE_ID;
    if (!PHAL_FDCAN_setFilters(FDCAN2, &other_can_id, 1U, nullptr, 0U)) {
        HardFault_Handler();
    }
    NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
}

static void peer_init_peripherals(void) {
    if (!PHAL_GPIO_init(gpio_config_leds, countof(gpio_config_leds))) {
        HardFault_Handler();
    }
    if (PEER_UART_ENABLED) {
        peer_init_uart();
    }
    if (PEER_SPI_ENABLED) {
        peer_init_spi();
    }
    if (PEER_CAN_ENABLED) {
        peer_init_can();
    }
}

/// Every PEER_PERIOD_MS: send on each enabled link and update the LEDs
static void peer_periodic(void) {
    static uint32_t seq = 0U;
    if (PEER_UART_ENABLED) {
        peer_send_uart(seq);
    }
    if (PEER_CAN_ENABLED) {
        peer_send_can(seq);
        peer_track_can_errors();
    }
    if (PEER_SPI_ENABLED && PEER_IS_SPI_MASTER) {
        peer_service_spi(true);
    }
    seq++;

    const bool errors = peer_any_errors();
    PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, errors);
    if (!errors) {
        PHAL_GPIO_toggle(LED_GREEN_PORT, LED_GREEN_PIN);
    }
}

/// Every PEER_FAST_SERVICE_MS: timestamp UART frames and keep the SPI slave armed
static void peer_fast_service(void) {
    if (PEER_UART_ENABLED) {
        peer_timestamp_uart_rx();
    }
    if (PEER_SPI_ENABLED && !PEER_IS_SPI_MASTER) {
        peer_service_spi(false);
    }
}

int main(void) {
    PHAL_RCC_init(PEER_RCC_MODE);
    peer_init_peripherals();

    RTOS_START_TASK(peer_periodic);
    RTOS_START_TASK(peer_fast_service);
    vTaskStartScheduler();

    return 0;
}

void HardFault_Handler(void) {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_PEER
