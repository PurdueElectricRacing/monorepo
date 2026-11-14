/**
 * @file switch.h
 * @brief switch mappings and control code
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef SWITCH_H
#define SWITCH_H

#include "common/phal_G4/gpio/gpio.h"
#include "can_library/generated/PDU.h"

static constexpr uint8_t NUM_SWITCHES = SWITCH_ID_EMETER + 1;

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

// TPS27S100B IMON current sense: G_IMON = 500 A/A, R1 = 180 ohm, R2 = 330 ohm
static constexpr float SWITCH_IMON_R1_OHMS        = 180.0f;
static constexpr float SWITCH_IMON_R2_OHMS        = 330.0f;
static constexpr float SWITCH_CURRENT_SENSE_GAIN  = 500.0f;
static constexpr float SWITCH_SENSE_DIVIDER_GAIN  = SWITCH_IMON_R2_OHMS / (SWITCH_IMON_R1_OHMS + SWITCH_IMON_R2_OHMS);

// High power switch outputs 4.75v on fault -> scaled by R1=180, R2=330
static constexpr float SWITCH_FAULT_SENSE_V       = 4.75f;
static constexpr float ADC_VREF_V                 = 3.3f;
static constexpr float ADC_MAX_COUNTS             = 4095.0f;
// todo: tune margin
static constexpr float SWITCH_FAULT_MARGIN        = 0.95f;
static constexpr float SWITCH_FAULT_CURRENT_THRESHOLD_MA =
    (SWITCH_FAULT_SENSE_V / (SWITCH_IMON_R1_OHMS + SWITCH_IMON_R2_OHMS))
    * SWITCH_CURRENT_SENSE_GAIN
    * 1000.0f
    * SWITCH_FAULT_MARGIN;


extern switch_t g_switches[NUM_SWITCHES];

void SWITCH_enable(switch_id_t switch_id);
void SWITCH_disable(switch_id_t switch_id);


void SWITCH_enable_all(void);

#endif // SWITCH_H
