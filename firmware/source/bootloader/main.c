/**
 * @file main.c
 * @brief Startup and recovery loop for resident G4 bootloader images.
 * @author Ronak Jain (jain717@purdue.edu)
 */

#include "bootloader/bootloader.h"
#include "common/phal_G4/rcc/rcc.h"

int main(void) {
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    /* BL_poll() owns the startup decision and resident recovery FSM. */
    BL_init();
    for (;;) {
        BL_poll();
    }
}
