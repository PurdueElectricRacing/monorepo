/**
 * @file ethernet.c
 * @brief Implementation of the ethernet thread for wireless data acquisition.
 *
 * @author Luke Oxley (lcoxley@purdue.edu)
 * @author Eileen Yoon (eyn@purdue.edu)
 * @author Irving Wang (irvingw@purdue.edu)
 */

#include <stdint.h>
#include "w5500/socket.h"
#include "common/phal_F4/gpio/gpio.h"
#include "common/rtos/rtos.h"
#include "main.h"
#include "spmc.h"

#include "ethernet.h"

static constexpr uint8_t DAQ_UDP_SOCKET_NUM   = 0;
static constexpr uint16_t DAQ_UDP_SOCKET_PORT = 5005;

static ethernet_config_t config = {
    .net_info = {
        .mac = {0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef}, // Mac address
        .ip  = {192, 168, 10, 41}, // IP address of DAQ PCB
        .sn  = {255, 255, 255, 0}, // Subnet mask
        .gw  = {192, 168, 10, 1},  // Gateway address
    },
    .udp_address = {192, 168, 10, 255}, // Broadcast address
    .udp_port = DAQ_UDP_SOCKET_PORT,
    .udp_socket = DAQ_UDP_SOCKET_NUM
};

static_assert(
    DAQ_UDP_SOCKET_NUM < _WIZCHIP_SOCK_NUM_,
    "UDP socket number should be less than WIZCHIP socket max"
);

static ethernet_state_t current_state = ETHERNET_STATE_HW_INIT;
static ethernet_state_t next_state    = ETHERNET_STATE_HW_INIT;

static inline bool is_linked(void) {
    volatile uint8_t phycfgr = getPHYCFGR();
    return phycfgr & PHY_LINK_ON;
}

static bool init_w5500(void) {
    static constexpr uint8_t EXPECTED_W5500_VERSION_ID = 0x04;

    w5500_register_callbacks(); // register custom SPI callbacks for W5500 driver

    // hold and release ETH reset pin
    PHAL_GPIO_write(ETH_RST_PORT, ETH_RST_PIN, 0);
    RTOS_delay_ms(10);
    PHAL_GPIO_write(ETH_RST_PORT, ETH_RST_PIN, 1);
    RTOS_delay_ms(50);

    // Read back the version register
    if (getVERSIONR() != EXPECTED_W5500_VERSION_ID) {
        return false;
    }

    
    // W5500 has 16kB memory for RX/TX each internally.
    // It doesnt matter how the block is distributed across the 8 sockets.
    // We only use socket 0 for UDP broadcast, so we can allocate all the memory to that socket.
    uint8_t tx_sizes[] = {16, 0, 0, 0, 0, 0, 0, 0}; // kB
    uint8_t rx_sizes[] = {16, 0, 0, 0, 0, 0, 0, 0}; // kB

    if (wizchip_init(tx_sizes, rx_sizes) != 0) {
        return false;
    }

    wizchip_setnetinfo(&config.net_info);

    return true;
}

static bool init_udp(void) {
    // UDP Broadcast address
    // 255.255.255.255 is not forwarded by routers
    // local address | (~subnet address) is forwarded
    int8_t bc_sock = socket(config.udp_socket, SOCK_DGRAM, config.udp_port, 0);
    if (bc_sock != config.udp_socket) {
        return false;
    }

    return true;
}

static void eth_ready_periodic(void) {
    static constexpr size_t UDP_MAX_BUFFER_SIZE = 1500;
    static constexpr size_t UDP_MAX_WRITE_COUNT = UDP_MAX_BUFFER_SIZE / sizeof(timestamped_frame_t);
    static constexpr size_t UDP_MAX_WRITE_MINIS = UDP_MAX_WRITE_COUNT / SPMC_MINI_NUM_FRAMES;

    timestamped_frame_t* outgoing;
    size_t minis_available = SPMC_follower_peek_minis(&g_spmc, &outgoing);
    if (minis_available == 0) {
        return; // No data to send
    }

    if (minis_available > UDP_MAX_WRITE_MINIS) {
        minis_available = UDP_MAX_WRITE_MINIS; // Cap to max UDP size
    }

    // Write time :D
    uint16_t write_len = SPMC_MINI_NUM_FRAMES * minis_available * sizeof(timestamped_frame_t);
    int32_t ret = sendto(config.udp_socket, (uint8_t*)outgoing, write_len, config.udp_address, config.udp_port);
    if (ret < 0) {
        // todo: something is not initialized correctly
        return;
    } else if (ret < write_len) {
        // todo: a partial write occured
        return;
    } 

    SPMC_follower_advance_tail(&g_spmc, minis_available);
}

void ethernet_periodic(void) {
    current_state = next_state;
    next_state = current_state; // default to no state change

    switch (current_state) {
        case ETHERNET_STATE_HW_INIT:
            RTOS_delay_ms(200); // block for a bit between each init attempt
            
            if (init_w5500()) {
                next_state = ETHERNET_STATE_UDP_INIT;
            }
            break;
        case ETHERNET_STATE_UDP_INIT:
            RTOS_delay_ms(200); // block for a bit between each init attempt

            if (init_udp()) {
                next_state = ETHERNET_STATE_LINKING;
            }
            break;
        case ETHERNET_STATE_LINKING:
            RTOS_delay_ms(200); // block for a bit between each link check

            if (is_linked()) {
                next_state = ETHERNET_STATE_READY2TX;
            }
            break;
        case ETHERNET_STATE_READY2TX:
            eth_ready_periodic();
            RTOS_delay_ms(10); // 100 Hz tx rate

            if (!is_linked()) {
                next_state = ETHERNET_STATE_LINKING; // Link lost, try to relink
            } 
            break;
        case ETHERNET_STATE_RECOVERING:
            // todo timeout or error during TX
            break;
        case ETHERNET_STATE_FATAL:
            // todo after a couple of tries, give up and wait for a reset
            break;
    }
}


