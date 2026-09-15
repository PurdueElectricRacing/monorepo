
#include "bootloader/bootloader.h"
#include "common/phal_G4/rcc/rcc.h"

int main(void) {
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    BL_init();
    for (;;) {
        BL_poll();
    }
}
