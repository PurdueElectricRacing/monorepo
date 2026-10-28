
#include "bootloader/bootloader.h"
#include "common/phal_G4/rcc/rcc.h"

int main(void) {
    uint32_t reset_flags = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;
    // Deliberately 16 MHz, unlike the 170 MHz apps: CAN, not the CPU, limits
    // update speed, and keeping the recovery path's clock tree simple means a
    // clock bug can't break over-CAN updates. The app re-inits its own clocks.
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    if ((reset_flags & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF)) != 0U) {
        (void)BL_checkAndBoot();
    }

    BL_init();
    for (;;) {
        BL_poll();
    }
}
