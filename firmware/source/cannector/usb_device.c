/**
 * @file usb_device.c
 * @brief USB tx thread
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "usb_device.h"

#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/usb/usb.h"
#include "common/rtos/rtos.h"
#include "common/timestamped_frame/timestamped_frame.h"
#include "main.h"

typedef enum {
    USB_STATE_CONNECTING,
    USB_WAIT_FOR_HOST,
    USB_STATE_SUBMIT,
    USB_STATE_FATAL
} usb_state_t;

static void tx_four_frames() {
    timestamped_frame_t usb_packet[4];
    static_assert(sizeof(usb_packet) == 64, "usb_packet max size is 64");

    for (size_t i = 0; i < 4; i++) {
        xQueueReceive(can_queue, &usb_packet[i], 0);
    }

    const size_t length = 4 * sizeof(timestamped_frame_t);
    (void)PHAL_USB_write(PHAL_USB_DATA_ENDPOINT, usb_packet, (uint16_t)length);
}

void usb_tx_periodic() {
    static usb_state_t curr_state = USB_STATE_CONNECTING;
    static usb_state_t next_state = USB_STATE_CONNECTING;

    curr_state = next_state;
    next_state = curr_state; // default self loop

    switch(curr_state) {
        case USB_STATE_CONNECTING:
            if (PHAL_USB_connect()) {
                next_state = USB_WAIT_FOR_HOST;
            } else {
                next_state = USB_STATE_FATAL;
            }
            break;
        case USB_WAIT_FOR_HOST:
            // todo check host status
            PHAL_USB_write(PHAL_USB_CONTROL_ENDPOINT, nullptr, 0);
            next_state = USB_STATE_SUBMIT;
            break;
        case USB_STATE_SUBMIT:
            // todo block here
            // block until can_queue has at least 4 messages
            if (uxQueueMessagesWaiting(can_queue) >= 4) {
                tx_four_frames();
                next_state = USB_WAIT_FOR_HOST;
            }
            break;
        case USB_STATE_FATAL:
            PHAL_GPIO_write(ERROR_LED_PORT, ERROR_LED_PIN, 1);
            break;
    }
}
