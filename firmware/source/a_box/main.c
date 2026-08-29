/**
 * @file main.c
 * @brief "Abox" node source code
 *
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "main.h"

/* System Includes */
#include "can_library/faults_common.h"
#include "can_library/generated/A_BOX.h"
#include "charging_fsm.h"
#include "common/rtos/rtos.h"
#include "common/heartbeat/heartbeat.h"
#include "common/phal_G4/adc/adc.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pin_defs/g474ret6.h"

/* Module Includes */
#include "adbms.h"
#include "common/utils/countof.h"
#include "common/watchdog/watchdog.h"
#include "telemetry.h"


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

SPI_InitConfig_t bms_spi_config = {
    .data_len      = 8,
    .nss_sw        = false, // BMS drive CS pin manually to ensure correct timing
    .nss_gpio_port = SPI1_CS_PORT,
    .nss_gpio_pin  = SPI1_CS_PIN,
    .rx_dma        = &spi1_rx_dma,
    .tx_dma        = &spi1_tx_dma,
    .periph        = SPI1,
    .cpol = SPI_CPOL_IDLE_LOW,
    .cpha = SPI_CPHA_FIRST_EDGE,
    .data_rate     = 500'000, // 500 kHz SPI clock for ADBMS6380
};

volatile adc1_dma_buffer_t adc1_dma_buffer;

static const PHAL_ADC_ChannelConfig_t adc_channels[] = {
    {.channel = ISENSE_ADC_CHANNEL},
    {.channel = VBATT_ADC_CHANNEL},
};
static const PHAL_ADC_Config_t adc_config = {
    .instance      = ADC1,
    .channels      = adc_channels,
    .channel_count = sizeof(adc_channels) / sizeof(adc_channels[0]),
};
PHAL_ADC_Handle_t adc_handle;



/* PER HAL Initilization Structures */
PHAL_GPIO_InitConfig_t gpio_config[] = {
    // Status LEDs
    PHAL_GPIO_INIT_OUTPUT(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(CONNECTION_LED_PORT, CONNECTION_LED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_OUTPUT_LOW_SPEED),

    // VCAN
    PHAL_PIN_DEFS_FDCAN1_RX_PA11,
    PHAL_PIN_DEFS_FDCAN1_TX_PA12,

    // CCAN
    PHAL_PIN_DEFS_FDCAN2_RX_PB12,
    PHAL_PIN_DEFS_FDCAN2_TX_PB13,

    // SPI for BMS
    PHAL_GPIO_INIT_OUTPUT(SPI1_CS_PORT, SPI1_CS_PIN, GPIO_OUTPUT_ULTRA_SPEED),
    PHAL_PIN_DEFS_SPI1_SCK_PA5,
    PHAL_PIN_DEFS_SPI1_MISO_PA6,
    PHAL_PIN_DEFS_SPI1_MOSI_PA7,

    // ISENSE and VSENSE
    PHAL_GPIO_INIT_ANALOG(ISENSE_GPIO_PORT, ISENSE_GPIO_PIN),
    PHAL_GPIO_INIT_ANALOG(VBATT_GPIO_PORT, VBATT_GPIO_PIN),

    // Input status pins
    PHAL_GPIO_INIT_INPUT(CHARGER_CONNECTED_PORT, CHARGER_CONNECTED_PIN, GPIO_INPUT_OPEN_DRAIN),
    PHAL_GPIO_INIT_INPUT(NOT_PRECHARGE_COMPLETE_PORT, NOT_PRECHARGE_COMPLETE_PIN, GPIO_INPUT_OPEN_DRAIN),
    PHAL_GPIO_INIT_INPUT(IMD_STATUS_PORT, IMD_STATUS_PIN, GPIO_INPUT_OPEN_DRAIN),

    // BMS SDC Control
    PHAL_GPIO_INIT_OUTPUT(BMS_SDC_CTRL_PORT, BMS_SDC_CTRL_PIN, GPIO_OUTPUT_LOW_SPEED)
};

adbms_bms_t g_bms                              = {0};
uint8_t g_bms_tx_buf[ADBMS_SPI_TX_BUFFER_SIZE] = {0};

static constexpr float MIN_V_FOR_BALANCE     = 3.2f; // hardstop to prevent cell drain
static constexpr float MIN_DELTA_FOR_BALANCE = 0.05f; // ~3% diff

extern void HardFault_Handler(void);
void bms_task(void);

// Thread Defines
DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(bms_task, 200, TASK_PRIORITY_NORMAL, STACK_2048);
RTOS_DEFINE_TASK(charging_fsm_periodic, ELCON_COMMAND_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_512);
RTOS_DEFINE_TASK(fault_library_periodic, A_BOX_FAULT_SYNC_PERIOD_MS, TASK_PRIORITY_NORMAL, STACK_1024);
RTOS_DEFINE_TASK(report_telemetry_100hz, TELEMETRY_100HZ_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
RTOS_DEFINE_TASK(report_telemetry_8hz, TELEMETRY_8HZ_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
RTOS_DEFINE_TASK(report_telemetry_02hz, TELEMETRY_02HZ_PERIOD_MS, TASK_PRIORITY_LOW, STACK_512);
DEFINE_WATCHDOG_TASK();
DEFINE_HEARTBEAT_TASK(nullptr);

int main(void) {
    // Hardware Initilization
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    WDG_init();
    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    // Set CS high to start
    adbms6380_set_cs_high(&bms_spi_config);

    if (!PHAL_SPI_init(&bms_spi_config)) {
        HardFault_Handler();
    }

    adbms_init(&g_bms, &bms_spi_config, g_bms_tx_buf);

    if (false == PHAL_ADC_init(&adc_handle, &adc_config)) {
        HardFault_Handler();
    }
    if (!PHAL_ADC_readDMA(&adc_handle, (uint16_t *)&adc1_dma_buffer,
                          sizeof(adc1_dma_buffer) / sizeof(uint16_t))) {
        HardFault_Handler();
    }

    PHAL_FDCAN_init(FDCAN1, VCAN_BAUD_RATE);
    PHAL_FDCAN_init(FDCAN2, CCAN_BAUD_RATE);
    CAN_init();

    START_CAN_TASKS();
    CAN_SEND_abox_init(WDG_get_CSR());
    RTOS_START_TASK(bms_task);
    RTOS_START_TASK(fault_library_periodic);
    RTOS_START_TASK(report_telemetry_100hz);
    RTOS_START_TASK(report_telemetry_8hz);
    RTOS_START_TASK(report_telemetry_02hz);
    RTOS_START_TASK(charging_fsm_periodic);
    START_WATCHDOG_TASK();
    START_HEARTBEAT_TASK();

    vTaskStartScheduler();

    return 0;
}

void bms_task(void) {
    // IMD
    bool imd_faulted = PHAL_GPIO_read(IMD_STATUS_PORT, IMD_STATUS_PIN) == false;
    update_fault(FAULT_ID_IMD, imd_faulted);

    // ADBMS
    adbms_periodic(&g_bms, MIN_V_FOR_BALANCE, MIN_DELTA_FOR_BALANCE);

    bool is_bms_disconnected = g_bms.state != ADBMS_STATE_CONNECTED;
    update_fault(FAULT_ID_BMS_DISCONNECTED, is_bms_disconnected);
    PHAL_GPIO_write(BMS_SDC_CTRL_PORT, BMS_SDC_CTRL_PIN, is_clear(FAULT_ID_BMS_DISCONNECTED));

    // Pack voltage checks
    update_fault(FAULT_ID_PACK_FULL, g_bms.sum_voltage);
    update_fault(FAULT_ID_PACK_EMPTY,g_bms.sum_voltage);

    // Cell voltage bounds
    update_fault(FAULT_ID_CELL_UNDERVOLTAGE, g_bms.min_voltage);
    update_fault(FAULT_ID_CELL_OVERVOLTAGE, g_bms.max_voltage);

    // Temperature related
    update_fault(FAULT_ID_PACK_OVERTEMP, g_bms.max_therm_temp);
    update_fault(FAULT_ID_PACK_WARM, g_bms.max_therm_temp);
    update_fault(FAULT_ID_PACK_COLD, g_bms.min_therm_temp);
}

void HardFault_Handler() {
    __disable_irq();
    SysTick->CTRL = 0;
    ERROR_LED_PORT->BSRR = (1U << ERROR_LED_PIN);
    while (1) {
        __asm__("NOP"); // spin
    }
}
