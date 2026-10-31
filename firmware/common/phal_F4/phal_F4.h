#ifndef _PHAL_F4_H
#define _PHAL_F4_H

#include <stdbool.h>
#include <stdint.h>

#if defined(STM32F423xx)
#include "stm32f423xx.h"
#include "stm32f4xx.h"
#else
#error "PHAL_ARCH_F4 defined, but no supported STM32F4 device macro found"
#endif

#endif // _PHAL_F4_H
