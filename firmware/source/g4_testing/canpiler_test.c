#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_CANPILER)

#include <stdint.h>

#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/pin_defs/g474ret6.h"
#include "common/rtos/rtos.h"
#include "common/utils/countof.h"
#include "can_library/can_common.h"
#include "can_library/generated/G4_TESTING.h"


PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_PIN_DEFS_FDCAN2_RX_PB12,
    PHAL_PIN_DEFS_FDCAN2_TX_PB13,
    PHAL_PIN_DEFS_FDCAN3_RX_PA8,
    PHAL_PIN_DEFS_FDCAN3_TX_PB4,
    PHAL_PIN_DEFS_FDCAN1_RX_PA11,
    PHAL_PIN_DEFS_FDCAN1_TX_PA12
};

void HardFault_Handler();

static int tx_counter = 0;
// static int rx_count = 0;
static int debg = 0;
static int fdcan_tx_counter = 0;

/** 
 * @brief Encode and transmit canpiler_test message
 *
 * Sends canpiler_test with fixed physical values, encoded using the
 * generated constants. Daqapp decodes via SuperDBC and should show the PHYS
 * values below. physical = raw * scale + offset
 *
 * signal       encode                        PHYS(shown by daqapp)     raw     
 * temperature  phys - OFFSET                  25.0 C                   65, -40C offset  
 * current      phys * PACK_COEFF             -125.5 A                 -1255, 0.1A scale
 * voltage      (phys - OFFSET) * PACK_COEFF   3.700 V                  1200, 0.001V scale, 2.5V offset   
 * pressure     (phys - OFFSET) * PACK_COEFF   300.0 kPa                140, 2.0 kPa scale, 20kPa offset
 * status       phys (no scale/offset)         165                      165, no scale/offset
 */

uint8_t temperature = 0;
void send_periodic() {
    temperature++;
    int16_t current = (int16_t)(-125.5f * PACK_COEFF_CANPILER_TEST_CURRENT);
    uint16_t voltage = (uint16_t)((3.700f - OFFSET_CANPILER_TEST_VOLTAGE) * PACK_COEFF_CANPILER_TEST_VOLTAGE);
    uint8_t pressure = (uint8_t)((300.0f - OFFSET_CANPILER_TEST_PRESSURE) * PACK_COEFF_CANPILER_TEST_PRESSURE);
    uint8_t status = 165;

    CAN_SEND_canpiler_test(temperature, current, voltage, pressure, status);

    tx_counter++;

    if (tx_counter > 250) {
        debg++;
    }
    
}

void send_tx_error_count() {
    uint32_t txCounter = PHAL_FDCAN_txErrCounter(FDCAN2);
    CAN_SEND_tx_errctr(txCounter);
}

void PHAL_FDCAN_rxCallback(CanMsgTypeDef_t *msg);

void send_to3() {
    CanMsgTypeDef_t msg;
    msg.Bus            = FDCAN2;
    msg.IDE   = false;
    msg.StdId          = 0x300;
    uint8_t payload[8] = {'A', 'A', 'A', 'A', 'A', '_', 'T', 'X'};
    msg.DLC            = sizeof(payload);
    memcpy(msg.Data, payload, sizeof(payload));
    PHAL_FDCAN_send(&msg);
    fdcan_tx_counter++;
}

void receive_from2();


void induce_fdcan2_error() {
    CanMsgTypeDef_t err_msg = {
        .Bus   = FDCAN2,
        .IDE   = false,
        .StdId = 0x123,
        .DLC   = 1,
        .Data  = { 0xAA }
    };
    PHAL_FDCAN_send(&err_msg);
}

void fdcan_error_monitor_task() {

}

void fdcan_busoff_recovery_task(void) {
    // Check if FDCAN2 is in Bus-Off state (TEC >= 256 or PSR.BO set)
    if (PHAL_FDCAN_isBusOff(FDCAN2)) {
        // Trigger recovery (non-blocking)
        PHAL_FDCAN_recoverBusOff(FDCAN2);
    }
}

DEFINE_CAN_TASKS();
RTOS_DEFINE_TASK(send_periodic, 100, TASK_PRIORITY_NORMAL, 512);
RTOS_DEFINE_TASK(send_tx_error_count, 100, TASK_PRIORITY_NORMAL, 512);
RTOS_DEFINE_TASK(induce_fdcan2_error, 400, TASK_PRIORITY_NORMAL, 512);//RTOS_DEFINE_TASK(send_to3, 150, TASK_PRIORITY_HIGH, 512);
RTOS_DEFINE_TASK(fdcan_busoff_recovery_task, 100, TASK_PRIORITY_NORMAL, 512);
//RTOS_DEFINE_TASK(fdcan_error_monitor_task, 100, TASK_PRIORITY_NORMAL, 512);
//RTOS_DEFINE_TASK(receive_from2, 10, TASK_PRIORITY_HIGH, 512);

// RTOS_DEFINE_QUEUE(q_can_rx, CanMsgTypeDef_t, 256);
// RTOS_DEFINE_TASK(fdcan2To3, 150, TASK_PRIORITY_NORMAL, 512);

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    // PHAL_FDCAN_init(FDCAN1, FDCAN_BAUD_500K);
    
    PHAL_FDCAN_init(FDCAN2, FDCAN_BAUD_250K);
    //PHAL_FDCAN_init(FDCAN3, FDCAN_BAUD_500K);

    CAN_init();

    

    START_CAN_TASKS();  
    RTOS_START_TASK(send_periodic);
    RTOS_START_TASK(send_tx_error_count);
    RTOS_START_TASK(induce_fdcan2_error);
    RTOS_START_TASK(fdcan_busoff_recovery_task);

    // RTOS_START_TASK(send_to3);
    //RTOS_START_TASK(receive_from2);

    // RTOS_INIT_QUEUE(q_can_rx);
    // RTOS_START_TASK(fdcan2To3);


    //uint32_t sids[8] = {0x300, 0x301};
    //uint32_t xids[8] = {0x1ABCDE1, 0x1ABCDE2, 0x1ABCDE3};
    //PHAL_FDCAN_setFilters(FDCAN2, sids, 2, xids, 3);
    //PHAL_FDCAN_setFilters(FDCAN3, sids, 2, xids, 3);

    //NVIC_SetPriority(FDCAN2_IT0_IRQn, 6);
    //NVIC_SetPriority(FDCAN3_IT0_IRQn, 7);
    //NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
    //NVIC_EnableIRQ(FDCAN3_IT0_IRQn);

    vTaskStartScheduler();

    return 0;
}
/*
__attribute__((weak)) void PHAL_FDCAN_rxCallback(CanMsgTypeDef_t *msg) {
    rx_count++;
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        BaseType_t xHigherPriorityTaskWoken = 0;
        xQueueSendFromISR(q_can_rx, msg, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

volatile CanMsgTypeDef_t rx_frame_0;
void receive_from2() {
    CanMsgTypeDef_t rx_frame;
    while (xQueueReceive(q_can_rx, &rx_frame, (TickType_t)0) == pdTRUE) {
        rx_frame_0 = rx_frame;
        rx_count++;
    }
}*/


void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_CANPILER
