/**
 * @file main.c
 * @brief Startup and recovery loop for resident G4 bootloader images.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#include "bootloader/bootloader.h"
#include "common/phal_G4/rcc/rcc.h"

int main(void) {
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    /* Initialize CAN and announce READY before deciding whether to launch. */
    BL_init();

    /* DaqApp receives READY, then resends START to enter update mode. */
    if (!BL_waitForUpdate()) {
        (void)BL_checkAndBoot();
    }

    /* Invalid applications and active updates share the recovery loop. */
    for (;;) {
        BL_poll();
    }
}
