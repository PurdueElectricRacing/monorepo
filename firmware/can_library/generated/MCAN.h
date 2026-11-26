#ifndef MCAN_H
#define MCAN_H

/**
 * @file MCAN.h
 * @brief Auto-generated CAN bus definitions for MCAN.
 * @note This file is auto-generated; do not modify manually.
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>
#if defined(STM32F423xx)
#include "common/phal_F4/can/can.h"
#elif defined(STM32G474xx)
#include "common/phal_G4/fdcan/fdcan.h"
#else
#error "Unsupported CAN target"
#endif
#include "can_library/generated/can_types.h"

static constexpr uint32_t MCAN_BUS_ID = 1;
static constexpr PHAL_FDCAN_BaudRate_t MCAN_BAUD_RATE = FDCAN_BAUD_500K;

static constexpr uint32_t INVA_SET_MSG_ID = 0x111;
static constexpr uint8_t  INVA_SET_DLC = 8;
static constexpr uint64_t INVA_SET_LAYOUT_HASH = 0xB379B31C5463DFD6;
static constexpr uint32_t INVA_SET_PERIOD_MS = 15;
static constexpr uint32_t INVA_SET_STALE_TIMEOUT_MS = 37;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t AMK_Control_bReserve;
    bool AMK_Control_bInverterOn;
    bool AMK_Control_bDcOn;
    bool AMK_Control_bEnable;
    bool AMK_Control_bErrorReset;
    uint8_t AMK_Control_bReserve2;
    int16_t AMK_TorqueSetpoint;
    int16_t AMK_PositiveTorqueLimit;
    int16_t AMK_NegativeTorqueLimit;
    // End signal defs
} INVA_SET_data_t;

static constexpr uint32_t INVB_SET_MSG_ID = 0x112;
static constexpr uint8_t  INVB_SET_DLC = 8;
static constexpr uint64_t INVB_SET_LAYOUT_HASH = 0xB379B31C5463DFD6;
static constexpr uint32_t INVB_SET_PERIOD_MS = 15;
static constexpr uint32_t INVB_SET_STALE_TIMEOUT_MS = 37;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t AMK_Control_bReserve;
    bool AMK_Control_bInverterOn;
    bool AMK_Control_bDcOn;
    bool AMK_Control_bEnable;
    bool AMK_Control_bErrorReset;
    uint8_t AMK_Control_bReserve2;
    int16_t AMK_TorqueSetpoint;
    int16_t AMK_PositiveTorqueLimit;
    int16_t AMK_NegativeTorqueLimit;
    // End signal defs
} INVB_SET_data_t;

static constexpr uint32_t INVC_SET_MSG_ID = 0x113;
static constexpr uint8_t  INVC_SET_DLC = 8;
static constexpr uint64_t INVC_SET_LAYOUT_HASH = 0xB379B31C5463DFD6;
static constexpr uint32_t INVC_SET_PERIOD_MS = 15;
static constexpr uint32_t INVC_SET_STALE_TIMEOUT_MS = 37;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t AMK_Control_bReserve;
    bool AMK_Control_bInverterOn;
    bool AMK_Control_bDcOn;
    bool AMK_Control_bEnable;
    bool AMK_Control_bErrorReset;
    uint8_t AMK_Control_bReserve2;
    int16_t AMK_TorqueSetpoint;
    int16_t AMK_PositiveTorqueLimit;
    int16_t AMK_NegativeTorqueLimit;
    // End signal defs
} INVC_SET_data_t;

static constexpr uint32_t INVD_SET_MSG_ID = 0x114;
static constexpr uint8_t  INVD_SET_DLC = 8;
static constexpr uint64_t INVD_SET_LAYOUT_HASH = 0xB379B31C5463DFD6;
static constexpr uint32_t INVD_SET_PERIOD_MS = 15;
static constexpr uint32_t INVD_SET_STALE_TIMEOUT_MS = 37;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t AMK_Control_bReserve;
    bool AMK_Control_bInverterOn;
    bool AMK_Control_bDcOn;
    bool AMK_Control_bEnable;
    bool AMK_Control_bErrorReset;
    uint8_t AMK_Control_bReserve2;
    int16_t AMK_TorqueSetpoint;
    int16_t AMK_PositiveTorqueLimit;
    int16_t AMK_NegativeTorqueLimit;
    // End signal defs
} INVD_SET_data_t;

static constexpr uint32_t INVA_CRIT_MSG_ID = 0x121;
static constexpr uint8_t  INVA_CRIT_DLC = 8;
static constexpr uint64_t INVA_CRIT_LAYOUT_HASH = 0xF79F3844F862C8B4;
static constexpr uint32_t INVA_CRIT_PERIOD_MS = 15;
static constexpr uint32_t INVA_CRIT_STALE_TIMEOUT_MS = 37;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t AMK_ActualSpeed;
    int16_t AMK_ActualTorque;
    uint16_t AMK_DisplayOverloadInverter;
    uint16_t AMK_DisplayOverloadMotor;
    // End signal defs
} INVA_CRIT_data_t;

static constexpr uint32_t INVB_CRIT_MSG_ID = 0x122;
static constexpr uint8_t  INVB_CRIT_DLC = 8;
static constexpr uint64_t INVB_CRIT_LAYOUT_HASH = 0xF79F3844F862C8B4;
static constexpr uint32_t INVB_CRIT_PERIOD_MS = 15;
static constexpr uint32_t INVB_CRIT_STALE_TIMEOUT_MS = 37;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t AMK_ActualSpeed;
    int16_t AMK_ActualTorque;
    uint16_t AMK_DisplayOverloadInverter;
    uint16_t AMK_DisplayOverloadMotor;
    // End signal defs
} INVB_CRIT_data_t;

static constexpr uint32_t INVC_CRIT_MSG_ID = 0x123;
static constexpr uint8_t  INVC_CRIT_DLC = 8;
static constexpr uint64_t INVC_CRIT_LAYOUT_HASH = 0xF79F3844F862C8B4;
static constexpr uint32_t INVC_CRIT_PERIOD_MS = 15;
static constexpr uint32_t INVC_CRIT_STALE_TIMEOUT_MS = 37;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t AMK_ActualSpeed;
    int16_t AMK_ActualTorque;
    uint16_t AMK_DisplayOverloadInverter;
    uint16_t AMK_DisplayOverloadMotor;
    // End signal defs
} INVC_CRIT_data_t;

static constexpr uint32_t INVD_CRIT_MSG_ID = 0x124;
static constexpr uint8_t  INVD_CRIT_DLC = 8;
static constexpr uint64_t INVD_CRIT_LAYOUT_HASH = 0xF79F3844F862C8B4;
static constexpr uint32_t INVD_CRIT_PERIOD_MS = 15;
static constexpr uint32_t INVD_CRIT_STALE_TIMEOUT_MS = 37;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t AMK_ActualSpeed;
    int16_t AMK_ActualTorque;
    uint16_t AMK_DisplayOverloadInverter;
    uint16_t AMK_DisplayOverloadMotor;
    // End signal defs
} INVD_CRIT_data_t;

static constexpr uint32_t INVA_INFO_MSG_ID = 0x231;
static constexpr uint8_t  INVA_INFO_DLC = 4;
static constexpr uint64_t INVA_INFO_LAYOUT_HASH = 0x1993D502386922E9;
static constexpr uint32_t INVA_INFO_PERIOD_MS = 500;
static constexpr uint32_t INVA_INFO_STALE_TIMEOUT_MS = 1250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t AMK_Status_bReserve;
    bool AMK_Status_bSystemReady;
    bool AMK_Status_bError;
    bool AMK_Status_bWarn;
    bool AMK_Status_bQuitDcOn;
    bool AMK_Status_bDcOn;
    bool AMK_Status_bQuitInverterOn;
    bool AMK_Status_bInverterOn;
    bool AMK_Status_bDerating;
    uint16_t AMK_DCBusVoltage;
    // End signal defs
} INVA_INFO_data_t;

static constexpr uint32_t INVB_INFO_MSG_ID = 0x232;
static constexpr uint8_t  INVB_INFO_DLC = 4;
static constexpr uint64_t INVB_INFO_LAYOUT_HASH = 0x1993D502386922E9;
static constexpr uint32_t INVB_INFO_PERIOD_MS = 500;
static constexpr uint32_t INVB_INFO_STALE_TIMEOUT_MS = 1250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t AMK_Status_bReserve;
    bool AMK_Status_bSystemReady;
    bool AMK_Status_bError;
    bool AMK_Status_bWarn;
    bool AMK_Status_bQuitDcOn;
    bool AMK_Status_bDcOn;
    bool AMK_Status_bQuitInverterOn;
    bool AMK_Status_bInverterOn;
    bool AMK_Status_bDerating;
    uint16_t AMK_DCBusVoltage;
    // End signal defs
} INVB_INFO_data_t;

static constexpr uint32_t INVC_INFO_MSG_ID = 0x233;
static constexpr uint8_t  INVC_INFO_DLC = 4;
static constexpr uint64_t INVC_INFO_LAYOUT_HASH = 0x1993D502386922E9;
static constexpr uint32_t INVC_INFO_PERIOD_MS = 500;
static constexpr uint32_t INVC_INFO_STALE_TIMEOUT_MS = 1250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t AMK_Status_bReserve;
    bool AMK_Status_bSystemReady;
    bool AMK_Status_bError;
    bool AMK_Status_bWarn;
    bool AMK_Status_bQuitDcOn;
    bool AMK_Status_bDcOn;
    bool AMK_Status_bQuitInverterOn;
    bool AMK_Status_bInverterOn;
    bool AMK_Status_bDerating;
    uint16_t AMK_DCBusVoltage;
    // End signal defs
} INVC_INFO_data_t;

static constexpr uint32_t INVD_INFO_MSG_ID = 0x234;
static constexpr uint8_t  INVD_INFO_DLC = 4;
static constexpr uint64_t INVD_INFO_LAYOUT_HASH = 0x1993D502386922E9;
static constexpr uint32_t INVD_INFO_PERIOD_MS = 500;
static constexpr uint32_t INVD_INFO_STALE_TIMEOUT_MS = 1250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t AMK_Status_bReserve;
    bool AMK_Status_bSystemReady;
    bool AMK_Status_bError;
    bool AMK_Status_bWarn;
    bool AMK_Status_bQuitDcOn;
    bool AMK_Status_bDcOn;
    bool AMK_Status_bQuitInverterOn;
    bool AMK_Status_bInverterOn;
    bool AMK_Status_bDerating;
    uint16_t AMK_DCBusVoltage;
    // End signal defs
} INVD_INFO_data_t;

static constexpr uint32_t MAIN_HB_AMK_MSG_ID = 0x235;
static constexpr uint8_t  MAIN_HB_AMK_DLC = 2;
static constexpr uint64_t MAIN_HB_AMK_LAYOUT_HASH = 0xA17A8B8581087A09;
static constexpr uint32_t MAIN_HB_AMK_PERIOD_MS = 500;
static constexpr uint32_t MAIN_HB_AMK_STALE_TIMEOUT_MS = 1250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint8_t car_state;
    bool precharge_state;
    // End signal defs
} main_hb_amk_data_t;

static constexpr uint32_t INVA_ERR_1_MSG_ID = 0x441;
static constexpr uint8_t  INVA_ERR_1_DLC = 8;
static constexpr uint64_t INVA_ERR_1_LAYOUT_HASH = 0x6843A6076F5B46DF;
static constexpr uint32_t INVA_ERR_1_PERIOD_MS = 2000;
static constexpr uint32_t INVA_ERR_1_STALE_TIMEOUT_MS = 5000;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t AMK_DiagnosticNumber;
    uint32_t AMK_ErrorInfo1;
    // End signal defs
} INVA_ERR_1_data_t;

static constexpr uint32_t INVB_ERR_1_MSG_ID = 0x442;
static constexpr uint8_t  INVB_ERR_1_DLC = 8;
static constexpr uint64_t INVB_ERR_1_LAYOUT_HASH = 0x6843A6076F5B46DF;
static constexpr uint32_t INVB_ERR_1_PERIOD_MS = 2000;
static constexpr uint32_t INVB_ERR_1_STALE_TIMEOUT_MS = 5000;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t AMK_DiagnosticNumber;
    uint32_t AMK_ErrorInfo1;
    // End signal defs
} INVB_ERR_1_data_t;

static constexpr uint32_t INVC_ERR_1_MSG_ID = 0x443;
static constexpr uint8_t  INVC_ERR_1_DLC = 8;
static constexpr uint64_t INVC_ERR_1_LAYOUT_HASH = 0x6843A6076F5B46DF;
static constexpr uint32_t INVC_ERR_1_PERIOD_MS = 2000;
static constexpr uint32_t INVC_ERR_1_STALE_TIMEOUT_MS = 5000;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t AMK_DiagnosticNumber;
    uint32_t AMK_ErrorInfo1;
    // End signal defs
} INVC_ERR_1_data_t;

static constexpr uint32_t INVD_ERR_1_MSG_ID = 0x444;
static constexpr uint8_t  INVD_ERR_1_DLC = 8;
static constexpr uint64_t INVD_ERR_1_LAYOUT_HASH = 0x6843A6076F5B46DF;
static constexpr uint32_t INVD_ERR_1_PERIOD_MS = 2000;
static constexpr uint32_t INVD_ERR_1_STALE_TIMEOUT_MS = 5000;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t AMK_DiagnosticNumber;
    uint32_t AMK_ErrorInfo1;
    // End signal defs
} INVD_ERR_1_data_t;

static constexpr uint32_t INVA_ERR_2_MSG_ID = 0x451;
static constexpr uint8_t  INVA_ERR_2_DLC = 8;
static constexpr uint64_t INVA_ERR_2_LAYOUT_HASH = 0x427028F8861EF493;
static constexpr uint32_t INVA_ERR_2_PERIOD_MS = 2000;
static constexpr uint32_t INVA_ERR_2_STALE_TIMEOUT_MS = 5000;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t AMK_ErrorInfo2;
    uint32_t AMK_ErrorInfo3;
    // End signal defs
} INVA_ERR_2_data_t;

static constexpr uint32_t INVB_ERR_2_MSG_ID = 0x452;
static constexpr uint8_t  INVB_ERR_2_DLC = 8;
static constexpr uint64_t INVB_ERR_2_LAYOUT_HASH = 0x427028F8861EF493;
static constexpr uint32_t INVB_ERR_2_PERIOD_MS = 2000;
static constexpr uint32_t INVB_ERR_2_STALE_TIMEOUT_MS = 5000;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t AMK_ErrorInfo2;
    uint32_t AMK_ErrorInfo3;
    // End signal defs
} INVB_ERR_2_data_t;

static constexpr uint32_t INVC_ERR_2_MSG_ID = 0x453;
static constexpr uint8_t  INVC_ERR_2_DLC = 8;
static constexpr uint64_t INVC_ERR_2_LAYOUT_HASH = 0x427028F8861EF493;
static constexpr uint32_t INVC_ERR_2_PERIOD_MS = 2000;
static constexpr uint32_t INVC_ERR_2_STALE_TIMEOUT_MS = 5000;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t AMK_ErrorInfo2;
    uint32_t AMK_ErrorInfo3;
    // End signal defs
} INVC_ERR_2_data_t;

static constexpr uint32_t INVD_ERR_2_MSG_ID = 0x454;
static constexpr uint8_t  INVD_ERR_2_DLC = 8;
static constexpr uint64_t INVD_ERR_2_LAYOUT_HASH = 0x427028F8861EF493;
static constexpr uint32_t INVD_ERR_2_PERIOD_MS = 2000;
static constexpr uint32_t INVD_ERR_2_STALE_TIMEOUT_MS = 5000;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    uint32_t AMK_ErrorInfo2;
    uint32_t AMK_ErrorInfo3;
    // End signal defs
} INVD_ERR_2_data_t;

static constexpr uint32_t INVA_TEMPS_MSG_ID = 0x461;
static constexpr uint8_t  INVA_TEMPS_DLC = 6;
static constexpr uint64_t INVA_TEMPS_LAYOUT_HASH = 0x7CDCF5453F9E646D;
static constexpr uint32_t INVA_TEMPS_PERIOD_MS = 500;
static constexpr uint32_t INVA_TEMPS_STALE_TIMEOUT_MS = 1250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t AMK_MotorTemp;
    int16_t AMK_InverterTemp;
    int16_t AMK_IGBTTemp;
    // End signal defs
} INVA_TEMPS_data_t;

static constexpr uint32_t INVB_TEMPS_MSG_ID = 0x462;
static constexpr uint8_t  INVB_TEMPS_DLC = 6;
static constexpr uint64_t INVB_TEMPS_LAYOUT_HASH = 0x7CDCF5453F9E646D;
static constexpr uint32_t INVB_TEMPS_PERIOD_MS = 500;
static constexpr uint32_t INVB_TEMPS_STALE_TIMEOUT_MS = 1250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t AMK_MotorTemp;
    int16_t AMK_InverterTemp;
    int16_t AMK_IGBTTemp;
    // End signal defs
} INVB_TEMPS_data_t;

static constexpr uint32_t INVC_TEMPS_MSG_ID = 0x463;
static constexpr uint8_t  INVC_TEMPS_DLC = 6;
static constexpr uint64_t INVC_TEMPS_LAYOUT_HASH = 0x7CDCF5453F9E646D;
static constexpr uint32_t INVC_TEMPS_PERIOD_MS = 500;
static constexpr uint32_t INVC_TEMPS_STALE_TIMEOUT_MS = 1250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t AMK_MotorTemp;
    int16_t AMK_InverterTemp;
    int16_t AMK_IGBTTemp;
    // End signal defs
} INVC_TEMPS_data_t;

static constexpr uint32_t INVD_TEMPS_MSG_ID = 0x464;
static constexpr uint8_t  INVD_TEMPS_DLC = 6;
static constexpr uint64_t INVD_TEMPS_LAYOUT_HASH = 0x7CDCF5453F9E646D;
static constexpr uint32_t INVD_TEMPS_PERIOD_MS = 500;
static constexpr uint32_t INVD_TEMPS_STALE_TIMEOUT_MS = 1250;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int16_t AMK_MotorTemp;
    int16_t AMK_InverterTemp;
    int16_t AMK_IGBTTemp;
    // End signal defs
} INVD_TEMPS_data_t;

static constexpr uint32_t INVA_PHASE_I_MSG_ID = 0x571;
static constexpr uint8_t  INVA_PHASE_I_DLC = 4;
static constexpr uint64_t INVA_PHASE_I_LAYOUT_HASH = 0x3278208CCB68C64C;
static constexpr uint32_t INVA_PHASE_I_PERIOD_MS = 250;
static constexpr uint32_t INVA_PHASE_I_STALE_TIMEOUT_MS = 625;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int32_t AMK_PhaseUCurrent;
    // End signal defs
} INVA_PHASE_I_data_t;

static constexpr uint32_t INVB_PHASE_I_MSG_ID = 0x572;
static constexpr uint8_t  INVB_PHASE_I_DLC = 4;
static constexpr uint64_t INVB_PHASE_I_LAYOUT_HASH = 0x3278208CCB68C64C;
static constexpr uint32_t INVB_PHASE_I_PERIOD_MS = 250;
static constexpr uint32_t INVB_PHASE_I_STALE_TIMEOUT_MS = 625;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int32_t AMK_PhaseUCurrent;
    // End signal defs
} INVB_PHASE_I_data_t;

static constexpr uint32_t INVC_PHASE_I_MSG_ID = 0x573;
static constexpr uint8_t  INVC_PHASE_I_DLC = 4;
static constexpr uint64_t INVC_PHASE_I_LAYOUT_HASH = 0x3278208CCB68C64C;
static constexpr uint32_t INVC_PHASE_I_PERIOD_MS = 250;
static constexpr uint32_t INVC_PHASE_I_STALE_TIMEOUT_MS = 625;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int32_t AMK_PhaseUCurrent;
    // End signal defs
} INVC_PHASE_I_data_t;

static constexpr uint32_t INVD_PHASE_I_MSG_ID = 0x574;
static constexpr uint8_t  INVD_PHASE_I_DLC = 4;
static constexpr uint64_t INVD_PHASE_I_LAYOUT_HASH = 0x3278208CCB68C64C;
static constexpr uint32_t INVD_PHASE_I_PERIOD_MS = 250;
static constexpr uint32_t INVD_PHASE_I_STALE_TIMEOUT_MS = 625;

typedef struct {
    uint32_t last_rx;
    bool (*is_stale)(void); // Pointer to stale checking function
    // Begin signal defs
    int32_t AMK_PhaseUCurrent;
    // End signal defs
} INVD_PHASE_I_data_t;


#endif // MCAN_H
