# F423 bench examples

`f4_testing` selects STM32F423RHT6, the F423 CMSIS/startup and standalone linker
layout. Select an example with `F4_TESTING_CHOSEN` in `f4_testing.h`.

Temporary LQFP64 assignments (external wiring required):

| Example | Signals | Pins |
| --- | --- | --- |
| Onboarding | LED output | PC0 |
| FreeRTOS demo | Four LED outputs | PC0, PC1, PC2, PC3 |
| FreeRTOS demo | USART2 TX / RX | PA2 / PA3, AF7 |
| PWM | Timer outputs | Existing PA/PB/PC AF assignments in `pwm_testing.c` |

The three examples are mutually exclusive. These mappings are for bench work;
they do not describe the DAQ schematic or an existing development board's LEDs.
The PWM example's PB3 output uses the SWO pin; keep SWD on PA13/PA14 and disable
SWO tracing when using that output. Check wiring, loads and voltage before use.
