/**
 * @file usb_device.h
 * @brief USB tx thread
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "common/phal_G4/usb/usb.h"
#include "common/timestamped_frame/timestamped_frame.h"

static constexpr size_t MAX_FRAMES_PER_PACKET = 4U;
static_assert(MAX_FRAMES_PER_PACKET * sizeof(timestamped_frame_t) == PHAL_USB_PACKET_SIZE_BYTES);

/**
 * @brief CAN frame structure for USB -> CAN transmission
 */
typedef struct {
    uint32_t can_id;        // 11-bit standard or 29-bit extended CAN ID
    uint8_t bus_id;         // Target bus ID (VCAN_BUS_ID, MCAN_BUS_ID, CCAN_BUS_ID, SCAN_BUS_ID)
    uint8_t dlc;            // Payload length in bytes (0..8)
    uint8_t is_extended_id; // 0 = standard 11-bit ID, 1 = extended 29-bit ID
    uint8_t reserved;       // Padding for 4-byte alignment (0)
    uint8_t data[8];        // Frame payload
} can_tx_frame_t;
void usb_tx_periodic(void);

#endif // USB_DEVICE_H
