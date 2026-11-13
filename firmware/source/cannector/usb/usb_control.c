/**
 * @file usb_control.c
 * @brief Host enumeration and endpoint-zero control transfers (USB ISR context)
 */

#include "usb_control.h"

#include <string.h>

#include "usb_device.h"
#include "usb_descriptors.h"


static usb_context_t g_usb_context = {
    .configured            = false,
    .bulk_in_halted        = false,
    .bulk_out_halted       = false,
    .address               = 0U,
    .pending_address       = NO_PENDING_VALUE,
    .pending_configuration = NO_PENDING_VALUE,
};


// Reject unsupported/malformed requests. A new SETUP recovers control EP0.
static void stall_control(void) {
    g_usb_context.pending_address = NO_PENDING_VALUE;
    g_usb_context.pending_configuration = NO_PENDING_VALUE;
    PHAL_USB_stall(PHAL_USB_CONTROL_ENDPOINT, PHAL_USB_ENDPOINT_DIRECTION_IN);
    PHAL_USB_stall(PHAL_USB_CONTROL_ENDPOINT, PHAL_USB_ENDPOINT_DIRECTION_OUT);
}

static void respond(const void *data, uint16_t size, uint16_t requested) {
    // The host may ask for just the beginning of a descriptor
    uint16_t response_length = size < requested ? size : requested;
    
    if (!PHAL_USB_write(PHAL_USB_CONTROL_ENDPOINT, data, response_length)) {
        stall_control();
    }
}

static bool valid_endpoint(uint16_t index) {
    if (g_usb_context.address == 0U) {
        return false;
    }

    // EP0 supports both directions.
    // Bulk IN and bulk OUT are advertised for application data.
    if (index == 0U || index == PHAL_USB_ENDPOINT_ADDRESS_IN) {
        return true;
    }
    return g_usb_context.configured && (index == BULK_IN_ADDRESS || index == BULK_OUT_ADDRESS);
}

static bool handle_get_descriptor(const PHAL_USB_SetupPacket_t *setup) {
    // wValue holds descriptor type in its high byte and descriptor index in its low byte.
    uint16_t descriptor_type = setup->value >> 8U;
    uint8_t descriptor_index = setup->value & 0xFFU;
    if (setup->type == PHAL_USB_REQUEST_TYPE_DEVICE_IN &&
        descriptor_type == PHAL_USB_DESCRIPTOR_STRING && setup->length != 0U) {
        if (descriptor_index == 0U && setup->index == 0U) {
            respond(LANGUAGE_DESCRIPTOR, sizeof(LANGUAGE_DESCRIPTOR), setup->length);
            return true;
        }
        if (descriptor_index == USB_PRODUCT_STRING_INDEX && setup->index == USB_LANGUAGE_ID) {
            static uint8_t product_descriptor[2U + 2U * (sizeof(USB_PRODUCT_NAME) - 1U)];
            product_descriptor[0] = sizeof(product_descriptor);
            product_descriptor[1] = PHAL_USB_DESCRIPTOR_STRING;
            for (size_t i = 0U; i < sizeof(USB_PRODUCT_NAME) - 1U; ++i) {
                product_descriptor[2U + 2U * i] = (uint8_t)USB_PRODUCT_NAME[i];
                product_descriptor[3U + 2U * i] = 0U;
            }
            respond(product_descriptor, sizeof(product_descriptor), setup->length);
            return true;
        }
        return false;
    }
    bool is_device_in = setup->type == PHAL_USB_REQUEST_TYPE_DEVICE_IN;
    bool is_index_zero = setup->index == 0U;
    bool is_nonzero_length = setup->length != 0U;
    bool is_descriptor_index_zero = (setup->value & 0xFFU) == 0U;
    if (is_device_in &&
        is_index_zero &&
        is_descriptor_index_zero &&
        is_nonzero_length
    ) {
        if (descriptor_type == PHAL_USB_DESCRIPTOR_DEVICE) {
            respond(DEVICE_DESCRIPTOR, sizeof(DEVICE_DESCRIPTOR), setup->length);
            return true;
        }
        if (descriptor_type == PHAL_USB_DESCRIPTOR_CONFIGURATION) {
            respond(CONFIGURATION_DESCRIPTOR, sizeof(CONFIGURATION_DESCRIPTOR), setup->length);
            return true;
        }
    }
    return false;
}

static bool handle_set_address(const PHAL_USB_SetupPacket_t *setup) {
    bool is_device_out = setup->type == PHAL_USB_REQUEST_TYPE_DEVICE_OUT;
    bool is_index_zero = setup->index == 0U;
    bool is_length_zero = setup->length == 0U;
    bool valid_address = setup->value <= 127U;
    if (is_device_out &&
        is_index_zero &&
        is_length_zero &&
        valid_address &&
        !g_usb_context.configured
    ) {
        // The host still expects the status packet at the old address.
        // Apply the new address from handle_control_in() after that packet completes.
        g_usb_context.pending_address = setup->value;
        respond(nullptr, 0U, 0U);
        return true;
    }
    return false;
}

static bool handle_get_configuration(const PHAL_USB_SetupPacket_t *setup) {
    bool is_device_in = setup->type == PHAL_USB_REQUEST_TYPE_DEVICE_IN;
    bool is_index_zero = setup->index == 0U;
    bool is_value_zero = setup->value == 0U;
    if (is_device_in &&
        is_index_zero &&
        is_value_zero &&
        setup->length == 1U &&
        g_usb_context.address != 0U
    ) {
        uint8_t value = g_usb_context.configured ? 1U : 0U;
        respond(&value, sizeof(value), setup->length);
        return true;
    }
    return false;
}

static bool handle_set_configuration(const PHAL_USB_SetupPacket_t *setup) {
    bool is_device_out = setup->type == PHAL_USB_REQUEST_TYPE_DEVICE_OUT;
    bool is_index_zero = setup->index == 0U;
    bool is_length_zero = setup->length == 0U;
    if (is_device_out &&
        is_index_zero &&
        is_length_zero &&
        setup->value <= 1U
        && g_usb_context.address != 0U
    ) {
        // Stop submissions until the configuration status stage completes.
        g_usb_context.configured = false;
        g_usb_context.pending_configuration = setup->value;
        respond(nullptr, 0U, 0U);
        return true;
    }
    return false;
}

static bool handle_get_status(const PHAL_USB_SetupPacket_t *setup) {
    if (setup->value != 0U || setup->length != 2U) {
        return false;
    }

    bool valid_recipient = false;
    switch (setup->type) {
        case PHAL_USB_REQUEST_TYPE_DEVICE_IN:
            valid_recipient = (setup->index == 0U && g_usb_context.address != 0U);
            break;
        case PHAL_USB_REQUEST_TYPE_INTERFACE_IN:
            valid_recipient = (setup->index == 0U && g_usb_context.configured);
            break;
        case PHAL_USB_REQUEST_TYPE_ENDPOINT_IN:
            valid_recipient = valid_endpoint(setup->index);
            break;
        default:
            break;
    }
    if (!valid_recipient) {
        return false;
    }

    // Device status is zero: bus-powered, no remote wakeup. Interface status
    // is also zero. Endpoint status uses bit 0 to report its halt state.
    uint8_t status[2] = {0U, 0U};
    if (setup->type == PHAL_USB_REQUEST_TYPE_ENDPOINT_IN) {
        if (setup->index == BULK_IN_ADDRESS) {
            status[0] = g_usb_context.bulk_in_halted ? 1U : 0U;
        } else if (setup->index == BULK_OUT_ADDRESS) {
            status[0] = g_usb_context.bulk_out_halted ? 1U : 0U;
        }
    }
    respond(status, sizeof(status), setup->length);
    return true;
}

static bool handle_get_interface(const PHAL_USB_SetupPacket_t *setup) {
    bool is_interface_in = setup->type == PHAL_USB_REQUEST_TYPE_INTERFACE_IN;
    bool is_index_zero = setup->index == 0U;
    bool is_value_zero = setup->value == 0U;
    if (is_interface_in &&
        g_usb_context.configured &&
        is_index_zero &&
        is_value_zero &&
        setup->length == 1U
    ) {
        // Interface 0 has only its default alternate setting.
        uint8_t alternate = 0U;
        respond(&alternate, sizeof(alternate), setup->length);
        return true;
    }
    return false;
}

// Selecting our sole alternate setting restarts the bulk pipe at DATA0.
static bool handle_set_interface(const PHAL_USB_SetupPacket_t *setup) {
    bool is_interface_out = setup->type == PHAL_USB_REQUEST_TYPE_INTERFACE_OUT;
    bool is_index_zero = setup->index == 0U;
    bool is_value_zero = setup->value == 0U;
    bool is_length_zero = setup->length == 0U;
    if (is_interface_out &&
        g_usb_context.configured &&
        is_index_zero &&
        is_value_zero &&
        is_length_zero
    ) {
        PHAL_USB_clearHalt(PHAL_USB_DATA_ENDPOINT, PHAL_USB_ENDPOINT_DIRECTION_IN);
        PHAL_USB_clearHalt(PHAL_USB_DATA_ENDPOINT, PHAL_USB_ENDPOINT_DIRECTION_OUT);
        g_usb_context.bulk_in_halted = false;
        g_usb_context.bulk_out_halted = false;
        respond(nullptr, 0U, 0U);
        return true;
    }
    return false;
}

// HALT is a host-controlled STALL, distinct from a temporarily busy endpoint.
// Clearing it discards any pending packet and resets the DATA0/DATA1 sequence.
static bool handle_endpoint_halt(const PHAL_USB_SetupPacket_t *setup) {
    bool is_endpoint_out = setup->type == PHAL_USB_REQUEST_TYPE_ENDPOINT_OUT;
    bool is_index_bulk_in = setup->index == BULK_IN_ADDRESS;
    bool is_index_bulk_out = setup->index == BULK_OUT_ADDRESS;
    bool is_value_halt = setup->value == PHAL_USB_FEATURE_ENDPOINT_HALT;
    bool is_length_zero = setup->length == 0U;
    if (is_endpoint_out &&
        g_usb_context.configured &&
        (is_index_bulk_in || is_index_bulk_out) &&
        is_value_halt &&
        is_length_zero
    ) {
        bool halt = setup->request == PHAL_USB_REQUEST_SET_FEATURE;
        PHAL_USB_EndpointDirection_t direction = is_index_bulk_in
            ? PHAL_USB_ENDPOINT_DIRECTION_IN
            : PHAL_USB_ENDPOINT_DIRECTION_OUT;
        if (is_index_bulk_in) {
            g_usb_context.bulk_in_halted = halt;
        } else {
            g_usb_context.bulk_out_halted = halt;
        }
        if (halt) {
            PHAL_USB_stall(PHAL_USB_DATA_ENDPOINT, direction);
        } else {
            PHAL_USB_clearHalt(PHAL_USB_DATA_ENDPOINT, direction);
        }
        respond(nullptr, 0U, 0U);
        return true;
    }
    return false;
}

// Each handler either queues its response or reports an invalid request.
static void handle_setup(const PHAL_USB_SetupPacket_t *setup) {
    bool accepted = false;
    switch (setup->request) {
        case PHAL_USB_REQUEST_GET_DESCRIPTOR:
            accepted = handle_get_descriptor(setup);
            break;
        case PHAL_USB_REQUEST_SET_ADDRESS:
            accepted = handle_set_address(setup);
            break;
        case PHAL_USB_REQUEST_GET_CONFIGURATION:
            accepted = handle_get_configuration(setup);
            break;
        case PHAL_USB_REQUEST_SET_CONFIGURATION:
            accepted = handle_set_configuration(setup);
            break;
        case PHAL_USB_REQUEST_GET_STATUS:
            accepted = handle_get_status(setup);
            break;
        case PHAL_USB_REQUEST_GET_INTERFACE:
            accepted = handle_get_interface(setup);
            break;
        case PHAL_USB_REQUEST_SET_INTERFACE:
            accepted = handle_set_interface(setup);
            break;
        case PHAL_USB_REQUEST_CLEAR_FEATURE:
        case PHAL_USB_REQUEST_SET_FEATURE:
            accepted = handle_endpoint_halt(setup);
            break;
        default:
            break;
    }
    if (!accepted) {
        stall_control();
    }
}

// OUT carries either the eight-byte SETUP header or an empty status packet.
static void handle_control_out(bool is_setup) {
    uint8_t packet[PHAL_USB_PACKET_SIZE_BYTES];
    uint16_t length = 0U;
    if (is_setup) {
        // A new SETUP aborts the preceding control request.
        g_usb_context.pending_address = NO_PENDING_VALUE;
        g_usb_context.pending_configuration = NO_PENDING_VALUE;
    }
    if (!PHAL_USB_read(PHAL_USB_CONTROL_ENDPOINT, packet, sizeof(packet), &length)) {
        stall_control();
    } else if (is_setup) {
        if (length == sizeof(PHAL_USB_SetupPacket_t)) {
            PHAL_USB_SetupPacket_t setup;
            memcpy(&setup, packet, sizeof(setup));
            handle_setup(&setup);
        } else {
            stall_control();
        }
    } else if (length != 0U) {
        stall_control(); // Control-IN status must be an empty OUT packet.
    }
}

// IN completion means the host accepted our response/status packet.
static void handle_control_in(void) {
    if (g_usb_context.pending_address != NO_PENDING_VALUE) {
        g_usb_context.address = (uint8_t)g_usb_context.pending_address;
        PHAL_USB_setAddress(g_usb_context.address);
        g_usb_context.pending_address = NO_PENDING_VALUE;
    }
    if (g_usb_context.pending_configuration != NO_PENDING_VALUE) {
        // Start each selection with a clean bulk pipe. Value 0 disables traffic;
        // value 1 enables the interface advertised in our configuration descriptor.
        PHAL_USB_clearHalt(PHAL_USB_DATA_ENDPOINT, PHAL_USB_ENDPOINT_DIRECTION_IN);
        PHAL_USB_clearHalt(PHAL_USB_DATA_ENDPOINT, PHAL_USB_ENDPOINT_DIRECTION_OUT);
        g_usb_context.bulk_in_halted = false;
        g_usb_context.bulk_out_halted = false;
        g_usb_context.configured = g_usb_context.pending_configuration == 1U;
        g_usb_context.pending_configuration = NO_PENDING_VALUE;
    }
}

static void handle_bulk_out(void) {
    cannector_tx_frame_t packet[MAX_FRAMES_PER_PACKET];
    uint16_t length = 0U;
    if (!PHAL_USB_read(PHAL_USB_DATA_ENDPOINT, packet, sizeof(packet), &length)) {
        return;
    }
    if (!g_usb_context.configured || g_usb_context.bulk_out_halted) {
        return;
    }
    if (length == 0U || (length % sizeof(cannector_tx_frame_t)) != 0U) {
        return;
    }
    usb_rx_enqueue(packet, length / sizeof(cannector_tx_frame_t));
}

void PHAL_USB_callback(const PHAL_USB_Event_t *event) {
    if (event == nullptr) {
        return;
    }
    // The HAL already restored address zero and endpoints on a bus reset.
    if (!event->transfer) {
        g_usb_context.configured = false;
        g_usb_context.bulk_in_halted = false;
        g_usb_context.bulk_out_halted = false;
        g_usb_context.address = 0U;
        g_usb_context.pending_address = NO_PENDING_VALUE;
        g_usb_context.pending_configuration = NO_PENDING_VALUE;
        return;
    }
    if (event->transfer_data.endpoint != PHAL_USB_CONTROL_ENDPOINT) {
        if (event->transfer_data.endpoint == PHAL_USB_DATA_ENDPOINT &&
            event->transfer_data.direction == PHAL_USB_ENDPOINT_DIRECTION_OUT) {
            handle_bulk_out();
        }
        return; // Bulk IN completions are retired by the HAL.
    }
    if (event->transfer_data.direction == PHAL_USB_ENDPOINT_DIRECTION_OUT) {
        handle_control_out(event->transfer_data.setup);
    } else {
        handle_control_in();
    }
}

// The TX task excludes USB interrupts while checking this and submitting data.
bool usb_bulk_in_enabled(void) {
    return g_usb_context.configured && !g_usb_context.bulk_in_halted;
}
