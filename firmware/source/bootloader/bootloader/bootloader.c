/**
 * @file bootloader.c
 * @brief Bare-metal G4 CAN update, validation, and application hand-off.
 * @author Ronak Jain (jain717@purdue.edu)
 *
 * The FDCAN ISR only queues frames; BL_poll() performs flash and CRC operations
 * in main context. Transport selection is data-driven by node_defs.h, while
 * each resident image accepts updates on exactly one configured CAN bus.
 */

#include "bootloader/bootloader.h"

#include <string.h>

#include "can_library/generated/can_version.h"
#include "common/bootloader/bootloader_common.h"
#include "common/phal_G4/crc/crc.h"
#include "common/phal_G4/fdcan/fdcan.h"
#include "common/phal_G4/flash/flash.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "node_defs.h"

/*
 * Single-producer/single-consumer queue: the FDCAN ISR owns head and BL_poll()
 * owns tail. One slot stays empty so head == tail means empty, giving a usable
 * capacity of 15 frames. Overflow is dropped in the ISR; the host sends bounded
 * bursts and reports boundary errors so a failed transfer can be retried from a
 * new START.
 */
#define BL_RX_QUEUE_LENGTH 16U

/* SRAM bounds used to reject an application with an implausible initial MSP. */
#define BL_RAM_START 0x20000000U
#define BL_RAM_END   0x2001FFFFU

typedef enum {
    BL_STATE_STARTUP,
    BL_STATE_READY,
    BL_STATE_UPDATING,
    BL_STATE_CHECKING,
    BL_STATE_RECOVERY,
} BLState_t;

static volatile CanMsgTypeDef_t bl_rx_queue[BL_RX_QUEUE_LENGTH];
static volatile uint8_t bl_rx_head;
static volatile uint8_t bl_rx_tail;

static BLState_t current_state = BL_STATE_STARTUP;
static BLState_t next_state    = BL_STATE_STARTUP;
static bool bl_boot_status_requested;
static bool bl_update_active;
static volatile uint32_t bl_millis;
static uint32_t bl_startup_start_ms;
static uint32_t bl_last_info_ms;
static bool bl_info_sent;
static uint32_t bl_firmware_size;
static uint32_t bl_total_words;
static uint32_t bl_next_word;
static uint32_t bl_pending_word;
static bool bl_pending_word_valid;

/* Size constraints shared by metadata and CRC validation. */
static bool bl_size_is_valid(uint32_t size_bytes) {
    return size_bytes != 0U && (size_bytes % BL_WORD_SIZE) == 0U && size_bytes <= BL_APP_SLOT_SIZE;
}

/* Validate a flash range without allowing endpoint wraparound. */
static bool bl_range_is_valid(uint32_t address, uint32_t size_bytes) {
    if (!bl_size_is_valid(size_bytes) || address < BL_FLASH_BASE) {
        return false;
    }

    uint32_t end_address = address + size_bytes - 1U;
    return end_address >= address && end_address <= BL_APP_END;
}

static uint32_t bl_message_id(const CanMsgTypeDef_t *message) {
    return message->IDE ? message->ExtId : message->StdId;
}

/* Accept bootloader traffic only from this target's single configured bus. */
static bool bl_message_is_bootloader_traffic(const CanMsgTypeDef_t *message) {
    if (message == NULL || message->Bus != bl_transport.peripheral
        || message->IDE != bl_transport.is_extended_id) {
        return false;
    }

    uint32_t message_id = bl_message_id(message);
    return message_id == bl_transport.start_id || message_id == bl_transport.crc_id
        || message_id == bl_transport.jump_id || message_id == bl_transport.data_id;
}

/* Send status and detail on this target's configured transport. */
static void bl_send_status(bootloader_status_t status, uint32_t detail) {
    CanMsgTypeDef_t response = {
        .Bus  = bl_transport.peripheral,
        .IDE  = bl_transport.is_extended_id,
        .DLC  = bl_transport.response_dlc,
        .Data = {0},
    };

    if (bl_transport.is_extended_id) {
        response.ExtId = bl_transport.response_id;
    } else {
        response.StdId = (uint16_t)bl_transport.response_id;
    }

    response.Data[0] = (uint8_t)status;
    response.Data[1] = (uint8_t)(detail >> 0U);
    response.Data[2] = (uint8_t)(detail >> 8U);
    response.Data[3] = (uint8_t)(detail >> 16U);
    response.Data[4] = (uint8_t)(detail >> 24U);
    (void)PHAL_FDCAN_send(&response);
}

/* Announce the resident image and its update capability. */
static void bl_send_info(void) {
    CanMsgTypeDef_t info = {
        .Bus  = bl_transport.peripheral,
        .IDE  = bl_transport.is_extended_id,
        .DLC  = bl_transport.info_dlc,
        .Data = {0},
    };

    if (bl_transport.is_extended_id) {
        info.ExtId = bl_transport.info_id;
    } else {
        info.StdId = (uint16_t)bl_transport.info_id;
    }

    info.Data[0] = BOOTLOADER_PROTOCOL_VERSION;
    info.Data[1] = (uint8_t)(CAN_LIBRARY_GIT_HASH >> 0U);
    info.Data[2] = (uint8_t)(CAN_LIBRARY_GIT_HASH >> 8U);
    info.Data[3] = (uint8_t)(CAN_LIBRARY_GIT_HASH >> 16U);
    info.Data[4] = (uint8_t)(CAN_LIBRARY_GIT_HASH >> 24U);
    info.Data[5] = bl_transport.target_id;
    info.Data[6] = BOOTLOADER_INFO_FLAG_BOOTLOADABLE | BOOTLOADER_INFO_FLAG_READY;
    (void)PHAL_FDCAN_send(&info);
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
    bool success          = PHAL_FLASH_write(BL_APP_ADDRESS + (bl_next_word - 1U) * BL_WORD_SIZE,
                                    double_word,
                                    sizeof(double_word));
    bl_pending_word_valid = false;
    if (!success) {
        bl_send_status(BOOTLOADER_STATUS_ERROR, BLERROR_FLASH);
    }
    return success;
}

/* Duplicate words are harmless; gaps cancel the sequential transfer. */
static bool bl_write_word(uint32_t index, uint32_t word) {
    if (!bl_update_active) {
        bl_send_status(BOOTLOADER_STATUS_ERROR, BLERROR_LOCKED);
        return false;
    }
    if (index >= bl_total_words) {
        bl_send_status(BOOTLOADER_STATUS_ERROR, BLERROR_ADDRESS);
        return false;
    }

    /* CAN retransmission is harmless for an already accepted index; the
     * protocol is otherwise a sequential stream, not random-access flash. */
    if (index < bl_next_word) {
        return true;
    }
    if (index != bl_next_word) {
        bl_update_active = false;
        next_state       = BL_STATE_RECOVERY;
        bl_send_status(BOOTLOADER_STATUS_ERROR, BLERROR_SEQUENCE);
        return false;
    }

    if ((index & 1U) == 0U) {
        bl_pending_word       = word;
        bl_pending_word_valid = true;
    } else {
        uint8_t double_word[BL_FLASH_WRITE_SIZE] = {0};
        memcpy(double_word, &bl_pending_word, sizeof(bl_pending_word));
        memcpy(double_word + sizeof(bl_pending_word), &word, sizeof(word));
        if (!PHAL_FLASH_write(BL_APP_ADDRESS + (index - 1U) * BL_WORD_SIZE,
                              double_word,
                              sizeof(double_word))) {
            bl_update_active = false;
            next_state       = BL_STATE_RECOVERY;
            bl_send_status(BOOTLOADER_STATUS_ERROR, BLERROR_FLASH);
            return false;
        }
        bl_pending_word_valid = false;
    }

    bl_next_word++;
    return true;
}

/* Round a logical image size up for its final 8-byte flash write. */
static uint32_t bl_physical_size(uint32_t size_bytes) {
    return (size_bytes + BL_FLASH_WRITE_SIZE - 1U) & ~(BL_FLASH_WRITE_SIZE - 1U);
}

/* Erase the metadata region to invalidate the application commit record. */
static bool bl_invalidate_metadata(void) {
    return PHAL_FLASH_erase(BL_METADATA_ADDRESS, BL_METADATA_REGION_SIZE);
}

/*
 * Invalidate the current application before erasing pages for its new image. A
 * reset or power loss after this point cannot launch the partially received
 * image because the metadata commit record is already erased.
 */
static bool bl_begin_update(uint32_t size_bytes) {
    if (!bl_size_is_valid(size_bytes) || !bl_range_is_valid(BL_APP_ADDRESS, size_bytes)) {
        bl_send_status(BOOTLOADER_STATUS_ERROR, BLERROR_SIZE);
        return false;
    }

    uint32_t physical_size = bl_physical_size(size_bytes);
    if (!bl_range_is_valid(BL_APP_ADDRESS, physical_size)) {
        bl_send_status(BOOTLOADER_STATUS_ERROR, BLERROR_SIZE);
        return false;
    }

    bl_update_active      = false;
    bl_pending_word_valid = false;

    if (!bl_invalidate_metadata() || !PHAL_FLASH_erase(BL_APP_ADDRESS, physical_size)) {
        bl_send_status(BOOTLOADER_STATUS_ERROR, BLERROR_FLASH);
        return false;
    }

    bl_firmware_size = size_bytes;
    bl_total_words   = size_bytes / BL_WORD_SIZE;
    bl_next_word     = 0U;
    bl_update_active = true;
    bl_send_status(BOOTLOADER_STATUS_ACK, size_bytes);
    return true;
}

/* Write the commit record after the application has passed every validation. */
static bool bl_write_metadata(uint32_t crc32, uint32_t size_bytes) {
    BootloaderMetadata_t metadata = {
        .magic          = BOOTLOADER_METADATA_MAGIC,
        .format_version = BOOTLOADER_METADATA_FORMAT_VERSION,
        .flags          = BOOTLOADER_METADATA_FLAG_INSTALLED_BY_BOOTLOADER,
        .crc32          = crc32,
        .address        = BL_APP_ADDRESS,
        .size_bytes     = size_bytes,
    };

    return PHAL_FLASH_write(BL_METADATA_ADDRESS, &metadata, sizeof(metadata));
}

static bool
bl_application_crc_is_valid(uint32_t address, uint32_t size_bytes, uint32_t expected_crc) {
    if (!bl_range_is_valid(address, size_bytes)) {
        return false;
    }

    uint32_t actual_crc =
        PHAL_CRC_calculate((const uint32_t *)(uintptr_t)address, size_bytes / BL_WORD_SIZE);
    return actual_crc == expected_crc;
}

/* Require a valid SRAM stack pointer and Thumb reset handler in the image. */
static bool bl_vector_is_valid(uint32_t app_address, uint32_t size_bytes) {
    if (size_bytes < (2U * BL_WORD_SIZE) || !bl_range_is_valid(app_address, size_bytes)) {
        return false;
    }

    uint32_t stack_pointer = *(const volatile uint32_t *)(uintptr_t)app_address;
    uint32_t reset_handler = *(const volatile uint32_t *)(uintptr_t)(app_address + 4U);
    uint32_t reset_address = reset_handler & ~1U;
    uint32_t app_end       = app_address + size_bytes;

    return stack_pointer >= BL_RAM_START && stack_pointer <= (BL_RAM_END + 1U)
        && (reset_handler & 1U) != 0U && reset_address >= app_address && reset_address < app_end;
}

/* Verify the complete direct write and commit metadata only after validation. */
static bool bl_commit_update(uint32_t expected_crc) {
    if (!bl_update_active || bl_next_word != bl_total_words) {
        bl_send_status(BOOTLOADER_STATUS_ERROR, BLERROR_SEQUENCE);
        return false;
    }

    if (!bl_flush_pending_word()) {
        bl_update_active = false;
        next_state       = BL_STATE_RECOVERY;
        return false;
    }

    uint32_t actual_crc =
        PHAL_CRC_calculate((const uint32_t *)(uintptr_t)BL_APP_ADDRESS, bl_total_words);
    if (actual_crc != expected_crc) {
        bl_update_active = false;
        next_state       = BL_STATE_RECOVERY;
        bl_send_status(BOOTLOADER_STATUS_CRC_ERROR, actual_crc);
        return false;
    }

    if (!bl_vector_is_valid(BL_APP_ADDRESS, bl_firmware_size)) {
        bl_update_active = false;
        next_state       = BL_STATE_RECOVERY;
        bl_send_status(BOOTLOADER_STATUS_ERROR, BLERROR_ADDRESS);
        return false;
    }

    if (!bl_application_crc_is_valid(BL_APP_ADDRESS, bl_firmware_size, actual_crc)
        || !bl_write_metadata(actual_crc, bl_firmware_size)) {
        bl_update_active = false;
        next_state       = BL_STATE_RECOVERY;
        bl_send_status(BOOTLOADER_STATUS_ERROR, BLERROR_FLASH);
        return false;
    }

    bl_update_active = false;
    next_state       = BL_STATE_READY;
    bl_send_status(BOOTLOADER_STATUS_ACK, actual_crc);
    return true;
}

/* Validate metadata, vectors, and CRC before transferring control. */
bool BL_checkAndBoot(void) {
    PHAL_CRC_init();
    BootloaderMetadata_t metadata = {0};
    if (!PHAL_FLASH_read(BL_METADATA_ADDRESS, &metadata, sizeof(metadata))) {
        return false;
    }

    if (metadata.magic != BOOTLOADER_METADATA_MAGIC
        || metadata.format_version != BOOTLOADER_METADATA_FORMAT_VERSION
        || (metadata.flags & BOOTLOADER_METADATA_FLAG_INSTALLED_BY_BOOTLOADER) == 0U
        || metadata.address != BL_APP_ADDRESS
        || !bl_vector_is_valid(metadata.address, metadata.size_bytes)) {
        return false;
    }

    uint32_t calculated_crc = PHAL_CRC_calculate((const uint32_t *)(uintptr_t)metadata.address,
                                                 metadata.size_bytes / BL_WORD_SIZE);
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
    SysTick->VAL  = 0U;

    __disable_irq();
    SCB->VTOR = metadata.address;
    __set_MSP(*(const volatile uint32_t *)(uintptr_t)metadata.address);
    ((void (*)(void))(uintptr_t)app_reset_handler)();

    return false;
}

/* Decode the small wire format without linking application CAN drivers. */
static uint32_t bl_uint32_data(const CanMsgTypeDef_t *message) {
    return ((uint32_t)message->Data[0] << 0U) | ((uint32_t)message->Data[1] << 8U)
        | ((uint32_t)message->Data[2] << 16U) | ((uint32_t)message->Data[3] << 24U);
}

static void bl_process_message(const CanMsgTypeDef_t *message) {
    if (message == NULL) {
        return;
    }

    uint32_t message_id = bl_message_id(message);
    if (message_id == bl_transport.start_id && message->DLC >= 4U) {
        bl_boot_status_requested = false;
        if (bl_begin_update(bl_uint32_data(message))) {
            next_state = BL_STATE_UPDATING;
        } else {
            next_state = BL_STATE_RECOVERY;
        }
        return;
    }
    if (message_id == bl_transport.crc_id && message->DLC >= 4U) {
        (void)bl_commit_update(bl_uint32_data(message));
        return;
    }
    if (message_id == bl_transport.jump_id && message->DLC >= 4U) {
        bl_boot_status_requested = true;
        next_state               = BL_STATE_CHECKING;
        return;
    }

    if (message_id == bl_transport.data_id && (message->DLC == 6U || message->DLC == 7U)) {
        /* DLC 6 is the legacy uint16 index layout; DLC 7 carries the new
         * uint24 index needed for the full application slot. */
        uint32_t index;
        uint32_t word;
        if (message->DLC == 7U) {
            index = ((uint32_t)message->Data[0] << 0U) | ((uint32_t)message->Data[1] << 8U)
                | ((uint32_t)message->Data[2] << 16U);
            word = ((uint32_t)message->Data[3] << 0U) | ((uint32_t)message->Data[4] << 8U)
                | ((uint32_t)message->Data[5] << 16U) | ((uint32_t)message->Data[6] << 24U);
        } else {
            index = ((uint32_t)message->Data[0] << 0U) | ((uint32_t)message->Data[1] << 8U);
            word = ((uint32_t)message->Data[2] << 0U) | ((uint32_t)message->Data[3] << 8U)
                | ((uint32_t)message->Data[4] << 16U) | ((uint32_t)message->Data[5] << 24U);
        }
        (void)bl_write_word(index, word);
    }
}

void SysTick_Handler(void) {
    bl_millis++;
}

/* Drain frames in main context, where flash operations are safe. */
static void bl_process_queued_messages(void) {
    while (bl_rx_tail != bl_rx_head) {
        CanMsgTypeDef_t message;

        /* Keep the ISR from advancing head while the volatile frame is copied. */
        __disable_irq();
        message    = bl_rx_queue[bl_rx_tail];
        bl_rx_tail = (uint8_t)((bl_rx_tail + 1U) % BL_RX_QUEUE_LENGTH);
        __enable_irq();

        bl_process_message(&message);
    }
}

/*
 * Run one resident FSM step. Startup polling is non-blocking; flash operations
 * remain in the main context, and application launch is an explicit state.
 */
void BL_poll(void) {
    current_state = next_state;
    next_state    = current_state; /* Default to an explicit self-loop. */

    switch (current_state) {
        case BL_STATE_STARTUP:
            bl_process_queued_messages();
            if (next_state == BL_STATE_STARTUP && !bl_update_active
                && (uint32_t)(bl_millis - bl_startup_start_ms) >= BL_STARTUP_WINDOW_MS) {
                bl_boot_status_requested = false;
                next_state               = BL_STATE_CHECKING;
            }
            break;

        case BL_STATE_READY:
        case BL_STATE_UPDATING:
        case BL_STATE_RECOVERY:
            bl_process_queued_messages();
            break;

        case BL_STATE_CHECKING: {
            bl_process_queued_messages();
            if (next_state != BL_STATE_CHECKING) {
                break;
            }

            bool report_failure      = bl_boot_status_requested;
            bl_boot_status_requested = false;
            if (!BL_checkAndBoot()) {
                if (report_failure) {
                    bl_send_status(BOOTLOADER_STATUS_ERROR, BLERROR_ADDRESS);
                }
                next_state = BL_STATE_RECOVERY;
            }
            break;
        }

        default:
            next_state = BL_STATE_RECOVERY;
            break;
    }

    if (!bl_update_active
        && (!bl_info_sent
            || (uint32_t)(bl_millis - bl_last_info_ms) >= BOOTLOADER_INFO_PERIOD_MS)) {
        bl_send_info();
        bl_last_info_ms = bl_millis;
        bl_info_sent    = true;
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

/* Initialize this target's single configured transport and announce readiness. */
void BL_init(void) {
    bl_rx_head = 0U;
    bl_rx_tail = 0U;

    PHAL_GPIO_InitConfig_t can_gpio[] = {bl_transport.rx_gpio, bl_transport.tx_gpio};
    (void)PHAL_GPIO_init(can_gpio, sizeof(can_gpio) / sizeof(can_gpio[0]));

    PHAL_FDCAN_init(bl_transport.peripheral, bl_transport.baud_rate);
    uint32_t filter_ids[] = {
        bl_transport.start_id,
        bl_transport.crc_id,
        bl_transport.jump_id,
        bl_transport.data_id,
    };
    if (bl_transport.is_extended_id) {
        (void)PHAL_FDCAN_setFilters(bl_transport.peripheral,
                                    NULL,
                                    0U,
                                    filter_ids,
                                    sizeof(filter_ids) / sizeof(filter_ids[0]));
    } else {
        (void)PHAL_FDCAN_setFilters(bl_transport.peripheral,
                                    filter_ids,
                                    sizeof(filter_ids) / sizeof(filter_ids[0]),
                                    NULL,
                                    0U);
    }
    bl_enable_irq(bl_transport.peripheral);

    PHAL_CRC_init();
    (void)SysTick_Config(PHAL_RCC_getAHBClockHz() / 1000U);
    bl_millis                = 0U;
    bl_startup_start_ms      = bl_millis;
    bl_last_info_ms          = 0U;
    bl_info_sent             = false;
    bl_boot_status_requested = false;
    bl_update_active         = false;
    bl_pending_word_valid    = false;
    current_state            = BL_STATE_STARTUP;
    next_state               = BL_STATE_STARTUP;
    bl_send_status(BOOTLOADER_STATUS_READY, BOOTLOADER_PROTOCOL_VERSION);
    bl_send_info();
    bl_last_info_ms = bl_millis;
    bl_info_sent    = true;
}

/*
 * ISR-side queue producer. Publish head only after the complete frame copy so
 * BL_poll() never observes a partial message. Drop on overflow rather than
 * transmitting, blocking, or performing flash work in interrupt context.
 */
void PHAL_FDCAN_rxCallback(CanMsgTypeDef_t *message) {
    if (!bl_message_is_bootloader_traffic(message)) {
        return;
    }

    uint8_t next_head = (uint8_t)((bl_rx_head + 1U) % BL_RX_QUEUE_LENGTH);
    if (next_head == bl_rx_tail) {
        return;
    }

    bl_rx_queue[bl_rx_head] = *message;
    bl_rx_head              = next_head;
}
