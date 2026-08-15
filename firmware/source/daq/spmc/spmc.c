/**
 * @file spmc.c
 * @brief Custom SPMC designed for high throughput handling of CAN message streams.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include "spmc.h"

#include "common/rtos/rtos.h"
#include "stm32f407xx.h"

// Singleton global allocation
SPMC_t g_spmc;

static constexpr uint32_t CAN_RX_IRQ_PRIO = 6; // highest RTOS priority
static_assert(
    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY < CAN_RX_IRQ_PRIO,
    "Do not set an IRQ priority higher (numerically lower) than FreeRTOS to prevent corruption of kernel data structures"
);

/**
 * @brief Helper function to compute the number of items in the buffer given head and tail indices, accounting for wraparound.
 */
[[gnu::always_inline]]
static inline size_t get_count(size_t head, size_t tail, bool is_full) {
    if (head == tail) {
        return is_full ? SPMC_FRAME_CAPACITY : 0;
    } else if (head > tail) {
        return head - tail;
    } else {
        return SPMC_FRAME_CAPACITY - tail + head;
    }
}

/**
 * @brief Initializes the SPMC instance and configures CAN RX interrupts.
 *
 * @param spmc Pointer to the SPMC instance to initialize.
 */
void SPMC_init(SPMC_t *spmc) {
    spmc->head = 0;
    spmc->master_tail = 0;
    spmc->follower_tail = 0;
    spmc->overflows = 0;
    spmc->follower_drops = 0;
    spmc->is_full = false;

    // CAN1 and CAN2 RX0 IRQs must be set to the same priority to hold the SPMC single producer assumption
    NVIC_SetPriority(CAN1_RX0_IRQn, CAN_RX_IRQ_PRIO);
    NVIC_SetPriority(CAN2_RX0_IRQn, CAN_RX_IRQ_PRIO);
    NVIC_EnableIRQ(CAN1_RX0_IRQn);
    NVIC_EnableIRQ(CAN2_RX0_IRQn);
}

/**
 * @brief Enqueues a received CAN message into the SPMC buffer from an ISR context.
 * ! the two producer ISRs must have the same NVIC priority to prevent preemption of each other in the middle of a write
 * 
 * @param spmc Pointer to the SPMC instance.
 * @param incoming_frame Pointer to the timestamped_frame_t containing the received CAN message.
 *
 * @return true if the frame was enqueued successfully, or false if the buffer is full and the frame was dropped.
 */
bool SPMC_enqueue_from_ISR(SPMC_t *spmc, timestamped_frame_t *incoming_frame) {
    if (spmc->is_full) { // atomic load
        spmc->overflows++;
        spmc->follower_drops++;
        return false; // ! the frame is dropped
    }

    const size_t head = spmc->head; // atomic load
    const size_t master_tail = spmc->master_tail; // atomic load

    // calc next head and account for wraparound
    size_t next_head = head + 1;
    if (next_head == SPMC_FRAME_CAPACITY) {
        next_head = 0;
    }
    
    // todo: notify the SD thread from here?

    // write the frame into the buffer
    spmc->data[head] = *incoming_frame;
    __DMB(); // do not switch the order of the data write and head update
    spmc->head    = next_head;
    spmc->is_full = (next_head == master_tail);

    return true;
}

/**
 * @brief Peeks at the next chunk for the master (SD logging) without committing the tail.
 * 
 * @param spmc Pointer to the SPMC instance.
 * @param first_item Output pointer that will point to the first item in the chunk if available, or NULL if no items are available.
 *
 * @return true if a chunk is available, false if no items/not enough are available
 */
bool SPMC_master_peek_chunk(SPMC_t *spmc, timestamped_frame_t **first_item) {
    uint32_t basepri = __get_BASEPRI();
    __set_BASEPRI(CAN_RX_IRQ_PRIO << (8 - __NVIC_PRIO_BITS));

    // snapshot the shared state of the spmc
    const size_t head = spmc->head;
    __DMB(); // dont reorder head and tail reads
    const size_t master_tail = spmc->master_tail;
    const bool is_full = spmc->is_full;

    __set_BASEPRI(basepri);

    // compute the number of items in the buffer
    const size_t count = get_count(head, master_tail, is_full);

    // not enough data for even one chunk
    if (count < SPMC_CHUNK_NUM_FRAMES) {
        *first_item = NULL;
        return false;
    }

    *first_item = &spmc->data[master_tail];
    return true;
}

/**
 * @brief Advances the master tail pointer by one chunk.
 * @warning this function should only be called by the master consumer context
 *
 * @param spmc Pointer to the SPMC instance.
 */
void SPMC_master_advance_tail(SPMC_t *spmc) {
    // mask CAN_RX_IRQ priority level and all lower-priority interrupts (including PendSV context switch)
    uint32_t basepri = __get_BASEPRI();
    __set_BASEPRI(CAN_RX_IRQ_PRIO << (8 - __NVIC_PRIO_BITS));

    const size_t master_tail = spmc->master_tail;

    size_t next_tail = (master_tail + SPMC_CHUNK_NUM_FRAMES);
    if (next_tail >= SPMC_FRAME_CAPACITY) {
        next_tail -= SPMC_FRAME_CAPACITY;
    }

    spmc->master_tail = next_tail;
    __DMB();
    spmc->is_full = false; // if we were full, we must now have space

    __set_BASEPRI(basepri);
}

/**
 * @brief Peeks at the number of contiguous mini chunks available for the follower without lapping the master's tail without committing the tail.
 *
 * @return The number of contiguous mini chunks available.
 */
size_t SPMC_follower_peek_minis(SPMC_t *spmc, timestamped_frame_t **first_item) {
    uint32_t basepri = __get_BASEPRI();
    __set_BASEPRI(CAN_RX_IRQ_PRIO << (8 - __NVIC_PRIO_BITS));

    // snapshot the shared state of the spmc
    const size_t head        = spmc->head;
    __DMB(); // dont reorder head and tail reads
    const size_t master_tail = spmc->master_tail;
    const size_t follower_tail = spmc->follower_tail;
    const bool is_full       = spmc->is_full;

    __set_BASEPRI(basepri);

    // tail catchup logic
    size_t follower_count     = get_count(head, follower_tail, is_full);
    const size_t master_count = get_count(head, master_tail, is_full);
    size_t new_follower_tail = follower_tail;
    if (follower_count > master_count) { // follower is behind master_tail, so old data was already overwritten/lost
        const size_t dropped = follower_count - master_count;
        spmc->follower_drops += (uint32_t)dropped;

        new_follower_tail  = master_tail;
        follower_count = master_count;
        spmc->follower_tail = new_follower_tail; // commit the catchup
    }

    // not enough data for even one mini chunk
    if (follower_count < SPMC_MINI_NUM_FRAMES) {
        *first_item = NULL;
        return 0;
    }

    // calc contiguous frames available before wrap
    size_t contiguous_frames = SPMC_FRAME_CAPACITY - new_follower_tail;
    if (contiguous_frames > follower_count) {
        contiguous_frames = follower_count;
    }

    const size_t contiguous_minis = contiguous_frames / SPMC_MINI_NUM_FRAMES;

    *first_item = &spmc->data[new_follower_tail];
    return contiguous_minis;
}

/**
 * @brief Advances the follower tail pointer by a given number of mini chunks.
 */
void SPMC_follower_advance_tail(SPMC_t *spmc, size_t minis_consumed) {
    // no critical section needed because follower tail is not shared with other contexts
    const size_t follower_tail = spmc->follower_tail;

    size_t next_tail = follower_tail + (minis_consumed * SPMC_MINI_NUM_FRAMES);
    while (next_tail >= SPMC_FRAME_CAPACITY) {
        next_tail -= SPMC_FRAME_CAPACITY;
    }

    spmc->follower_tail = next_tail;
}