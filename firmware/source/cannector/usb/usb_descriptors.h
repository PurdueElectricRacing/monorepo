/**
 * @file usb_descriptors.h
 * @brief CANnector's USB identity, describe the device to the host.
 *
 * Descriptors are byte arrays in the USB wire format.
 * Multibyte values are little endian.
 */
#ifndef CANNECTOR_USB_DESCRIPTORS_H
#define CANNECTOR_USB_DESCRIPTORS_H

#include "common/phal_G4/usb/usb.h"
#include "can_library/generated/can_version.h"

static const char USB_PRODUCT_NAME[] = "CANnector27_" FIRMWARE_GIT_HASH_STRING;
static constexpr uint8_t USB_PRODUCT_STRING_INDEX = 1U;
static constexpr uint16_t USB_LANGUAGE_ID = 0x0409U; // English (United States)
static const uint8_t LANGUAGE_DESCRIPTOR[] = {
    4U, PHAL_USB_DESCRIPTOR_STRING, 0x09U, 0x04U,
};
// Keep the complete UTF-16LE descriptor within the HAL's single-packet limit.
static_assert(2U + 2U * (sizeof(USB_PRODUCT_NAME) - 1U) <= PHAL_USB_PACKET_SIZE_BYTES);

// IN/OUT are named from the host's perspective: endpoint 1
static constexpr uint8_t BULK_IN_ADDRESS = PHAL_USB_ENDPOINT_ADDRESS_IN | PHAL_USB_DATA_ENDPOINT;
static constexpr uint8_t BULK_OUT_ADDRESS = PHAL_USB_DATA_ENDPOINT;

/**
 * Device identity:
 * 1. USB version: 2.0.
 * 2. Device class: vendor-specific (the daqcore driver knows our format)
 * 3. Control packet size: 64 bytes.
 * 4. IDs (vendor:product): 5045:0271
 *    Unregistered; replace with allocated IDs before distribution.
 * 5. Device version: 0.01.
 * 6. Product string includes the build's Git hash; no manufacturer or serial string.
 * 7. One available configuration.
 */
static const uint8_t DEVICE_DESCRIPTOR[] = {
    PHAL_USB_DEVICE_DESCRIPTOR_LENGTH,
    PHAL_USB_DESCRIPTOR_DEVICE,
    0x00U, 0x02U,                         // bcdUSB: USB 2.0
    PHAL_USB_DEVICE_CLASS_VENDOR_SPECIFIC,
    0U,                                  // Device subclass
    0U,                                  // Device protocol
    PHAL_USB_PACKET_SIZE_BYTES,          // EP0 maximum packet size
    0x45U, 0x50U,                        // Vendor ID: 0x5045 ("PE")
    0x71U, 0x02U,                        // Product ID: 0x0271
    0x01U, 0x00U,                        // bcdDevice: 0.01
    0U,                                  // No manufacturer string
    USB_PRODUCT_STRING_INDEX,            // Product string
    0U,                                  // No serial-number string
    1U,                                  // Number of configurations
};

/**
 * Configuration 1 is the CAN-to-USB bridge.
 * Configuration 0 disables it.
 * One interface (0), with one alternate setting (0), advertises bulk IN (0x81) and bulk OUT (0x01).
 */
static const uint8_t CONFIGURATION_DESCRIPTOR[] = {
    // Configuration descriptor: 9 bytes
    PHAL_USB_CONFIGURATION_DESCRIPTOR_LENGTH,
    PHAL_USB_DESCRIPTOR_CONFIGURATION,
    32U, 0U,                             // Total length: 9 + 9 + 7 + 7 bytes
    1U,                                  // Number of interfaces
    1U,                                  // Configuration value selected by host
    0U,                                  // No configuration string
    PHAL_USB_CONFIGURATION_ATTRIBUTES_BUS_POWERED,
    50U,                                 // Maximum power: 50 * 2 mA = 100 mA

    // Interface descriptor: 9 bytes
    // One function with no alternate modes
    PHAL_USB_INTERFACE_DESCRIPTOR_LENGTH,
    PHAL_USB_DESCRIPTOR_INTERFACE,
    0U,                                  // Interface number
    0U,                                  // Alternate setting
    2U,                                  // Number of endpoints (excluding EP0)
    PHAL_USB_DEVICE_CLASS_VENDOR_SPECIFIC,
    0U,                                  // Interface subclass
    0U,                                  // Interface protocol
    0U,                                  // No interface string

    // Endpoint descriptor: 7 bytes (Bulk IN)
    PHAL_USB_ENDPOINT_DESCRIPTOR_LENGTH,
    PHAL_USB_DESCRIPTOR_ENDPOINT,
    BULK_IN_ADDRESS,
    PHAL_USB_ENDPOINT_TRANSFER_TYPE_BULK,
    PHAL_USB_PACKET_SIZE_BYTES, 0U,      // Maximum packet size: 64 bytes
    0U,                                  // Polling interval unused for bulk IN

    // Endpoint descriptor: 7 bytes (Bulk OUT)
    PHAL_USB_ENDPOINT_DESCRIPTOR_LENGTH,
    PHAL_USB_DESCRIPTOR_ENDPOINT,
    BULK_OUT_ADDRESS,
    PHAL_USB_ENDPOINT_TRANSFER_TYPE_BULK,
    PHAL_USB_PACKET_SIZE_BYTES, 0U,      // Maximum packet size: 64 bytes
    0U,                                  // Polling interval unused for bulk OUT
};

static_assert(sizeof(DEVICE_DESCRIPTOR) == PHAL_USB_DEVICE_DESCRIPTOR_LENGTH);
static_assert(sizeof(CONFIGURATION_DESCRIPTOR) == 32U);

#endif
