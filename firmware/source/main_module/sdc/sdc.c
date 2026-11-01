/**
 * @file sdc.c
 * @brief SDC task implementation
 *
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "sdc.h"

#include "can_library/faults_common.h"
#include "common/rtos/rtos.h"
#include "common/phal_G4/gpio/gpio.h"
#include "main.h"

typedef struct {
    fault_id_t fault_id;
    uint8_t mux_addr;
} sdc_node_t;

static const sdc_node_t SDC_NODE_LUT[] = {
    {FAULT_ID_SDC1_IMD, 8},
    {FAULT_ID_SDC2_BMS, 9},
    {FAULT_ID_SDC3_BSPD, 10},
    {FAULT_ID_SDC4_ECU_OK, 11},
    {FAULT_ID_SDC5_BOTS, 12},
    {FAULT_ID_SDC6_INERTIA, 13},
    {FAULT_ID_SDC7_COCKPIT_ESTOP, 14},
    {FAULT_ID_SDC9_FRONT_INTERLOCK, 15},
    {FAULT_ID_SDC10_RIGHT_ESTOP, 7},
    {FAULT_ID_SDC11_LEFT_ESTOP, 6},
    {FAULT_ID_SDC12_MSD, 5},
    {FAULT_ID_SDC13_E_METER, 4},
    {FAULT_ID_SDC14_HVD, 3},
    {FAULT_ID_SDC16_REAR_INTERLOCK, 2},
    {FAULT_ID_SDC17_TSMS, 1},
    {FAULT_ID_SDC19_PRECHARGE_COMPLETE, 0},
};

static size_t g_sdc_poll_index = 0;

void SDC_task_periodic(void) {
    const sdc_node_t *current_node = &SDC_NODE_LUT[g_sdc_poll_index];
    uint8_t mux_addr = current_node->mux_addr;

    // Set mux control
    bool mux_s0 = (mux_addr >> 0) & 0x1;
    bool mux_s1 = (mux_addr >> 1) & 0x1;
    bool mux_s2 = (mux_addr >> 2) & 0x1;
    bool mux_s3 = (mux_addr >> 3) & 0x1;
    PHAL_GPIO_write(SDC_MUX_S0_PORT, SDC_MUX_S0_PIN, mux_s0);
    PHAL_GPIO_write(SDC_MUX_S1_PORT, SDC_MUX_S1_PIN, mux_s1);
    PHAL_GPIO_write(SDC_MUX_S2_PORT, SDC_MUX_S2_PIN, mux_s2);
    PHAL_GPIO_write(SDC_MUX_S3_PORT, SDC_MUX_S3_PIN, mux_s3);

    // delay to allow mux signals to stabilize
    RTOS_delay_ms(1);

    // Reading the input pin as 1 = closed, 0 = open
    bool is_node_open = !PHAL_GPIO_read(SDC_MUX_PORT, SDC_MUX_PIN);
    update_fault(current_node->fault_id, is_node_open);

    // Advance poll
    size_t num_nodes = sizeof(SDC_NODE_LUT) / sizeof(SDC_NODE_LUT[0]);
    g_sdc_poll_index = (g_sdc_poll_index + 1) % num_nodes;
}