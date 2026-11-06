/**
 * @file bootloader_app.c
 * @brief Application-side handlers for reprogramming the resident bootloader.
 *
 * The resident bootloader executes from the region it would have to erase, so
 * only a running application can replace it. These handlers receive the same
 * START/DATA/CRC/JUMP sequence as an application update and stream the image
 * directly into BL_BOOTLOADER_ADDRESS. There is no staging slot: see the
 * resident bootloader guide for the recovery limitations of this operation.
 */

#include "common/bootloader/bootloader_common.h"

#if defined(BOOTLOADER_ENABLED)

#include <string.h>

#include "common/phal_G4/crc/crc.h"
#include "common/phal_G4/flash/flash.h"
#include "stm32g474xx.h"

/* SRAM bounds used to reject an image with an implausible initial MSP. */
#define BL_RAM_START 0x20000000U
#define BL_RAM_END   0x2001FFFFU

#if defined(CAN_NODE_MAIN_MODULE)
#include "can_library/generated/MAIN_MODULE.h"
#define BL_APP_SEND_RESPONSE(status, detail) \
    CAN_SEND_bl_main_module_bootloader_resp((bootloader_status_t)(status), (detail))
#elif defined(CAN_NODE_DASHBOARD)
#include "can_library/generated/DASHBOARD.h"
#define BL_APP_SEND_RESPONSE(status, detail) \
    CAN_SEND_bl_dashboard_bootloader_resp((bootloader_status_t)(status), (detail))
#elif defined(CAN_NODE_TORQUE_VECTOR)
#include "can_library/generated/TORQUE_VECTOR.h"
#define BL_APP_SEND_RESPONSE(status, detail) \
    CAN_SEND_bl_torque_vector_bootloader_resp((bootloader_status_t)(status), (detail))
#elif defined(CAN_NODE_A_BOX)
#include "can_library/generated/A_BOX.h"
#define BL_APP_SEND_RESPONSE(status, detail) \
    CAN_SEND_bl_a_box_bootloader_resp((bootloader_status_t)(status), (detail))
#elif defined(CAN_NODE_DRIVELINE)
#include "can_library/generated/DRIVELINE.h"
#if defined(IS_FRONT_DRIVELINE)
#define BL_APP_SEND_RESPONSE(status, detail) \
    CAN_SEND_bl_front_driveline_bootloader_resp((bootloader_status_t)(status), (detail))
#elif defined(IS_REAR_DRIVELINE)
#define BL_APP_SEND_RESPONSE(status, detail) \
    CAN_SEND_bl_rear_driveline_bootloader_resp((bootloader_status_t)(status), (detail))
#else
#error "A driveline build must define IS_FRONT_DRIVELINE or IS_REAR_DRIVELINE"
#endif
#else
#error "bootloader_app.c requires a bootloader-aware CAN node"
#endif

static bool bl_app_update_active;
static bool bl_app_image_ready;
static uint32_t bl_app_total_words;
static uint32_t bl_app_next_word;
static uint32_t bl_app_pending_word;
static bool bl_app_pending_word_valid;

static void bl_app_send_ack(uint32_t detail) {
    BL_APP_SEND_RESPONSE(BOOTLOADER_STATUS_ACK, detail);
}

static void bl_app_send_error(BLError_t error) {
    BL_APP_SEND_RESPONSE(BOOTLOADER_STATUS_ERROR, (uint32_t)error);
}

static bool bl_app_size_is_valid(uint32_t size_bytes) {
    return size_bytes >= (2U * BL_WORD_SIZE) && (size_bytes % BL_WORD_SIZE) == 0U
        && size_bytes <= BL_BOOTLOADER_REGION_SIZE;
}

/*
 * STM32G4 flash writes are 64-bit. An unpaired final image word is followed by
 * erased-state 0xFF padding for the physical write while the logical image and
 * CRC still cover only the supplied words.
 */
static bool bl_app_flush_pending_word(void) {
    if (!bl_app_pending_word_valid) {
        return true;
    }

    uint8_t double_word[BL_FLASH_WRITE_SIZE];
    memset(double_word, 0xFF, sizeof(double_word));
    memcpy(double_word, &bl_app_pending_word, sizeof(bl_app_pending_word));
    bl_app_pending_word_valid = false;
    if (!PHAL_FLASH_write(BL_BOOTLOADER_ADDRESS + (bl_app_next_word - 1U) * BL_WORD_SIZE,
                          double_word,
                          sizeof(double_word))) {
        bl_app_update_active = false;
        bl_app_send_error(BLERROR_FLASH);
        return false;
    }
    return true;
}

/* Validate the initial MSP and reset vector of the received bootloader image. */
static bool bl_app_image_is_valid(uint32_t size_bytes) {
    const uint32_t *image = (const uint32_t *)(uintptr_t)BL_BOOTLOADER_ADDRESS;
    uint32_t stack_pointer  = image[0];
    uint32_t reset_handler  = image[1];

    if (stack_pointer < BL_RAM_START || stack_pointer > (BL_RAM_END + 1U)
        || (stack_pointer % BL_WORD_SIZE) != 0U) {
        return false;
    }
    if ((reset_handler & 1U) == 0U) {
        return false;
    }

    uint32_t reset_address = reset_handler & ~1U;
    return reset_address >= BL_BOOTLOADER_ADDRESS
        && reset_address < BL_BOOTLOADER_ADDRESS + size_bytes;
}

bool BL_bootloaderUpdateStart(uint32_t size_bytes) {
    if (!bl_app_size_is_valid(size_bytes)) {
        bl_app_send_error(BLERROR_SIZE);
        return false;
    }

    bl_app_update_active      = false;
    bl_app_image_ready        = false;
    bl_app_pending_word_valid = false;

    /* Erase the whole region so stale bytes outside size_bytes are cleared. */
    if (!PHAL_FLASH_erase(BL_BOOTLOADER_ADDRESS, BL_BOOTLOADER_REGION_SIZE)) {
        bl_app_send_error(BLERROR_FLASH);
        return false;
    }

    bl_app_total_words   = size_bytes / BL_WORD_SIZE;
    bl_app_next_word     = 0U;
    bl_app_update_active = true;
    bl_app_send_ack(size_bytes);
    return true;
}

bool BL_bootloaderUpdateWord(uint32_t index, uint32_t word) {
    if (!bl_app_update_active) {
        bl_app_send_error(BLERROR_LOCKED);
        return false;
    }
    if (index >= bl_app_total_words) {
        bl_app_send_error(BLERROR_ADDRESS);
        return false;
    }
    if (index < bl_app_next_word) {
        return true;
    }
    if (index != bl_app_next_word) {
        bl_app_update_active = false;
        bl_app_send_error(BLERROR_SEQUENCE);
        return false;
    }

    if ((index & 1U) == 0U) {
        bl_app_pending_word       = word;
        bl_app_pending_word_valid = true;
    } else {
        uint8_t double_word[BL_FLASH_WRITE_SIZE] = {0};
        memcpy(double_word, &bl_app_pending_word, sizeof(bl_app_pending_word));
        memcpy(double_word + sizeof(bl_app_pending_word), &word, sizeof(word));
        if (!PHAL_FLASH_write(BL_BOOTLOADER_ADDRESS + (index - 1U) * BL_WORD_SIZE,
                              double_word,
                              sizeof(double_word))) {
            bl_app_update_active = false;
            bl_app_send_error(BLERROR_FLASH);
            return false;
        }
        bl_app_pending_word_valid = false;
    }

    bl_app_next_word++;
    return true;
}

bool BL_bootloaderUpdateCrc(uint32_t expected_crc) {
    if (!bl_app_update_active) {
        bl_app_send_error(BLERROR_LOCKED);
        return false;
    }
    /* Erased trailing words can match the CRC even if they were never sent. */
    if (bl_app_next_word != bl_app_total_words) {
        bl_app_update_active = false;
        bl_app_send_error(BLERROR_SEQUENCE);
        return false;
    }
    if (!bl_app_flush_pending_word()) {
        return false;
    }

    uint32_t size_bytes = bl_app_total_words * BL_WORD_SIZE;
    uint32_t calculated = PHAL_CRC_calculate(
        (const uint32_t *)(uintptr_t)BL_BOOTLOADER_ADDRESS, bl_app_total_words);

    if (calculated != expected_crc) {
        bl_app_update_active = false;
        BL_APP_SEND_RESPONSE(BOOTLOADER_STATUS_CRC_ERROR, 0U);
        return false;
    }
    if (!bl_app_image_is_valid(size_bytes)) {
        bl_app_update_active = false;
        bl_app_send_error(BLERROR_ADDRESS);
        return false;
    }

    bl_app_update_active = false;
    bl_app_image_ready   = true;
    bl_app_send_ack(calculated);
    return true;
}

bool BL_bootloaderUpdateJump(void) {
    if (bl_app_image_ready) {
        NVIC_SystemReset();
    }
    return false;
}

#define BL_APP_DEFINE_CALLBACKS(prefix) \
    void prefix##_bootloader_start_CALLBACK(void) { \
        (void)BL_bootloaderUpdateStart(can_data.prefix##_bootloader_start.image_size_bytes); \
    } \
    void prefix##_bootloader_data_CALLBACK(void) { \
        (void)BL_bootloaderUpdateWord(can_data.prefix##_bootloader_data.index, \
                                      can_data.prefix##_bootloader_data.data); \
    } \
    void prefix##_bootloader_crc_CALLBACK(void) { \
        (void)BL_bootloaderUpdateCrc(can_data.prefix##_bootloader_crc.data); \
    } \
    void prefix##_bootloader_jump_CALLBACK(void) { \
        (void)BL_bootloaderUpdateJump(); \
    }

#define BL_APP_DEFINE_EMPTY_CALLBACKS(prefix) \
    void prefix##_bootloader_start_CALLBACK(void) {} \
    void prefix##_bootloader_data_CALLBACK(void) {} \
    void prefix##_bootloader_crc_CALLBACK(void) {} \
    void prefix##_bootloader_jump_CALLBACK(void) {}

#if defined(CAN_NODE_MAIN_MODULE)
BL_APP_DEFINE_CALLBACKS(bl_main_module)
#elif defined(CAN_NODE_DASHBOARD)
BL_APP_DEFINE_CALLBACKS(bl_dashboard)
#elif defined(CAN_NODE_TORQUE_VECTOR)
BL_APP_DEFINE_CALLBACKS(bl_torque_vector)
#elif defined(CAN_NODE_A_BOX)
BL_APP_DEFINE_CALLBACKS(bl_a_box)
#elif defined(CAN_NODE_DRIVELINE)
#if defined(IS_FRONT_DRIVELINE)
BL_APP_DEFINE_CALLBACKS(bl_front_driveline)
BL_APP_DEFINE_EMPTY_CALLBACKS(bl_rear_driveline)
#elif defined(IS_REAR_DRIVELINE)
BL_APP_DEFINE_EMPTY_CALLBACKS(bl_front_driveline)
BL_APP_DEFINE_CALLBACKS(bl_rear_driveline)
#endif
#endif

#else

/* Standalone images link the shared component but never handle updates. */
#define BL_APP_DEFINE_EMPTY_CALLBACKS(prefix) \
    void prefix##_bootloader_start_CALLBACK(void) {} \
    void prefix##_bootloader_data_CALLBACK(void) {} \
    void prefix##_bootloader_crc_CALLBACK(void) {} \
    void prefix##_bootloader_jump_CALLBACK(void) {}

#if defined(CAN_NODE_MAIN_MODULE)
BL_APP_DEFINE_EMPTY_CALLBACKS(bl_main_module)
#elif defined(CAN_NODE_DASHBOARD)
BL_APP_DEFINE_EMPTY_CALLBACKS(bl_dashboard)
#elif defined(CAN_NODE_TORQUE_VECTOR)
BL_APP_DEFINE_EMPTY_CALLBACKS(bl_torque_vector)
#elif defined(CAN_NODE_A_BOX)
BL_APP_DEFINE_EMPTY_CALLBACKS(bl_a_box)
#elif defined(CAN_NODE_DRIVELINE)
BL_APP_DEFINE_EMPTY_CALLBACKS(bl_front_driveline)
BL_APP_DEFINE_EMPTY_CALLBACKS(bl_rear_driveline)
#endif

#endif
