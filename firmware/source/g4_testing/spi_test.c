#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_SPI)

#include <stdbool.h>
#include <stdint.h>

#include "common/phal_G4/dma/dma.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/spi/spi.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/utils/countof.h"

// Prototypes
void HardFault_Handler();
static void delay_ms(uint32_t ms);
static bool verify_buffers(const uint8_t *tx, const uint8_t *rx, uint32_t len);

// Test Config 
#define XFER_LEN        16
#define TEST_DELAY_MS   100
#define TEST_ITERATIONS 1000
#define TIMEOUT 100000

// GPIO Configuration: SPI1 (Master) -> SPI2 (Slave)
PHAL_GPIO_InitConfig_t gpio_config[] = {
    // SPI1 - Standard Pins (Master)
    PHAL_PIN_DEFS_SPI1_SCK_PA5,
    PHAL_PIN_DEFS_SPI1_MISO_PA6,
    PHAL_PIN_DEFS_SPI1_MOSI_PA7,
    PHAL_GPIO_INIT_OUTPUT(GPIOA, 4, GPIO_OUTPUT_ULTRA_SPEED), // CS Software Master

    // SPI2 - RET Specific (Slave)
    PHAL_PIN_DEFS_SPI2_SCK_PB13,
    PHAL_PIN_DEFS_SPI2_MISO_PB14,
    PHAL_PIN_DEFS_SPI2_MOSI_PB15,
    PHAL_PIN_DEFS_SPI2_NSS_PB12, // CS Hardware Slave
};

// Test Buffers
static uint8_t master_tx[XFER_LEN];
static uint8_t master_rx[XFER_LEN];
static uint8_t slave_tx[XFER_LEN];
static uint8_t slave_rx[XFER_LEN];

// DMA handles
static PHAL_DMA_Handle_t spi1_rx_dma = {
    .wiring = &SPI1_RX_DMA_WIRING,
    .params = {
        .priority  = DMA_PRIORITY_HIGH,
        .mode      = DMA_MODE_NORMAL,
        .mem_inc   = true,
        .tx_isr_en = true,
    },
};

static PHAL_DMA_Handle_t spi1_tx_dma = {
    .wiring = &SPI1_TX_DMA_WIRING,
    .params = {
        .priority  = DMA_PRIORITY_HIGH,
        .mode      = DMA_MODE_NORMAL,
        .mem_inc   = true,
        .tx_isr_en = true,
    },
};

static PHAL_DMA_Handle_t spi2_rx_dma = {
    .wiring = &SPI2_RX_DMA_WIRING,
    .params = {
        .priority  = DMA_PRIORITY_HIGH,
        .mode      = DMA_MODE_NORMAL,
        .mem_inc   = true,
        .tx_isr_en = true,
    },
};

static PHAL_DMA_Handle_t spi2_tx_dma = {
    .wiring = &SPI2_TX_DMA_WIRING,
    .params = {
        .priority  = DMA_PRIORITY_HIGH,
        .mode      = DMA_MODE_NORMAL,
        .mem_inc   = true,
        .tx_isr_en = true,
    },
};

// SPI configs
static SPI_InitConfig_t spi1 = {
    .periph        = SPI1,
    .data_rate     = 1000000,
    .data_len      = 8,
    .mode          = SPI_MODE_MASTER,
    .nss_sw        = true,
    .nss_gpio_port = GPIOA,
    .nss_gpio_pin  = 4,
    .cpol          = SPI_CPOL_IDLE_LOW,
    .cpha          = SPI_CPHA_FIRST_EDGE,
    .rx_dma        = &spi1_rx_dma,
    .tx_dma        = &spi1_tx_dma,
};

static SPI_InitConfig_t spi2 = {
    .periph        = SPI2,
    .data_rate     = 1000000,
    .data_len      = 8,
    .mode          = SPI_MODE_SLAVE,
    .nss_sw        = false, // use hardware NSS via PB12
    .nss_gpio_port = GPIOB,
    .nss_gpio_pin  = 12,
    .cpol          = SPI_CPOL_IDLE_LOW,
    .cpha          = SPI_CPHA_FIRST_EDGE,
    .rx_dma        = &spi2_rx_dma,
    .tx_dma        = &spi2_tx_dma,
};

// Test success tracking (debugger)
volatile uint32_t pass_count = 0;
volatile uint32_t fail_count = 0;
volatile uint32_t iteration  = 0;

// Tick counter
volatile uint32_t ms_ticks = 0;

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    // Config systick for 1 ms
    SysTick_Config(SystemCoreClock / 1000);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    if (!PHAL_SPI_init(&spi1)) {
        HardFault_Handler();
    }
    if (!PHAL_SPI_init(&spi2)) {
        HardFault_Handler();
    }

    // Alternate between testing blocking and non-blocking SPI transfers
    while (iteration < TEST_ITERATIONS) {
        iteration++;

        // Test data changes each iteration
        for (uint32_t i = 0; i < XFER_LEN; i++) {
            master_tx[i] = (uint8_t)(iteration + i); // Master sends incrementing bytes
            slave_tx[i]  = (uint8_t)(0xFF - (iteration + i)); // Slave sends decrementing bytes
            master_rx[i] = 0x00;
            slave_rx[i]  = 0x00;
        }

        // Alternate between blocking + non-blocking
        bool is_blocking = (iteration % 2); // odd == blocking, even == non-blocking

        if (!is_blocking) {
            /// Non-blocking transfer test
            
            PHAL_SPI_transfer(&spi2, slave_tx, slave_rx, XFER_LEN);
            PHAL_SPI_transfer(&spi1, master_tx, master_rx, XFER_LEN);

            uint32_t timeout = TIMEOUT;
            while ((PHAL_SPI_busy(&spi1) || PHAL_SPI_busy(&spi2)) && --timeout > 0) {
                __asm__("nop");
            }

            if (timeout == 0) {
                fail_count++; // Timed out waiting for DMA
            }
        } else {
            /// Blocking transfer test
            
            PHAL_SPI_transfer(&spi2, slave_tx, slave_rx, XFER_LEN);
            PHAL_SPI_transferBlocking(&spi1, master_tx, master_rx, XFER_LEN);

            while (PHAL_SPI_busy(&spi1) || PHAL_SPI_busy(&spi2)) {
                __asm__("nop");
            }
        }

        // Verify that the data received matches what was sent
        bool master_valid = verify_buffers(slave_tx, master_rx, XFER_LEN);
        bool slave_valid  = verify_buffers(master_tx, slave_rx, XFER_LEN);

        if (master_valid && slave_valid) {
            pass_count++;
        } else {
            fail_count++; // Failed verification
        }

        delay_ms(TEST_DELAY_MS);
    }

    return 0;
}

static bool verify_buffers(const uint8_t *tx, const uint8_t *rx, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (tx[i] != rx[i]) return false;
    }
    return true;
}

void SysTick_Handler(void) {
    ms_ticks++;
}

static void delay_ms(uint32_t ms) {
    uint32_t start = ms_ticks;
    while ((ms_ticks - start) < ms);
}

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_SPI
