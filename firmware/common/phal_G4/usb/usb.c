/**
 * @file usb.c
 * @brief Validation and state for the public STM32G4 USB API.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#include "phal_G4/usb/usb.h"

#include "phal_G4/usb/usb_priv.h"

static bool g_initialized = false;

static bool usb_endpoint_is_valid(uint8_t endpoint) {
    return endpoint == PHAL_USB_CONTROL_ENDPOINT || endpoint == PHAL_USB_DATA_ENDPOINT;
}

static bool usb_direction_is_valid(PHAL_USB_EndpointDirection_t direction) {
    return direction == PHAL_USB_ENDPOINT_DIRECTION_OUT
        || direction == PHAL_USB_ENDPOINT_DIRECTION_IN;
}

bool PHAL_USB_init(void) {
    if (g_initialized || !USB_PRIV_init()) {
        return false;
    }

    g_initialized = true;
    USB_PRIV_enableInterrupts();
    return true;
}

bool PHAL_USB_deinit(void) {
    if (!g_initialized) {
        return false;
    }

    USB_PRIV_deinit();
    g_initialized = false;
    return true;
}

bool PHAL_USB_connect(void) {
    if (!g_initialized) {
        return false;
    }

    USB_PRIV_connect();
    return true;
}

bool PHAL_USB_setAddress(uint8_t address) {
    if (!g_initialized || address > 127U) {
        return false;
    }

    USB_PRIV_setAddress(address);
    return true;
}

bool PHAL_USB_stall(uint8_t endpoint, PHAL_USB_EndpointDirection_t direction) {
    if (!g_initialized || !usb_endpoint_is_valid(endpoint)
        || !usb_direction_is_valid(direction)) {
        return false;
    }

    USB_PRIV_stall(endpoint, direction);
    return true;
}

bool PHAL_USB_clearHalt(uint8_t endpoint, PHAL_USB_EndpointDirection_t direction) {
    if (!g_initialized || !usb_endpoint_is_valid(endpoint) || !usb_direction_is_valid(direction)) {
        return false;
    }
    USB_PRIV_clearHalt(endpoint, direction);
    return true;
}

bool PHAL_USB_write(uint8_t endpoint, const void *source, uint16_t length_bytes) {
    if (!g_initialized || !usb_endpoint_is_valid(endpoint)
        || length_bytes > PHAL_USB_PACKET_SIZE_BYTES
        || (source == nullptr && length_bytes != 0U)) {
        return false;
    }

    return USB_PRIV_write(endpoint, source, length_bytes);
}

bool PHAL_USB_read(uint8_t endpoint,
                   void *destination,
                   uint16_t destination_capacity,
                   uint16_t *received_length) {
    if (!g_initialized || !usb_endpoint_is_valid(endpoint)
        || received_length == nullptr
        || (destination == nullptr && destination_capacity != 0U)) {
        return false;
    }

    return USB_PRIV_read(endpoint, destination, destination_capacity, received_length);
}

uint16_t PHAL_USB_getFrameNumber(void) {
    return USB_PRIV_getFrameNumber();
}

[[gnu::weak]]
void PHAL_USB_callback(const PHAL_USB_Event_t *event) {
    (void)event;
}

void USB_LP_IRQHandler(void) {
    USB_PRIV_handleInterrupt();
}

void USB_HP_IRQHandler(void) {
    USB_PRIV_handleInterrupt();
}
