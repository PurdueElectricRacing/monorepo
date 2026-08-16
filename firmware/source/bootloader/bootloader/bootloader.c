/**
 * @file bootloader.c
 * @brief Bare-metal G4 CAN update, validation, and application hand-off.
 * @author Ronak Jain (jain717@purdue.edu)
 *
 * The FDCAN ISR only queues frames; BL_poll() performs flash and CRC operations
 * in main context.  Transport selection is data-driven by node_defs.h so the
 * same update state machine can serve standard or extended frames on any
 * configured FDCAN peripheral.
 */

#include "bootloader/bootloader.h"

#include "node_defs.h"

#include "common/bootloader/bootloader_common.h"
#include "common/phal_G4/crc/crc.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/flash/flash.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/phal_G4.h"

#include <string.h>

/*
 * Single-producer/single-consumer queue: the FDCAN ISR owns head and BL_poll()
 * owns tail. One slot stays empty so head == tail means empty, giving a usable
 * capacity of 15 frames. Overflow is dropped in the ISR; the host's bounded
 * bursts, boundary acknowledgements, and retries provide recovery.
 */
#define BL_RX_QUEUE_LENGTH 16U

typedef struct {
    CanMsgTypeDef_t message;
    const BLTransportConfig_t *transport;
} BLQueuedFrame_t;

/* SRAM bounds used to reject an application with an implausible initial MSP. */
#define BL_RAM_START 0x20000000U
#define BL_RAM_END 0x2001FFFFU

static volatile BLQueuedFrame_t bl_rx_queue[BL_RX_QUEUE_LENGTH];
static volatile uint8_t bl_rx_head;
static volatile uint8_t bl_rx_tail;

static bool bl_update_active;
static const BLTransportConfig_t *bl_update_transport;
static uint32_t bl_firmware_size;
static uint32_t bl_total_words;
static uint32_t bl_next_word;
static uint32_t bl_pending_word;
static bool bl_pending_word_valid;

/* Size constraints shared by staging, metadata, and CRC validation. */
static bool bl_size_is_valid(uint32_t size_bytes) {
    return size_bytes != 0U && (size_bytes % BL_WORD_SIZE) == 0U
        && size_bytes <= BL_APP_SLOT_SIZE;
}

/* Validate a flash range without allowing endpoint wraparound. */
static bool bl_range_is_valid(uint32_t address, uint32_t size_bytes) {
    if (!bl_size_is_valid(size_bytes) || address < BL_FLASH_BASE) {
        return false;
    }

    uint32_t end_address = address + size_bytes - 1U;
    return end_address >= address && end_address <= BL_FLASH_END;
}

static uint32_t bl_message_id(const CanMsgTypeDef_t *message) {
    return message->IDE ? message->ExtId : message->StdId;
}

/* Find the configured transport that received a bootloader command/data frame. */
static const BLTransportConfig_t *bl_find_transport(const CanMsgTypeDef_t *message) {
    if (message == NULL) {
        return NULL;
    }

    uint32_t message_id = bl_message_id(message);
    for (size_t i = 0U; i < BL_TRANSPORT_COUNT; i++) {
        const BLTransportConfig_t *transport = &bl_transports[i];
        if (message->Bus != transport->peripheral
            || message->IDE != transport->is_extended_id) {
            continue;
        }

        if (message_id == transport->command_id || message_id == transport->data_id) {
            return transport;
        }
    }

    return NULL;
}

/* Send status and detail on the transport that supplied the request. */
static void bl_send_status(const BLTransportConfig_t *transport,
                           uint8_t status,
                           uint32_t detail) {
    if (transport == NULL) {
        return;
    }

    CanMsgTypeDef_t response = {
        .Bus = transport->peripheral,
        .IDE = transport->is_extended_id,
        .DLC = transport->response_dlc,
        .Data = {0},
    };

    if (transport->is_extended_id) {
        response.ExtId = transport->response_id;
    } else {
        response.StdId = (uint16_t)transport->response_id;
    }

    response.Data[0] = status;
    response.Data[1] = (uint8_t)(detail >> 0U);
    response.Data[2] = (uint8_t)(detail >> 8U);
    response.Data[3] = (uint8_t)(detail >> 16U);
    response.Data[4] = (uint8_t)(detail >> 24U);
    (void)PHAL_FDCAN_send(&response);
}

/* Check that a frame belongs to the active update transport. */
static bool bl_is_active_transport(const BLTransportConfig_t *transport) {
    return bl_update_active && transport == bl_update_transport;
}

/*
 * STM32G4 flash writes are 64-bit. An unpaired final image word is followed by
 * erased-state 0xFF padding for the physical write. The logical image size and
 * CRC still cover only the host-supplied 32-bit words.
 */
static bool bl_flush_pending_word(void) {
    if (!bl_pending_word_valid) {
        return true;
    }

    uint8_t double_word[BL_FLASH_WRITE_SIZE];
    memset(double_word, 0xFF, sizeof(double_word));
    memcpy(double_word, &bl_pending_word, sizeof(bl_pending_word));
    bool success = PHAL_FLASH_write(
        BL_STAGING_ADDRESS + (bl_next_word - 1U) * BL_WORD_SIZE,
        double_word,
        sizeof(double_word)
    );
    bl_pending_word_valid = false;
    if (!success) {
        bl_send_status(bl_update_transport, BLSTAT_ERROR, BLERROR_FLASH);
    }
    return success;
}

/* Duplicate words are harmless; gaps cancel the sequential transfer. */
static bool bl_write_word(const BLTransportConfig_t *transport,
                          uint16_t index,
                          uint32_t word) {
    if (!bl_update_active) {
        bl_send_status(transport, BLSTAT_ERROR, BLERROR_LOCKED);
        return false;
    }
    if ((uint32_t)index >= bl_total_words) {
        bl_send_status(transport, BLSTAT_ERROR, BLERROR_ADDRESS);
        return false;
    }

    /* CAN retransmission is harmless for an already accepted index; the
     * protocol is otherwise a sequential stream, not random-access flash. */
    if ((uint32_t)index < bl_next_word) {
        return true;
    }
    if ((uint32_t)index != bl_next_word) {
        bl_update_active = false;
        bl_update_transport = NULL;
        bl_send_status(transport, BLSTAT_ERROR, BLERROR_SEQUENCE);
        return false;
    }

    if ((index & 1U) == 0U) {
        bl_pending_word = word;
        bl_pending_word_valid = true;
    } else {
        uint8_t double_word[BL_FLASH_WRITE_SIZE] = {0};
        memcpy(double_word, &bl_pending_word, sizeof(bl_pending_word));
        memcpy(double_word + sizeof(bl_pending_word), &word, sizeof(word));
        if (!PHAL_FLASH_write(
                BL_STAGING_ADDRESS + ((uint32_t)index - 1U) * BL_WORD_SIZE,
                double_word,
                sizeof(double_word))) {
            bl_update_active = false;
            bl_update_transport = NULL;
            bl_send_status(transport, BLSTAT_ERROR, BLERROR_FLASH);
            return false;
        }
        bl_pending_word_valid = false;
    }

    bl_next_word++;
    return true;
}

/*
 * Leave the active application and metadata untouched while receiving data.
 * Staging begins on an erase-page boundary; PHAL_FLASH_erase() rounds the
 * requested image range to complete pages inside the dedicated staging slot.
 */
static bool bl_begin_update(const BLTransportConfig_t *transport, uint32_t size_bytes) {
    if (!bl_size_is_valid(size_bytes)) {
        bl_send_status(transport, BLSTAT_ERROR, BLERROR_SIZE);
        return false;
    }

    bl_update_active = false;
    bl_update_transport = NULL;
    bl_pending_word_valid = false;

    if (!PHAL_FLASH_erase(BL_STAGING_ADDRESS, size_bytes)) {
        bl_send_status(transport, BLSTAT_ERROR, BLERROR_FLASH);
        return false;
    }

    bl_firmware_size = size_bytes;
    bl_total_words = size_bytes / BL_WORD_SIZE;
    bl_next_word = 0U;
    bl_update_transport = transport;
    bl_update_active = true;
    bl_send_status(transport, BLSTAT_ACK, size_bytes);
    return true;
}

/*
 * Copy verified staging data with 8-byte-aligned writes. A final four-byte
 * image word is physically padded with 0xFF; metadata retains size_bytes so
 * validation and CRC ignore that padding.
 */
static bool bl_copy_staging_to_application(const BLTransportConfig_t *transport,
                                           uint32_t size_bytes) {
    if (!bl_range_is_valid(BL_APP_ADDRESS, size_bytes)) {
        bl_send_status(transport, BLSTAT_ERROR, BLERROR_ADDRESS);
        return false;
    }

    if (!PHAL_FLASH_erase(BL_APP_ADDRESS, size_bytes)) {
        bl_send_status(transport, BLSTAT_ERROR, BLERROR_FLASH);
        return false;
    }

    for (uint32_t offset = 0U; offset < size_bytes; offset += BL_FLASH_WRITE_SIZE) {
        uint8_t double_word[BL_FLASH_WRITE_SIZE];
        memset(double_word, 0xFF, sizeof(double_word));
        uint32_t remaining = size_bytes - offset;
        uint32_t copy_size = remaining < BL_FLASH_WRITE_SIZE ? remaining : BL_FLASH_WRITE_SIZE;
        memcpy(double_word, (const void *)(BL_STAGING_ADDRESS + offset), copy_size);

        if (!PHAL_FLASH_write(BL_APP_ADDRESS + offset, double_word, sizeof(double_word))) {
            bl_send_status(transport, BLSTAT_ERROR, BLERROR_FLASH);
            return false;
        }
    }

    return true;
}

/*
 * Metadata is the commit marker and is written after the application. Erasing
 * BL_METADATA_SIZE clears the complete dedicated 2 KiB metadata page; no other
 * persistent data may share that page.
 */
static bool bl_write_metadata(uint32_t crc32, uint32_t size_bytes) {
    BootloaderMetadata_t metadata = {
        .magic = BOOTLOADER_METADATA_MAGIC,
        .crc32 = crc32,
        .address = BL_APP_ADDRESS,
        .size_bytes = size_bytes,
    };

    if (!PHAL_FLASH_erase(BL_METADATA_ADDRESS, BL_METADATA_SIZE)) {
        return false;
    }
    return PHAL_FLASH_write(BL_METADATA_ADDRESS, &metadata, sizeof(metadata));
}

/* Verify staging, install it, and commit metadata. */
static bool bl_commit_update(const BLTransportConfig_t *transport, uint32_t expected_crc) {
    if (!bl_is_active_transport(transport) || bl_next_word != bl_total_words) {
        bl_send_status(transport, BLSTAT_ERROR, BLERROR_SEQUENCE);
        return false;
    }

    if (!bl_flush_pending_word()) {
        bl_update_active = false;
        bl_update_transport = NULL;
        return false;
    }

    uint32_t actual_crc = PHAL_CRC_calculate(
        (const uint32_t *)(uintptr_t)BL_STAGING_ADDRESS,
        bl_total_words
    );
    if (actual_crc != expected_crc) {
        bl_update_active = false;
        bl_update_transport = NULL;
        bl_send_status(transport, BLSTAT_CRC_ERROR, actual_crc);
        return false;
    }

    if (!bl_copy_staging_to_application(transport, bl_firmware_size)
        || !bl_write_metadata(actual_crc, bl_firmware_size)) {
        bl_update_active = false;
        bl_update_transport = NULL;
        bl_send_status(transport, BLSTAT_ERROR, BLERROR_FLASH);
        return false;
    }

    bl_update_active = false;
    bl_update_transport = NULL;
    bl_send_status(transport, BLSTAT_ACK, actual_crc);
    return true;
}

/* Require a valid SRAM stack pointer and Thumb reset handler in the image. */
static bool bl_vector_is_valid(uint32_t app_address, uint32_t size_bytes) {
    if (!bl_range_is_valid(app_address, size_bytes)) {
        return false;
    }

    uint32_t stack_pointer = *(const volatile uint32_t *)(uintptr_t)app_address;
    uint32_t reset_handler = *(const volatile uint32_t *)(uintptr_t)(app_address + 4U);
    uint32_t reset_address = reset_handler & ~1U;
    uint32_t app_end = app_address + size_bytes;

    return stack_pointer >= BL_RAM_START && stack_pointer <= (BL_RAM_END + 1U)
        && (reset_handler & 1U) != 0U && reset_address >= app_address && reset_address < app_end;
}

/* Validate metadata, vectors, and CRC before transferring control. */
bool BL_checkAndBoot(void) {
    PHAL_CRC_init();
    BootloaderMetadata_t metadata = {0};
    if (!PHAL_FLASH_read(BL_METADATA_ADDRESS, &metadata, sizeof(metadata))) {
        return false;
    }

    if (metadata.magic != BOOTLOADER_METADATA_MAGIC
        || metadata.address != BL_APP_ADDRESS
        || !bl_vector_is_valid(metadata.address, metadata.size_bytes)) {
        return false;
    }

    uint32_t calculated_crc = PHAL_CRC_calculate(
        (const uint32_t *)(uintptr_t)metadata.address,
        metadata.size_bytes / BL_WORD_SIZE
    );
    if (calculated_crc != metadata.crc32) {
        return false;
    }

    uint32_t app_reset_handler = *(const volatile uint32_t *)(uintptr_t)(metadata.address + 4U);

    /* The application must inherit no pending bootloader timing/CAN context. */
    NVIC_DisableIRQ(FDCAN1_IT0_IRQn);
    NVIC_DisableIRQ(FDCAN1_IT1_IRQn);
    NVIC_DisableIRQ(FDCAN2_IT0_IRQn);
    NVIC_DisableIRQ(FDCAN2_IT1_IRQn);
    NVIC_DisableIRQ(FDCAN3_IT0_IRQn);
    NVIC_DisableIRQ(FDCAN3_IT1_IRQn);
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    __disable_irq();
    SCB->VTOR = metadata.address;
    __set_MSP(*(const volatile uint32_t *)(uintptr_t)metadata.address);
    ((void (*)(void))(uintptr_t)app_reset_handler)();

    return false;
}

/* Decode the small wire format without linking application CAN drivers. */
static void bl_process_message(const BLQueuedFrame_t *queued) {
    if (queued == NULL || queued->transport == NULL) {
        return;
    }

    const BLTransportConfig_t *transport = queued->transport;
    const CanMsgTypeDef_t *message = &queued->message;
    uint32_t message_id = bl_message_id(message);

    if (message_id == transport->command_id && message->DLC >= 5U) {
        uint32_t argument = ((uint32_t)message->Data[1] << 0U)
            | ((uint32_t)message->Data[2] << 8U)
            | ((uint32_t)message->Data[3] << 16U)
            | ((uint32_t)message->Data[4] << 24U);

        switch ((BLCmd_t)message->Data[0]) {
            case BLCMD_START:
                (void)bl_begin_update(transport, argument);
                break;
            case BLCMD_CRC:
                (void)bl_commit_update(transport, argument);
                break;
            case BLCMD_JUMP:
                if (!BL_checkAndBoot()) {
                    bl_send_status(transport, BLSTAT_ERROR, BLERROR_ADDRESS);
                }
                break;
            default:
                bl_send_status(transport, BLSTAT_UNKNOWN_CMD, message->Data[0]);
                break;
        }
        return;
    }

    if (message_id == transport->data_id && message->DLC >= 6U) {
        uint16_t index = (uint16_t)(((uint16_t)message->Data[1] << 8U) | message->Data[0]);
        uint32_t word = ((uint32_t)message->Data[2] << 0U)
            | ((uint32_t)message->Data[3] << 8U)
            | ((uint32_t)message->Data[4] << 16U)
            | ((uint32_t)message->Data[5] << 24U);
        (void)bl_write_word(transport, index, word);
    }
}

/* Drain frames in main context, where flash operations are safe. */
void BL_poll(void) {
    while (bl_rx_tail != bl_rx_head) {
        BLQueuedFrame_t queued;

        /* Keep the ISR from advancing head while the volatile frame is copied. */
        __disable_irq();
        queued = bl_rx_queue[bl_rx_tail];
        bl_rx_tail = (uint8_t)((bl_rx_tail + 1U) % BL_RX_QUEUE_LENGTH);
        __enable_irq();

        bl_process_message(&queued);
    }
}

static void bl_enable_irq(FDCAN_GlobalTypeDef *peripheral) {
    IRQn_Type irq;
    if (peripheral == FDCAN1) {
        irq = FDCAN1_IT0_IRQn;
    } else if (peripheral == FDCAN2) {
        irq = FDCAN2_IT0_IRQn;
    } else {
        irq = FDCAN3_IT0_IRQn;
    }

    NVIC_SetPriority(irq, 5U);
    NVIC_EnableIRQ(irq);
}

/* Initialize every configured transport and announce readiness on each one. */
void BL_init(void) {
    bl_rx_head = 0U;
    bl_rx_tail = 0U;

    for (size_t i = 0U; i < BL_TRANSPORT_COUNT; i++) {
        const BLTransportConfig_t *transport = &bl_transports[i];
        GPIOInitConfig_t can_gpio[] = {transport->rx_gpio, transport->tx_gpio};
        (void)PHAL_initGPIO(can_gpio, sizeof(can_gpio) / sizeof(can_gpio[0]));

        PHAL_FDCAN_init(transport->peripheral, transport->baud_rate);
        uint32_t filter_ids[] = {transport->command_id, transport->data_id};
        if (transport->is_extended_id) {
            (void)PHAL_FDCAN_setFilters(
                transport->peripheral,
                NULL,
                0U,
                filter_ids,
                sizeof(filter_ids) / sizeof(filter_ids[0])
            );
        } else {
            (void)PHAL_FDCAN_setFilters(
                transport->peripheral,
                filter_ids,
                sizeof(filter_ids) / sizeof(filter_ids[0]),
                NULL,
                0U
            );
        }
        bl_enable_irq(transport->peripheral);
    }

    PHAL_CRC_init();
    for (size_t i = 0U; i < BL_TRANSPORT_COUNT; i++) {
        bl_send_status(&bl_transports[i], BLSTAT_READY, BOOTLOADER_PROTOCOL_VERSION);
    }
}

/*
 * ISR-side queue producer. Publish head only after the complete frame copy so
 * BL_poll() never observes a partial message. Drop on overflow rather than
 * transmitting, blocking, or performing flash work in interrupt context.
 */
void PHAL_FDCAN_rxCallback(CanMsgTypeDef_t *message) {
    const BLTransportConfig_t *transport = bl_find_transport(message);
    if (transport == NULL) {
        return;
    }

    uint8_t next_head = (uint8_t)((bl_rx_head + 1U) % BL_RX_QUEUE_LENGTH);
    if (next_head == bl_rx_tail) {
        return;
    }

    bl_rx_queue[bl_rx_head].message = *message;
    bl_rx_queue[bl_rx_head].transport = transport;
    bl_rx_head = next_head;
}
