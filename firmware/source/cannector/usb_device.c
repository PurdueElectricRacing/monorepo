/**
 * @file usb_device.c
 * @brief Periodic CAN<->USB forwarding
 *
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Millan Kumar (kumar798@purdue.edu)
 */

#include "usb_device.h"

#include <string.h>

#include "activity_leds.h"
#include "can_library/generated/SCAN.h"
#include "can_library/generated/VCAN.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/rtos/rtos.h"
#include "main.h"
#include "usb/usb_control.h"


static can_tx_frame_t g_usb_rx_queue[USB_RX_QUEUE_CAPACITY];
static size_t g_usb_rx_head = 0U;
static size_t g_usb_rx_tail = 0U;
static size_t g_usb_rx_count = 0U;


void usb_rx_enqueue(const can_tx_frame_t* frames, size_t count) {
    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
    for (size_t i = 0U; i < count; ++i) {
        if (g_usb_rx_count == USB_RX_QUEUE_CAPACITY) {
            // Overwrite oldest frame on overflow
            g_usb_rx_head = (g_usb_rx_head + 1U) % USB_RX_QUEUE_CAPACITY;
            g_usb_rx_count--;
        }
        g_usb_rx_queue[g_usb_rx_tail] = frames[i];
        g_usb_rx_tail = (g_usb_rx_tail + 1U) % USB_RX_QUEUE_CAPACITY;
        g_usb_rx_count++;
    }
    __set_PRIMASK(saved_primask);
}

static bool usb_rx_dequeue(can_tx_frame_t* frame) {
    bool popped = false;
    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
    if (g_usb_rx_count > 0U) {
        *frame = g_usb_rx_queue[g_usb_rx_head];
        g_usb_rx_head = (g_usb_rx_head + 1U) % USB_RX_QUEUE_CAPACITY;
        g_usb_rx_count--;
        popped = true;
    }
    __set_PRIMASK(saved_primask);
    return popped;
}

static void process_usb_rx_frames(void) {
    can_tx_frame_t rx_frame;
    bool any_received = false;
    uint32_t now = xTaskGetTickCount();

    while (usb_rx_dequeue(&rx_frame)) {
        any_received = true;
        if (rx_frame.dlc > 8U) {
            continue;
        }

        FDCAN_GlobalTypeDef *bus = nullptr;
        activity_led_t bus_led = ACTIVITY_VCAN;
        if (rx_frame.bus_id == VCAN_BUS_ID) {
            bus = FDCAN1;
            bus_led = ACTIVITY_VCAN;
        } else if (rx_frame.bus_id == g_fdcan2_bus_id) {
            bus = FDCAN2;
            bus_led = ACTIVITY_MCAN;
        } else if (rx_frame.bus_id == SCAN_BUS_ID) {
            bus = FDCAN3;
            bus_led = ACTIVITY_SCAN;
        } else {
            // Discard messages for inactive or unmapped buses
            continue;
        }

        CanMsgTypeDef_t msg = {0};
        msg.Bus = bus;
        msg.IDE = (rx_frame.is_extended_id != 0U);
        if (msg.IDE) {
            msg.ExtId = rx_frame.can_id & 0x1FFFFFFFU;
        } else {
            msg.StdId = (uint16_t)(rx_frame.can_id & 0x7FFU);
        }
        msg.DLC = rx_frame.dlc;
        memcpy(msg.Data, rx_frame.data, rx_frame.dlc);

        if (PHAL_FDCAN_send(&msg)) {
            activity_leds_record(bus_led, now);
        }
    }

    if (any_received) {
        activity_leds_record(ACTIVITY_USB, now);
    }
}

void usb_tx_periodic(void) {
    process_usb_rx_frames();

    timestamped_frame_t packet[MAX_FRAMES_PER_PACKET];
    size_t count = 0U;
    while (count < MAX_FRAMES_PER_PACKET && xQueueReceive(can_queue, &packet[count], 0U) == pdTRUE) {
        count++;
    }
    if (count == 0U) {
        // no frames to send
        return;
    }

    // USB IRQ runs above the RTOS syscall priority
    // PRIMASK excludes it across the configuration check and PMA write
    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
    if (usb_data_enabled()) {
        // Best effort: discard this batch if IN is busy
        uint16_t packet_length = (uint16_t)(count * sizeof(packet[0]));
        if (PHAL_USB_write(PHAL_USB_DATA_ENDPOINT, packet, packet_length)) {
            activity_leds_record(ACTIVITY_USB, xTaskGetTickCount());
        }
    }
    __set_PRIMASK(saved_primask);

    // Unconfigured/halted batches are discarded
}
