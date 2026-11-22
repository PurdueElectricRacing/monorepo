#include "stm32g4xx.h"

#include "g4_testing.h"

void HardFault_Handler(void) {
    __disable_irq();
    SysTick->CTRL = 0;
    while (1) {
        __asm__("nop");
    }
}
