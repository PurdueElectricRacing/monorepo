#ifndef __F4_TESTING__
#define __F4_TESTING__

// To add new tests create a separate file (see led_blink.c) and it to the enum here
#define TEST_FREERTOS_DEMO 0
#define TEST_ONBOARDING_26 1
#define TEST_PWM 2

// Change this define to set the test compiled
#ifndef F4_TESTING_CHOSEN
#define F4_TESTING_CHOSEN TEST_ONBOARDING_26
#endif

#endif // __F4_TESTING__
