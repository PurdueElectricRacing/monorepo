/**
 * @file switch.h
 * @brief switch mappings and control code
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef SWITCH_H
#define SWITCH_H

#include "common/phal_G4/gpio/gpio.h"
#include "can_library/faults_common.h"

typedef enum {
    SWITCH_SDC = 0,
    SWITCH_SDC_LOOP,
    SWITCH_MAIN,
    SWITCH_DASH,
    SWITCH_DAQ,
    SWITCH_ABOX,
    SWITCH_HV_BOARD,
    SWITCH_TV,
    SWITCH_DL_FRONT,
    SWITCH_DL_REAR,
    SWITCH_AMK1,
    SWITCH_AMK2,

    SWITCH_PUMP1,
    SWITCH_PUMP2,
    SWITCH_HXFAN,
    SWITCH_FAN1,
    SWITCH_FAN2,
    SWITCH_FAN3,
    SWITCH_FAN4,
    SWITCH_FAN5,

    SWITCH_EMETER,

    NUM_SWITCHES
} switch_id_t;

typedef enum {
    ANALOG_MUX1 = 0,
    ANALOG_MUX2
} mux_id_t;

typedef struct {
    GPIO_TypeDef* ctrl_port;
    uint8_t ctrl_pin;
    mux_id_t mux_id;
    uint8_t mux_addr;
    fault_id_t fault_id;
    float current_ma;
} switch_t;

// High power switch outputs 4.75v on fault -> scaled by R1=180, R2=330
static constexpr float SWITCH_FAULT_SENSE_V      = 4.75f;
static constexpr float SWITCH_SENSE_DIVIDER_GAIN = 330.0f / (180.0f + 330.0f);
static constexpr float ADC_VREF_V                = 3.3f;
// todo: tune margin
static constexpr float SWITCH_FAULT_MARGIN       = 0.95f;
static constexpr float SWITCH_FAULT_ADC_THRESHOLD =
    SWITCH_FAULT_SENSE_V
    * SWITCH_SENSE_DIVIDER_GAIN
    * (4095.0f / ADC_VREF_V)
    * SWITCH_FAULT_MARGIN;


extern switch_t g_switches[NUM_SWITCHES];

void SWITCH_enable(switch_id_t switch_id);
void SWITCH_disable(switch_id_t switch_id);

#endif // SWITCH_H