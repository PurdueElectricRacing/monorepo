#include "g4_testing.h"
#if (G4_TESTING_CHOSEN == TEST_THREAD_LOCK)

/**
 * Small scale version of the can_data mutex scheme:
 *  - one writer task plays the role of CAN_rx_dispatcher, unpacking a
 *    multi-field "message" into a shared struct while holding the mutex
 *  - five reader tasks play the role of app code, grabbing a snapshot
 *    through TEST_DATA_GET() (same shape as CAN_DATA_GET) and checking
 *    that every field came from the same frame
 *
 * The writer yields halfway through the unpack to force the worst case
 * (getting preempted mid-message). Set THREAD_LOCK_USE_MUTEX to 0 to see
 * the readers catch torn frames.
 *
 * LEDs:
 *  - blue:   writer heartbeat
 *  - orange: reader heartbeat
 *  - green:  on while no torn reads have been seen
 *  - red:    latched on once any reader sees a torn frame
 *
 * Watch writes, reads[], and torn_reads[] in the debugger for exact counts.
 */

#include <stdint.h>

#include "common/rtos/rtos.h"
#include "common/phal_G4/gpio/gpio.h"
#include "common/phal_G4/rcc/rcc.h"
#include "common/utils/countof.h"
#include "main.h"

// 1 = guard shared data with the mutex, 0 = unguarded (expect torn reads)
#define THREAD_LOCK_USE_MUTEX 1

#define NUM_READERS 5

PHAL_GPIO_InitConfig_t gpio_config[] = {
    PHAL_GPIO_INIT_OUTPUT(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_RED_PORT, LED_RED_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_BLUE_PORT, LED_BLUE_PIN, GPIO_OUTPUT_LOW_SPEED),
    PHAL_GPIO_INIT_OUTPUT(LED_ORANGE_PORT, LED_ORANGE_PIN, GPIO_OUTPUT_LOW_SPEED),
};

// Stand-in for a generated <msg>_data_t, every field holds the same sequence number
typedef struct {
    uint32_t field_a;
    uint32_t field_b;
    uint32_t field_c;
    uint32_t last_rx;
} test_msg_data_t;

// Stand-ins for can_data / can_data_mutex
volatile test_msg_data_t test_data;
RTOS_DEFINE_MUTEX(test_data_mutex);

#if THREAD_LOCK_USE_MUTEX
#define TEST_DATA_LOCK()   xSemaphoreTake(test_data_mutex, portMAX_DELAY)
#define TEST_DATA_UNLOCK() xSemaphoreGive(test_data_mutex)
#else
#define TEST_DATA_LOCK()
#define TEST_DATA_UNLOCK()
#endif

// Same shape as the generated CAN_data_get_<msg>() accessor
static inline test_msg_data_t TEST_data_get(void) {
    TEST_DATA_LOCK();
    test_msg_data_t snapshot = test_data;
    TEST_DATA_UNLOCK();
    return snapshot;
}

#define TEST_DATA_GET() TEST_data_get()

// Debug counters
volatile uint32_t writes;
volatile uint32_t reads[NUM_READERS];
volatile uint32_t torn_reads[NUM_READERS];

void HardFault_Handler();

static void writer(void);
static void reader1(void);
static void reader2(void);
static void reader3(void);
static void reader4(void);
static void reader5(void);

RTOS_DEFINE_TASK(writer, 5, TASK_PRIORITY_NORMAL, STACK_256);
RTOS_DEFINE_TASK(reader1, 1, TASK_PRIORITY_NORMAL, STACK_256);
RTOS_DEFINE_TASK(reader2, 2, TASK_PRIORITY_NORMAL, STACK_256);
RTOS_DEFINE_TASK(reader3, 3, TASK_PRIORITY_NORMAL, STACK_256);
RTOS_DEFINE_TASK(reader4, 4, TASK_PRIORITY_NORMAL, STACK_256);
RTOS_DEFINE_TASK(reader5, 0, TASK_PRIORITY_NORMAL, STACK_256); // runs back to back

int main() {
    PHAL_RCC_init(PHAL_RCC_HSI_16MHZ);

    if (!PHAL_GPIO_init(gpio_config, countof(gpio_config))) {
        HardFault_Handler();
    }

    PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, 1);
    PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, 0);
    PHAL_GPIO_write(LED_BLUE_PORT, LED_BLUE_PIN, 0);
    PHAL_GPIO_write(LED_ORANGE_PORT, LED_ORANGE_PIN, 0);

    RTOS_INIT_MUTEX(test_data_mutex);

    // Create threads
    RTOS_START_TASK(writer);
    RTOS_START_TASK(reader1);
    RTOS_START_TASK(reader2);
    RTOS_START_TASK(reader3);
    RTOS_START_TASK(reader4);
    RTOS_START_TASK(reader5);

    vTaskStartScheduler();

    return 0;
}

// Mirrors CAN_rx_dispatcher: lock, unpack every signal, stamp last_rx, unlock
static void writer(void) {
    static uint32_t seq = 0;
    seq++;

    TEST_DATA_LOCK();
    test_data.field_a = seq;
    test_data.field_b = seq;
    // simulate getting preempted mid-unpack, readers get the CPU here
    taskYIELD();
    test_data.field_c = seq;
    test_data.last_rx = xTaskGetTickCount();
    TEST_DATA_UNLOCK();

    writes++;
    if ((writes % 100) == 0) {
        PHAL_GPIO_toggle(LED_BLUE_PORT, LED_BLUE_PIN);
    }
}

static void reader_common(uint32_t idx) {
    // one locked copy so every field comes from the same frame
    const test_msg_data_t snapshot = TEST_DATA_GET();

    reads[idx]++;

    if ((snapshot.field_a != snapshot.field_b) || (snapshot.field_b != snapshot.field_c)) {
        torn_reads[idx]++;
        PHAL_GPIO_write(LED_RED_PORT, LED_RED_PIN, 1);
        PHAL_GPIO_write(LED_GREEN_PORT, LED_GREEN_PIN, 0);
    }

    if (idx == 0 && (reads[idx] % 250) == 0) {
        PHAL_GPIO_toggle(LED_ORANGE_PORT, LED_ORANGE_PIN);
    }
}

static void reader1(void) {
    reader_common(0);
}

static void reader2(void) {
    reader_common(1);
}

static void reader3(void) {
    reader_common(2);
}

static void reader4(void) {
    reader_common(3);
}

static void reader5(void) {
    reader_common(4);
}

void HardFault_Handler() {
    while (1) {
        __asm__("nop");
    }
}

#endif // G4_TESTING_CHOSEN == TEST_THREAD_LOCK
