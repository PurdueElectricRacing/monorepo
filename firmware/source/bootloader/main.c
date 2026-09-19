
#include "bootloader/bootloader.h"
#include "common/phal_G4/rcc/rcc.h"

int main(void) {
    uint32_t reset_flags = RCC->CSR;
    PHAL_RCC_init(PHAL_RCC_HSE_16MHZ);

    if ((reset_flags & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF)) != 0U) {
        (void)BL_checkAndBoot();
    }

    BL_init();
    for (;;) {
        BL_poll();
    }
}
