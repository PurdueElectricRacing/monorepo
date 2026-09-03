#ifndef __G4_TESTING__
#define __G4_TESTING__

// To add new tests create a separate file (see led_blink.c) and add it to the enum here
#define TEST_BLINKY          0
#define TEST_FDCAN           1
#define TEST_USART           2
#define TEST_SPI             3
#define TEST_CANPILER        4
#define IZZE_IMU_CONFIG      5
#define TEST_FLASH           6
#define TEST_FREERTOS        7
#define TEST_CRC             8
#define TEST_USB             9
#define TEST_PWM             10
#define TEST_ADC             11
#define TEST_GPIO            12
#define TEST_ONBOARDING_2027 13

// Change this define to set the test compiled
#define G4_TESTING_CHOSEN TEST_ONBOARDING_2027

#endif // __G4_TESTING__
