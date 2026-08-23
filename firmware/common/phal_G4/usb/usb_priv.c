/**
 * @file usb_priv.c
 * @brief STM32G4 USB register and packet-memory implementation.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#include "common/phal_G4/usb/usb_priv.h"

#include "stm32g474xx.h"

// BTABLE occupies the first 64 bytes of packet memory: eight 8-byte endpoint
// descriptors. Starting packet buffers at byte 64 leaves the table isolated
// and keeps every 64-byte endpoint buffer naturally aligned.
static constexpr uint16_t USB_PRIV_BTABLE_OFFSET = 0U;
static constexpr uint16_t USB_PRIV_BUFFER_START = 64U;
static constexpr uint32_t USB_PRIV_CLOCK_TIMEOUT = 1'000'000U;
static constexpr uint32_t USB_PRIV_STARTUP_DELAY = 1'000U;

// STAT_TX and STAT_RX encode the handshake returned by one endpoint direction.
// DISABLED is zero and is not needed after endpoint setup.
static constexpr uint8_t USB_PRIV_STATUS_STALL = 1U;
static constexpr uint8_t USB_PRIV_STATUS_NAK = 2U;
static constexpr uint8_t USB_PRIV_STATUS_VALID = 3U;

static constexpr uint8_t USB_PRIV_DM_PIN = 11U;
static constexpr uint8_t USB_PRIV_DP_PIN = 12U;
static constexpr uint8_t USB_PRIV_CC1_PIN = 8U;
static constexpr uint8_t USB_PRIV_CC2_PIN = 9U;
static constexpr uint8_t USB_PRIV_GPIO_MODE_BITS = 0b11U;
static constexpr uint8_t USB_PRIV_GPIO_MODE_BIT_WIDTH = 2U;
static constexpr uint8_t USB_PRIV_ENDPOINT_REGISTER_STRIDE = 2U;
static constexpr uint8_t USB_PRIV_BTABLE_ENTRY_SIZE_BYTES = 8U;
static constexpr uint8_t USB_PRIV_BTABLE_WORD_SIZE_BYTES = 2U;
static constexpr uint8_t USB_PRIV_BUFFERS_PER_ENDPOINT = 2U;
static constexpr uint8_t USB_PRIV_ENDPOINT_COUNT = 8U;
static constexpr uint8_t USB_PRIV_STATUS_TX_SHIFT = 4U;
static constexpr uint8_t USB_PRIV_STATUS_RX_SHIFT = 12U;
static constexpr uint8_t USB_PRIV_RX_BLOCK_COUNT = 1U;
static constexpr uint8_t USB_PRIV_RX_BLOCK_COUNT_SHIFT = 10U;

/** Return the register that controls both directions of one endpoint. */
static volatile uint16_t *usb_endpoint_register(uint8_t endpoint) {
    // Endpoint registers are 32 bits apart even though their implemented fields
    // and the CMSIS declaration are 16 bits wide.
    return &USB->EP0R + endpoint * USB_PRIV_ENDPOINT_REGISTER_STRIDE;
}

/** Return one 16-bit field from an endpoint's four-word BTABLE entry. */
static volatile uint16_t *usb_descriptor_word(uint8_t endpoint, uint8_t word) {
    // Each BTABLE entry contains TX address/count followed by RX address/count.
    // USB_PMAADDR exposes packet memory as 16-bit words at byte-addressed offsets.
    uint32_t offset = USB_PRIV_BTABLE_OFFSET
        + endpoint * USB_PRIV_BTABLE_ENTRY_SIZE_BYTES
        + word * USB_PRIV_BTABLE_WORD_SIZE_BYTES;
    return (volatile uint16_t *)(USB_PMAADDR + offset);
}

/** Convert a USB byte address to its memory-mapped packet-memory address. */
static volatile uint16_t *usb_pma(uint16_t address) {
    return (volatile uint16_t *)(USB_PMAADDR + address);
}

/** Calculate the packet-memory byte address of an endpoint's IN buffer. */
static uint16_t usb_transmit_address(uint8_t endpoint) {
    // Every bidirectional endpoint owns adjacent TX and RX packet buffers.
    return USB_PRIV_BUFFER_START
        + endpoint * PHAL_USB_PACKET_SIZE_BYTES * USB_PRIV_BUFFERS_PER_ENDPOINT;
}

/** Calculate the packet-memory byte address of an endpoint's OUT buffer. */
static uint16_t usb_receive_address(uint8_t endpoint) {
    return usb_transmit_address(endpoint) + PHAL_USB_PACKET_SIZE_BYTES;
}

/** Provide the minimum peripheral power-up delay without requiring a timer. */
static void usb_delay(uint32_t iterations) {
    while (iterations-- > 0U) {
        __NOP();
    }
}

/** Enable HSI48 and wait a bounded period for clock-ready acknowledgement. */
static bool usb_enable_clock(void) {
    // USB full-speed requires a dedicated, accurate 48 MHz clock. HSI48 can run
    // independently of the application's system-clock and PLL configuration.
    RCC->CRRCR |= RCC_CRRCR_HSI48ON;
    for (uint32_t attempt = 0U; attempt < USB_PRIV_CLOCK_TIMEOUT; attempt++) {
        if ((RCC->CRRCR & RCC_CRRCR_HSI48RDY) != 0U) {
            return true;
        }
    }
    return false;
}

/** Release the USB-C CC and USB data pins from their GPIO digital paths. */
static void usb_configure_pins(void) {
    // GPIOA must be clocked before changing PA8/PA9 (USB-C CC lines) or
    // PA11/PA12 (USB D-/D+). The barrier ensures the clock write takes effect
    // before the following peripheral access.
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    __DSB();

    constexpr uint32_t cc_mask =
        (USB_PRIV_GPIO_MODE_BITS << (USB_PRIV_CC1_PIN * USB_PRIV_GPIO_MODE_BIT_WIDTH))
        | (USB_PRIV_GPIO_MODE_BITS << (USB_PRIV_CC2_PIN * USB_PRIV_GPIO_MODE_BIT_WIDTH));
    constexpr uint32_t data_mask =
        (USB_PRIV_GPIO_MODE_BITS << (USB_PRIV_DM_PIN * USB_PRIV_GPIO_MODE_BIT_WIDTH))
        | (USB_PRIV_GPIO_MODE_BITS << (USB_PRIV_DP_PIN * USB_PRIV_GPIO_MODE_BIT_WIDTH));
    // Analog mode disconnects the GPIO digital paths so UCPD and USB can own
    // their pins. Neither signaling block uses GPIO pull resistors.
    GPIOA->MODER = (GPIOA->MODER & ~(cc_mask | data_mask)) | cc_mask | data_mask;
    GPIOA->PUPDR &= ~(cc_mask | data_mask);
}

/** Configure UCPD1 to advertise this board as a USB-C power sink. */
static void usb_configure_type_c_sink(void) {
    // UCPD1 presents the USB-C sink pull-downs on PA8/PA9. PWR controls the
    // dead-battery pull-downs that otherwise remain connected during startup.
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    RCC->APB1ENR2 |= RCC_APB1ENR2_UCPD1EN;
    __DSB();

    // Enable the UCPD analog block and both CC channels in sink mode, then
    // disconnect the separate dead-battery resistors to avoid double loading.
    UCPD1->CFG1 = UCPD_CFG1_UCPDEN;
    UCPD1->CR = UCPD_CR_ANAMODE | UCPD_CR_CCENABLE;
    PWR->CR3 |= PWR_CR3_UCPD_DBDIS;
}

/** Reset and power up USB into a detached, interrupt-disabled state. */
static void usb_reset_peripheral(void) {
    // Pulse the RCC reset first so no endpoint or interrupt state survives a
    // previous application or debugger session.
    RCC->APB1RSTR1 |= RCC_APB1RSTR1_USBRST;
    __DSB();
    RCC->APB1RSTR1 &= ~RCC_APB1RSTR1_USBRST;
    __DSB();

    // PDWN disables the analog transceiver. Keep the core in forced reset while
    // powering the transceiver up, then wait for the reference-manual startup
    // interval before touching normal USB registers.
    USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
    usb_delay(USB_PRIV_STARTUP_DELAY);
    USB->CNTR = USB_CNTR_FRES;
    usb_delay(USB_PRIV_STARTUP_DELAY);

    // Start detached, with no stale flags, address, low-power state, or battery
    // charger configuration. Endpoint setup follows after this reset sequence.
    USB->ISTR = 0U;
    USB->BTABLE = USB_PRIV_BTABLE_OFFSET;
    USB->DADDR = 0U;
    USB->LPMCSR = 0U;
    USB->BCDR = 0U;
    USB->CNTR = 0U;
}

/** Change the hardware handshake state for one endpoint direction. */
static void usb_set_status(uint8_t endpoint,
                           PHAL_USB_EndpointDirection_t direction,
                           uint8_t status) {
    // STAT_TX and STAT_RX are toggle-on-write fields: writing one flips a bit
    // instead of assigning it. XOR the current and desired values to produce
    // exactly the toggles needed while preserving the other endpoint fields.
    volatile uint16_t *reg = usb_endpoint_register(endpoint);
    uint16_t current = *reg;
    uint16_t mask = direction == PHAL_USB_ENDPOINT_DIRECTION_IN
        ? USB_EPTX_STAT : USB_EPRX_STAT;
    uint8_t shift = direction == PHAL_USB_ENDPOINT_DIRECTION_IN
        ? USB_PRIV_STATUS_TX_SHIFT
        : USB_PRIV_STATUS_RX_SHIFT;
    // USB_EPREG_MASK preserves ordinary read/write fields and writes zero to
    // CTR flags, which preserves them because those flags clear on write-zero.
    // SETUP is read-only and is excluded from the value written back.
    uint16_t value = current & (USB_EPREG_MASK & ~USB_EP_SETUP);
    value |= (current & mask) ^ ((uint16_t)status << shift);
    *reg = value;
}

/** Acknowledge one endpoint transfer without clearing the opposite direction. */
static void usb_clear_transfer(uint8_t endpoint, PHAL_USB_EndpointDirection_t direction) {
    // CTR_TX and CTR_RX clear when software writes zero. The mask writes one to
    // the opposite CTR flag so acknowledging one direction cannot discard an
    // event that arrived concurrently on the other direction.
    volatile uint16_t *reg = usb_endpoint_register(endpoint);
    uint16_t flag = direction == PHAL_USB_ENDPOINT_DIRECTION_IN
        ? USB_EP_CTR_TX : USB_EP_CTR_RX;
    *reg = *reg & (USB_EPREG_MASK & ~USB_EP_SETUP & ~flag);
}

/** Populate one BTABLE entry and enable its IN and OUT directions. */
static void usb_configure_endpoint(uint8_t endpoint, uint16_t type_bits) {
    // A set BLSIZE selects 32-byte receive blocks; NUM_BLOCK=1 allocates two
    // blocks, giving each OUT direction one 64-byte full-speed packet buffer.
    constexpr uint16_t receive_size =
        USB_COUNT0_RX_BLSIZE | (USB_PRIV_RX_BLOCK_COUNT << USB_PRIV_RX_BLOCK_COUNT_SHIFT);
    *usb_descriptor_word(endpoint, 0U) = usb_transmit_address(endpoint);
    *usb_descriptor_word(endpoint, 1U) = 0U;
    *usb_descriptor_word(endpoint, 2U) = usb_receive_address(endpoint);
    *usb_descriptor_word(endpoint, 3U) = receive_size;

    // New IN endpoints begin at NAK until the application queues data. OUT
    // endpoints begin VALID so the host may immediately deliver SETUP or data.
    // Status fields still require toggle values even during configuration.
    uint16_t current = *usb_endpoint_register(endpoint);
    uint16_t value = endpoint | type_bits;
    value |= (current & USB_EPTX_STAT) ^ (USB_PRIV_STATUS_NAK << USB_PRIV_STATUS_TX_SHIFT);
    value |= (current & USB_EPRX_STAT) ^ (USB_PRIV_STATUS_VALID << USB_PRIV_STATUS_RX_SHIFT);
    *usb_endpoint_register(endpoint) = value;
}

/** Restore the fixed control and bulk endpoint configuration. */
static void usb_configure_endpoints(void) {
    // Endpoint zero handles enumeration; endpoint one is the application's
    // bidirectional bulk pipe. A bus reset restores this same known layout.
    usb_configure_endpoint(PHAL_USB_CONTROL_ENDPOINT, USB_EP_CONTROL);
    usb_configure_endpoint(PHAL_USB_DATA_ENDPOINT, USB_EP_BULK);
}

/** Copy an application packet into the STM32's 16-bit packet memory. */
static void usb_copy_to_pma(uint16_t address, const void *source, uint16_t length_bytes) {
    // STM32 packet memory is accessed as little-endian 16-bit words. Explicit
    // byte packing supports odd packet lengths without reading past source.
    const uint8_t *input = source;
    volatile uint16_t *output = usb_pma(address);

    for (uint16_t offset = 0U; offset < length_bytes; offset += 2U) {
        uint16_t word = input[offset];
        if (offset + 1U < length_bytes) {
            word |= (uint16_t)input[offset + 1U] << 8U;
        }
        *output++ = word;
    }
}

/** Copy a packet from STM32 packet memory into application memory. */
static void usb_copy_from_pma(uint16_t address, void *destination, uint16_t length_bytes) {
    // Unpack each PMA word into application bytes. The final high byte is
    // intentionally ignored for odd-length packets.
    volatile const uint16_t *input = usb_pma(address);
    uint8_t *output = destination;

    for (uint16_t offset = 0U; offset < length_bytes; offset += 2U) {
        uint16_t word = *input++;
        output[offset] = (uint8_t)word;
        if (offset + 1U < length_bytes) {
            output[offset + 1U] = (uint8_t)(word >> 8U);
        }
    }
}

bool USB_PRIV_init(void) {
    // Clock selection must happen before enabling USB on APB1. CLK48SEL=0
    // selects HSI48 as the peripheral's 48 MHz kernel clock.
    if (!usb_enable_clock()) {
        return false;
    }

    RCC->CCIPR &= ~RCC_CCIPR_CLK48SEL_Msk;
    RCC->APB1ENR1 |= RCC_APB1ENR1_USBEN;
    __DSB();

    usb_configure_pins();
    usb_configure_type_c_sink();
    usb_reset_peripheral();

    // Clear all eight hardware endpoint registers before enabling the two
    // endpoints owned by this HAL. Unused endpoints remain disabled.
    for (uint8_t endpoint = 0U; endpoint < USB_PRIV_ENDPOINT_COUNT; endpoint++) {
        *usb_endpoint_register(endpoint) = 0U;
    }
    usb_configure_endpoints();
    return true;
}

void USB_PRIV_deinit(void) {
    // Remove the D+ pull-up first so the host observes a clean disconnect before
    // interrupts, the analog transceiver, and the APB clock are disabled.
    USB->BCDR &= ~USB_BCDR_DPPU;
    USB->CNTR = 0U;
    NVIC_DisableIRQ(USB_LP_IRQn);
    USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
    USB->DADDR = 0U;
    RCC->APB1ENR1 &= ~RCC_APB1ENR1_USBEN;
    __DSB();
}

void USB_PRIV_enableInterrupts(void) {
    // CTRM reports endpoint transfers. The remaining masks report bus reset,
    // suspend/wakeup, packet-memory overrun, and protocol errors. Setting EF in
    // DADDR enables USB operation while the device address remains zero.
    USB->CNTR = USB_CNTR_CTRM | USB_CNTR_PMAOVRM | USB_CNTR_ERRM
        | USB_CNTR_WKUPM | USB_CNTR_SUSPM | USB_CNTR_RESETM;
    USB->DADDR = USB_DADDR_EF;
    NVIC_EnableIRQ(USB_LP_IRQn);
}

void USB_PRIV_connect(void) {
    // A full-speed device announces attachment with a pull-up on D+. Delaying
    // this until public initialization completes prevents premature enumeration.
    USB->BCDR |= USB_BCDR_DPPU;
}

void USB_PRIV_setAddress(uint8_t address) {
    // EF must remain set whenever DADDR is updated or the device stops responding.
    USB->DADDR = USB_DADDR_EF | address;
}

void USB_PRIV_stall(uint8_t endpoint, PHAL_USB_EndpointDirection_t direction) {
    usb_set_status(endpoint, direction, USB_PRIV_STATUS_STALL);
}

bool USB_PRIV_write(uint8_t endpoint, const void *source, uint16_t length_bytes) {
    // NAK is the idle IN state. VALID means a packet is already queued, while
    // STALL requires host recovery before another normal transfer can begin.
    if ((*usb_endpoint_register(endpoint) & USB_EPTX_STAT)
        != (USB_PRIV_STATUS_NAK << USB_PRIV_STATUS_TX_SHIFT)) {
        return false;
    }

    // Clear any previous completion, copy the payload, and publish its byte
    // count before marking the endpoint VALID. Hardware may transmit as soon as
    // VALID is written, so packet memory must be complete first.
    usb_clear_transfer(endpoint, PHAL_USB_ENDPOINT_DIRECTION_IN);
    usb_copy_to_pma(usb_transmit_address(endpoint), source, length_bytes);
    *usb_descriptor_word(endpoint, 1U) = length_bytes;
    usb_set_status(endpoint, PHAL_USB_ENDPOINT_DIRECTION_IN, USB_PRIV_STATUS_VALID);
    return true;
}

bool USB_PRIV_read(uint8_t endpoint,
                   void *destination,
                   uint16_t destination_capacity,
                   uint16_t *received_length) {
    // CTR_RX remains set until software consumes the completed OUT packet.
    if ((*usb_endpoint_register(endpoint) & USB_EP_CTR_RX) == 0U) {
        return false;
    }

    volatile uint16_t *receive_count = usb_descriptor_word(endpoint, 3U);
    uint16_t length = *receive_count & USB_COUNT0_RX_COUNT0_RX_Msk;
    *received_length = length;
    if (length > destination_capacity) {
        // Leave CTR_RX set and the endpoint un-rearmed. This preserves the packet
        // and naturally NAKs more OUT traffic until the application retries.
        return false;
    }

    usb_copy_from_pma(usb_receive_address(endpoint), destination, length);
    // Acknowledge the packet, clear only the received-byte count, then return
    // the OUT direction to VALID so hardware can accept the next host packet.
    usb_clear_transfer(endpoint, PHAL_USB_ENDPOINT_DIRECTION_OUT);
    *receive_count &= (uint16_t)~USB_COUNT0_RX_COUNT0_RX_Msk;
    usb_set_status(endpoint, PHAL_USB_ENDPOINT_DIRECTION_OUT, USB_PRIV_STATUS_VALID);
    return true;
}

/** Acknowledge non-endpoint flags present in an ISTR snapshot. */
static void usb_clear_interrupt_flags(uint16_t interrupt_status) {
    // These ISTR flags clear by writing zero. CTR is excluded because its source
    // lives in EPnR and is acknowledged by usb_clear_transfer instead.
    constexpr uint16_t clearable = USB_ISTR_PMAOVR | USB_ISTR_ERR | USB_ISTR_WKUP
        | USB_ISTR_SUSP | USB_ISTR_RESET | USB_ISTR_SOF | USB_ISTR_ESOF | USB_ISTR_L1REQ;
    if ((interrupt_status & clearable) != 0U) {
        USB->ISTR = (uint16_t)~(interrupt_status & clearable);
    }
}

void USB_PRIV_handleInterrupt(void) {
    // Snapshot ISTR once so the callback receives one coherent event. If more
    // events arrive concurrently, their flags remain set and retrigger the ISR.
    uint16_t interrupt_status = USB->ISTR;
    PHAL_USB_Event_t event = {0};

    if ((interrupt_status & USB_ISTR_RESET) != 0U) {
        // USB reset returns the device to address zero and invalidates all prior
        // endpoint state. Rebuild BTABLE and endpoint handshakes before notifying
        // the application so it can immediately process new SETUP packets.
        USB_PRIV_setAddress(0U);
        usb_configure_endpoints();
        event.transfer = false;
    } else if ((interrupt_status & USB_ISTR_CTR) != 0U) {
        event.transfer = true;
        // EP_ID identifies the endpoint with the highest-priority CTR flag. DIR
        // distinguishes an OUT/SETUP reception from an IN transmission.
        event.transfer_data.endpoint = (uint8_t)(interrupt_status & USB_ISTR_EP_ID);
        event.transfer_data.direction = (interrupt_status & USB_ISTR_DIR) != 0U
            ? PHAL_USB_ENDPOINT_DIRECTION_OUT : PHAL_USB_ENDPOINT_DIRECTION_IN;
        event.transfer_data.setup =
            event.transfer_data.direction == PHAL_USB_ENDPOINT_DIRECTION_OUT
            && ((*usb_endpoint_register(event.transfer_data.endpoint) & USB_EP_SETUP) != 0U);

        if (event.transfer_data.direction == PHAL_USB_ENDPOINT_DIRECTION_IN) {
            // Retire an IN packet before the callback. NAK makes the endpoint
            // available for PHAL_USB_write() to queue its next packet immediately.
            usb_clear_transfer(event.transfer_data.endpoint, event.transfer_data.direction);
            usb_set_status(event.transfer_data.endpoint,
                           event.transfer_data.direction,
                           USB_PRIV_STATUS_NAK);
        }
    } else {
        // Suspend, wakeup, and error details are not part of the public event API.
        usb_clear_interrupt_flags(interrupt_status);
        return;
    }

    // The callback runs in interrupt context. Clear non-endpoint flags only
    // afterward so reset state remains observable throughout the callback.
    PHAL_USB_callback(&event);
    usb_clear_interrupt_flags(interrupt_status);
}

uint16_t USB_PRIV_getFrameNumber(void) {
    // FN is the rolling 11-bit frame number sampled from Start-of-Frame packets.
    return USB->FNR & USB_FNR_FN;
}
