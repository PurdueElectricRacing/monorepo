/**
 * @file usb_test.c
 * @brief Minimal vendor bulk-echo device using the STM32G4 USB HAL.
 *
 * The HAL owns clocks, pins, packet memory, endpoint configuration, and USB
 * interrupts. The application supplies descriptors and handles the standard
 * control requests needed for enumeration in PHAL_USB_callback(). Once the host
 * selects the configuration, endpoint one echoes one bulk packet at a time.
 */

#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_USB)

#include <stdint.h>

#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/usb/usb.h"
#include "common/utils/countof.h"
#include "main.h"

static constexpr uint8_t CONFIGURATION_VALUE = 1U;
static constexpr uint16_t NO_PENDING_ADDRESS = UINT16_MAX;
static constexpr uint8_t USB_DEVICE_DESCRIPTOR_LENGTH = 18U;
static constexpr uint8_t USB_CONFIGURATION_DESCRIPTOR_LENGTH = 9U;
static constexpr uint8_t USB_CONFIGURATION_TOTAL_LENGTH = 32U;
static constexpr uint8_t USB_INTERFACE_DESCRIPTOR_LENGTH = 9U;
static constexpr uint8_t USB_ENDPOINT_DESCRIPTOR_LENGTH = 7U;
static constexpr uint8_t USB_INTERFACE_COUNT = 1U;
static constexpr uint8_t USB_ENDPOINT_COUNT = 2U;
static constexpr uint8_t USB_DEVICE_CLASS_VENDOR_SPECIFIC = 0xFFU;
static constexpr uint8_t USB_CONFIGURATION_ATTRIBUTES_BUS_POWERED = 0x80U;
static constexpr uint8_t USB_CONFIGURATION_MAX_POWER_100MA = 50U;
static constexpr uint8_t USB_ENDPOINT_TRANSFER_TYPE_BULK = 2U;
static constexpr uint8_t USB_ENDPOINT_IN_ADDRESS = 0x80U | PHAL_USB_DATA_ENDPOINT;

typedef enum : uint8_t {
    USB_REQUEST_GET_STATUS = 0U,
    USB_REQUEST_SET_ADDRESS = 5U,
    USB_REQUEST_GET_DESCRIPTOR = 6U,
    USB_REQUEST_GET_CONFIGURATION = 8U,
    USB_REQUEST_SET_CONFIGURATION = 9U,
} USB_Request_t;

typedef enum : uint8_t {
    USB_DESCRIPTOR_DEVICE = 1U,
    USB_DESCRIPTOR_CONFIGURATION = 2U,
} USB_DescriptorType_t;

/** The host sends this fixed eight-byte SETUP header for every control request. */
typedef struct __attribute__((packed)) {
    uint8_t type;      /**< Direction, request category, and recipient. */
    uint8_t request;   /**< Request selected from USB_Request_t. */
    uint16_t value;    /**< Request-specific value or descriptor type/index. */
    uint16_t index;    /**< Request-specific interface, endpoint, or language. */
    uint16_t length;   /**< Maximum number of bytes in the data stage. */
} USB_SetupPacket_t;

PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_RED_PORT, LED_RED_PIN, GPIO_OUTPUT_LOW_SPEED),
};

void HardFault_Handler(void);

// The device descriptor identifies a vendor-specific USB 2.0 device. Zero
// string indexes keep this example focused on the requests required to operate.
static const uint8_t g_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_LENGTH, USB_DESCRIPTOR_DEVICE,
    0x00U, 0x02U, // USB 2.0
    USB_DEVICE_CLASS_VENDOR_SPECIFIC, 0U, 0U, // Vendor-specific device
    PHAL_USB_PACKET_SIZE_BYTES,
    0x09U, 0x12U, // VID 0x1209
    0x03U, 0x00U, // PID 0x0003
    0x01U, 0x00U, // Device version 0.01
    0U, 0U, 0U, CONFIGURATION_VALUE,
};

// The complete configuration contains one vendor interface and the fixed bulk
// endpoint supplied by the HAL. Bit 7 of bmAttributes marks the required
// bus-powered configuration; bMaxPower is expressed in 2 mA units.
static const uint8_t g_configuration_descriptor[] = {
    USB_CONFIGURATION_DESCRIPTOR_LENGTH, USB_DESCRIPTOR_CONFIGURATION,
    USB_CONFIGURATION_TOTAL_LENGTH, 0U,
    USB_INTERFACE_COUNT, CONFIGURATION_VALUE, 0U,
    USB_CONFIGURATION_ATTRIBUTES_BUS_POWERED, USB_CONFIGURATION_MAX_POWER_100MA,

    USB_INTERFACE_DESCRIPTOR_LENGTH, 4U, 0U, 0U,
    USB_ENDPOINT_COUNT, USB_DEVICE_CLASS_VENDOR_SPECIFIC, 0U, 0U, 0U,
    // Vendor interface with two endpoints

    USB_ENDPOINT_DESCRIPTOR_LENGTH, 5U, PHAL_USB_DATA_ENDPOINT,
    USB_ENDPOINT_TRANSFER_TYPE_BULK,
    PHAL_USB_PACKET_SIZE_BYTES, 0U, 0U, // Bulk OUT endpoint

    USB_ENDPOINT_DESCRIPTOR_LENGTH, 5U, USB_ENDPOINT_IN_ADDRESS,
    USB_ENDPOINT_TRANSFER_TYPE_BULK,
    PHAL_USB_PACKET_SIZE_BYTES, 0U, 0U, // Bulk IN endpoint
};

static USB_SetupPacket_t g_setup_packet = {0};
static uint8_t g_echo_packet[PHAL_USB_PACKET_SIZE_BYTES] = {0};
static uint16_t g_pending_address = NO_PENDING_ADDRESS;
static bool g_configured = false;

/** Reject the current control request in either possible data direction. */
static void stall_control_endpoint(void) {
    // Unsupported or malformed requests must receive STALL rather than silently
    // timing out. A later SETUP packet causes hardware to recover endpoint zero.
    PHAL_USB_stall(PHAL_USB_CONTROL_ENDPOINT, PHAL_USB_ENDPOINT_DIRECTION_IN);
    PHAL_USB_stall(PHAL_USB_CONTROL_ENDPOINT, PHAL_USB_ENDPOINT_DIRECTION_OUT);
}

/** Queue a control-IN response, truncated to the host's requested length. */
static void send_control_response(const void *data,
                                  uint16_t available_length,
                                  uint16_t requested_length) {
    uint16_t response_length = available_length < requested_length
        ? available_length
        : requested_length;

    // PHAL_USB_write() copies the response into packet memory, so local and
    // static response storage may be reused as soon as this call returns.
    if (!PHAL_USB_write(PHAL_USB_CONTROL_ENDPOINT, data, response_length)) {
        stall_control_endpoint();
    }
}

/** Select the descriptor named by the high byte of the request's wValue. */
static void send_descriptor(uint16_t descriptor, uint16_t requested_length) {
    switch ((USB_DescriptorType_t)(descriptor >> 8U)) {
        case USB_DESCRIPTOR_DEVICE:
            send_control_response(
                g_device_descriptor, sizeof(g_device_descriptor), requested_length);
            break;
        case USB_DESCRIPTOR_CONFIGURATION:
            send_control_response(g_configuration_descriptor,
                                  sizeof(g_configuration_descriptor),
                                  requested_length);
            break;
        default:
            stall_control_endpoint();
            break;
    }
}

/** Handle the small standard-request subset needed to enumerate this device. */
static void handle_setup_packet(void) {
    // bmRequestType bits 6:5 are zero for standard requests. This example has no
    // class or vendor control protocol, so all other request categories stall.
    if ((g_setup_packet.type & 0x60U) != 0U) {
        stall_control_endpoint();
        return;
    }

    switch ((USB_Request_t)g_setup_packet.request) {
        case USB_REQUEST_GET_STATUS: {
            static const uint8_t status[] = {0U, 0U};
            send_control_response(status, sizeof(status), g_setup_packet.length);
            break;
        }
        case USB_REQUEST_GET_DESCRIPTOR:
            send_descriptor(g_setup_packet.value, g_setup_packet.length);
            break;
        case USB_REQUEST_SET_ADDRESS:
            if (g_setup_packet.value > 127U) {
                stall_control_endpoint();
                break;
            }
            // USB requires the old address through the zero-length IN status
            // packet. Save the address and apply it from that packet's completion.
            g_pending_address = g_setup_packet.value;
            send_control_response(nullptr, 0U, 0U);
            break;
        case USB_REQUEST_GET_CONFIGURATION: {
            uint8_t configuration = g_configured ? CONFIGURATION_VALUE : 0U;
            send_control_response(&configuration, sizeof(configuration), g_setup_packet.length);
            break;
        }
        case USB_REQUEST_SET_CONFIGURATION:
            // Configuration zero returns to the addressed state; configuration
            // one enables application traffic on the bulk endpoint.
            if (g_setup_packet.value == 0U
                || g_setup_packet.value == CONFIGURATION_VALUE) {
                g_configured = g_setup_packet.value == CONFIGURATION_VALUE;
                send_control_response(nullptr, 0U, 0U);
            } else {
                stall_control_endpoint();
            }
            break;
        default:
            stall_control_endpoint();
            break;
    }
}

/** Consume either a SETUP packet or the OUT status stage of control-IN. */
static void handle_control_out(bool is_setup_packet) {
    uint16_t received_length = 0U;

    if (!is_setup_packet) {
        // A control-IN transfer ends when the host sends an empty OUT packet.
        // Reading it rearms endpoint zero for the next SETUP request.
        PHAL_USB_read(PHAL_USB_CONTROL_ENDPOINT, nullptr, 0U, &received_length);
        return;
    }

    // PHAL_USB_read() consumes the packet and rearms the OUT direction only when
    // the destination is large enough. Every valid SETUP packet is eight bytes.
    if (!PHAL_USB_read(PHAL_USB_CONTROL_ENDPOINT,
                       &g_setup_packet,
                       sizeof(g_setup_packet),
                       &received_length)
        || received_length != sizeof(g_setup_packet)) {
        stall_control_endpoint();
        return;
    }

    handle_setup_packet();
}

/** Read one host packet and queue the same bytes for device-to-host transfer. */
static void echo_bulk_packet(void) {
    uint16_t received_length = 0U;
    // OUT data is available only after the callback reports its transfer. A
    // successful read rearms OUT; write copies the bytes to the independent IN
    // packet buffer. The host test waits for each echo before sending another.
    if (g_configured
        && PHAL_USB_read(PHAL_USB_DATA_ENDPOINT,
                         g_echo_packet,
                         sizeof(g_echo_packet),
                         &received_length)) {
        PHAL_USB_write(PHAL_USB_DATA_ENDPOINT, g_echo_packet, received_length);
    }
}

/**
 * @brief Receive bus and endpoint events from the HAL in interrupt context.
 *
 * Applications provide this strong definition; the HAL's weak default does
 * nothing. Keep callback work bounded and never block while handling an event.
 */
void PHAL_USB_callback(const PHAL_USB_Event_t *event) {
    if (event == nullptr) {
        return;
    }

    if (!event->transfer) {
        g_configured = false;
        g_pending_address = NO_PENDING_ADDRESS;
        return;
    }

    if (event->transfer_data.endpoint == PHAL_USB_CONTROL_ENDPOINT) {
        if (event->transfer_data.direction == PHAL_USB_ENDPOINT_DIRECTION_OUT) {
            handle_control_out(event->transfer_data.setup);
        } else if (g_pending_address <= 127U) {
            // An endpoint-zero IN event means the status packet has reached the
            // host, so changing DADDR now follows the SET_ADDRESS sequence.
            PHAL_USB_setAddress((uint8_t)g_pending_address);
            g_pending_address = NO_PENDING_ADDRESS;
        }
    } else if (event->transfer_data.endpoint == PHAL_USB_DATA_ENDPOINT
               && event->transfer_data.direction == PHAL_USB_ENDPOINT_DIRECTION_OUT) {
        echo_bulk_packet();
    }
}

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_170MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    // Initialize while detached, then connect only after the callback and all
    // application state are ready for the host's immediate enumeration traffic.
    if (!PHAL_USB_init() || !PHAL_USB_connect()) {
        HardFault_Handler();
    }

    PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, true);
    PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, false);

    // USB work is interrupt-driven through PHAL_USB_callback(). The main loop
    // has no polling requirement and can sleep until the next peripheral event.
    while (true) {
        __WFI();
    }
}

void HardFault_Handler(void) {
    PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, false);
    PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, true);
    while (true) {
        __NOP();
    }
}

#endif // G4_TESTING_CHOSEN == TEST_USB
