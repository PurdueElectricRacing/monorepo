/**
 * @file main.c
 * @brief Startup and recovery loop for resident G4 bootloader images.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#include "bootloader/bootloader.h"
#include "common/bootloader/bootloader_common.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/phal_G4/phal_G4.h"

int main(void) {
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    /* The download marker is one-shot. */
    bool requested_update = bootloader_shared_memory.magic_word == BOOTLOADER_SHARED_MEMORY_MAGIC
        && bootloader_shared_memory.reset_reason == RESET_REASON_DOWNLOAD_FW;
    bootloader_shared_memory.magic_word = 0U;
    bootloader_shared_memory.reset_reason = RESET_REASON_INVALID;

    /* Skip normal launch only for an explicit application update request. */
    if (!requested_update) {
        (void)BL_checkAndBoot();
    }

    /* Invalid applications and update requests share the recovery loop. */
    BL_init();
    for (;;) {
        BL_poll();
    }
}
