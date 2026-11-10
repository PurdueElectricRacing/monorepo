/**
 * @file usb_control.h
 * @brief Interface between USB control handling and CAN forwarding
 */
#ifndef CANNECTOR_USB_CONTROL_H
#define CANNECTOR_USB_CONTROL_H

#include "common/phal_G4/usb/usb.h"

// Zero is a valid address/configuration
// UINT16_MAX is an out-of-range pending sentinel
static constexpr uint16_t NO_PENDING_VALUE = UINT16_MAX;


typedef struct {
    volatile bool configured;       // The host has selected configuration 1 and completed its acknowledgement
    volatile bool bulk_in_halted;   // The host has halted the CAN bulk IN endpoint
    volatile bool bulk_out_halted;  // The host has halted the CAN bulk OUT endpoint
    uint8_t address;                // The USB address assigned by the host
    uint16_t pending_address;       // An address change waiting for acknowledgement
    uint16_t pending_configuration; // A configuration change waiting for acknowledgement
} usb_context_t;


// Check configured/halted state.
// The caller must exclude USB interrupts through the subsequent packet submission
// to avoid racing reset/deconfiguration.
bool usb_data_enabled(void);


#endif
