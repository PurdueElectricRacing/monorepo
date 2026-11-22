#include "g4_testing.h"

void HardFault_Handler(void) {
    while (1) {
        __asm__("nop");
    }
}
