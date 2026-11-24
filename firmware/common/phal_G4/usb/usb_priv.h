/**
 * @file usb_priv.h
 * @brief Register-level interface used by the STM32G4 USB public layer.
 * @author Ronak Jain (jain717@purdue.edu)
 *
 * These functions assume that the public layer has validated endpoint numbers,
 * directions, pointers, packet lengths, and peripheral lifecycle state.
 */

#ifndef PHAL_G4_USB_PRIV_H
#define PHAL_G4_USB_PRIV_H

#include <stdint.h>

#include "phal_G4/usb/usb.h"

/**
 * @brief Enable and reset the USB hardware, configure pins and packet memory.
 * @return true once HSI48 is ready and initialization completes.
 */
bool USB_PRIV_init(void);

/** @brief Detach, disable interrupts, and power down the USB peripheral. */
void USB_PRIV_deinit(void);

/** @brief Enable USB event interrupts and endpoint operation. */
void USB_PRIV_enableInterrupts(void);

/** @brief Attach to the bus by enabling the internal D+ pull-up. */
void USB_PRIV_connect(void);

/** @brief Write the seven-bit device address while keeping the device enabled. */
void USB_PRIV_setAddress(uint8_t address);

/** @brief Set one endpoint direction to the USB STALL handshake state. */
void USB_PRIV_stall(uint8_t endpoint, PHAL_USB_EndpointDirection_t direction);

/** @brief Discard a transfer, clear STALL, and reset the direction to DATA0. */
void USB_PRIV_clearHalt(uint8_t endpoint, PHAL_USB_EndpointDirection_t direction);

/**
 * @brief Copy one IN packet to packet memory and make it available to the host.
 * @return true when the endpoint was idle and accepted the packet.
 */
bool USB_PRIV_write(uint8_t endpoint, const void *source, uint16_t length_bytes);

/**
 * @brief Copy one completed OUT packet from packet memory and rearm reception.
 *
 * The received length is reported even when the destination is too small; in
 * that case the packet remains pending and the OUT endpoint is not rearmed.
 *
 * @return true when a complete packet fit in the destination and was consumed.
 */
bool USB_PRIV_read(uint8_t endpoint,
                   void *destination,
                   uint16_t destination_capacity,
                   uint16_t *received_length);

/** @brief Decode one pending USB interrupt and notify the application. */
void USB_PRIV_handleInterrupt(void);

/**
 * @brief Read the USB peripheral's current 11-bit frame number.
 * @return The frame number maintained from Start-of-Frame packets.
 */
uint16_t USB_PRIV_getFrameNumber(void);

#endif // PHAL_G4_USB_PRIV_H
