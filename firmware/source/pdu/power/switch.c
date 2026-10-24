/**
 * @file switch.c
 * @brief switch mappings and control code
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "switch.h"
#include "main.h"

switch_t g_switches[NUM_SWITCHES] = {
    [SWITCH_SDC] = {
        .ctrl_port = CTRL2_PORT,
        .ctrl_pin  = CTRL2_PIN,
        .mux_id    = ANALOG_MUX1,
        .mux_addr  = 1,
        .fault_id  = FAULT_ID_SDC_SWITCH_FAULT,
    },
    [SWITCH_SDC_LOOP] = {
        .ctrl_port = CTRL9_PORT,
        .ctrl_pin  = CTRL9_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 3,
        .fault_id  = FAULT_ID_SDC_LOOP_SWITCH_FAULT,
    },
    [SWITCH_MAIN] = {
        .ctrl_port = CTRL14_PORT,
        .ctrl_pin  = CTRL14_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 8,
        .fault_id  = FAULT_ID_MAIN_SWITCH_FAULT,
    },
    [SWITCH_DASH] = {
        .ctrl_port = CTRL3_PORT,
        .ctrl_pin  = CTRL3_PIN,
        .mux_id    = ANALOG_MUX1,
        .mux_addr  = 2,
        .fault_id  = FAULT_ID_DASH_SWITCH_FAULT,
    },
    [SWITCH_DAQ] = {
        .ctrl_port = CTRL13_PORT,
        .ctrl_pin  = CTRL13_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 1,
        .fault_id  = FAULT_ID_DAQ_SWITCH_FAULT,
    },
    [SWITCH_ABOX] = {
        .ctrl_port = CTRL5_PORT,
        .ctrl_pin  = CTRL5_PIN,
        .mux_id    = ANALOG_MUX1,
        .mux_addr  = 5,
        .fault_id  = FAULT_ID_ABOX_SWITCH_FAULT,
    },
    [SWITCH_HV_BOARD] = {
        .ctrl_port = CTRL21_PORT,
        .ctrl_pin  = CTRL21_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 14,
        .fault_id  = FAULT_ID_HV_BOARD_SWITCH_FAULT,
    },

    [SWITCH_TV] = {
        .ctrl_port = CTRL8_PORT,
        .ctrl_pin  = CTRL8_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 7,
        .fault_id  = FAULT_ID_TV_SWITCH_FAULT,
    },
    [SWITCH_DL_FRONT] = {
        .ctrl_port = CTRL7_PORT,
        .ctrl_pin  = CTRL7_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 6,
        .fault_id  = FAULT_ID_DL_FRONT_SWITCH_FAULT,
    },
    [SWITCH_DL_REAR] = {
        .ctrl_port = CTRL16_PORT,
        .ctrl_pin  = CTRL16_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 10,
        .fault_id  = FAULT_ID_DL_REAR_SWITCH_FAULT,
    },
    [SWITCH_AMK1] = {
        .ctrl_port = CTRL20_PORT,
        .ctrl_pin  = CTRL20_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 15,
        .fault_id  = FAULT_ID_AMK1_SWITCH_FAULT,
    },
    [SWITCH_AMK2] = {
        .ctrl_port = CTRL19_PORT,
        .ctrl_pin  = CTRL19_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 13,
        .fault_id  = FAULT_ID_AMK2_SWITCH_FAULT,
    },

    [SWITCH_PUMP1] = {
        .ctrl_port = CTRL11_PORT,
        .ctrl_pin  = CTRL11_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 4,
        .fault_id  = FAULT_ID_PUMP1_SWITCH_FAULT,
    },
    [SWITCH_PUMP2] = {
        .ctrl_port = CTRL6_PORT,
        .ctrl_pin  = CTRL6_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 5,
        .fault_id  = FAULT_ID_PUMP2_SWITCH_FAULT,
    },
    [SWITCH_HXFAN] = {
        .ctrl_port = CTRL10_PORT,
        .ctrl_pin  = CTRL10_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 2,
        .fault_id  = FAULT_ID_HXFAN_SWITCH_FAULT,
    },
    [SWITCH_FAN1] = {
        .ctrl_port = CTRL12_PORT,
        .ctrl_pin  = CTRL12_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 0,
        .fault_id  = FAULT_ID_FAN1_SWITCH_FAULT,
    },
    [SWITCH_FAN2] = {
        .ctrl_port = CTRL18_PORT,
        .ctrl_pin  = CTRL18_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 12,
        .fault_id  = FAULT_ID_FAN2_SWITCH_FAULT,
    },
    [SWITCH_FAN3] = {
        .ctrl_port = CTRL17_PORT,
        .ctrl_pin  = CTRL17_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 11,
        .fault_id  = FAULT_ID_FAN3_SWITCH_FAULT,
    },
    [SWITCH_FAN4] = {
        .ctrl_port = CTRL1_PORT,
        .ctrl_pin  = CTRL1_PIN,
        .mux_id    = ANALOG_MUX1,
        .mux_addr  = 0,
        .fault_id  = FAULT_ID_FAN4_SWITCH_FAULT,
    },
    [SWITCH_FAN5] = {
        .ctrl_port = CTRL4_PORT,
        .ctrl_pin  = CTRL4_PIN,
        .mux_id    = ANALOG_MUX1,
        .mux_addr  = 3,
        .fault_id  = FAULT_ID_FAN5_SWITCH_FAULT,
    },
    [SWITCH_EMETER] = {
        .ctrl_port = CTRL15_PORT,
        .ctrl_pin  = CTRL15_PIN,
        .mux_id    = ANALOG_MUX2,
        .mux_addr  = 9,
        .fault_id  = FAULT_ID_EMETER_SWITCH_FAULT,
    },
};

void SWITCH_enable(switch_id_t switch_id) {
    PHAL_GPIO_write(
        g_switches[switch_id].ctrl_port,
        g_switches[switch_id].ctrl_pin,
        true
    );
}

void SWITCH_disable(switch_id_t switch_id) {
    PHAL_GPIO_write(
        g_switches[switch_id].ctrl_port,
        g_switches[switch_id].ctrl_pin,
        false
    );
}
