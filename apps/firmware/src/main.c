#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "ble_stack.h"
#include "openhaystack.h"
#include "app_timer.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

// Includes for the DFU
#include "nrf_power.h"
#include "nrf_dfu_ble_svci_bond_sharing.h"
#include "nrf_svci_async_function.h"
#include "nrf_svci_async_handler.h"
#include "ble_dfu.h"
#include "nrf_bootloader_info.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_sdh.h"

#ifndef uint8_t
    typedef __uint8_t uint8_t;
    typedef __uint16_t uint16_t;
    typedef __uint32_t uint32_t;
    typedef __uint64_t uint64_t;
#endif

/** 
 * advertising interval in milliseconds 
 */
#define ADVERTISING_INTERVAL 500

//static char public_key[28] = "OFFLINEFINDINGPUBLICKEYHERE!";
static char public_key[28] = {0x4e, 0xe3, 0xf3, 0xc5, 0xbf, 0x2f, 0xcb, 0x61, 
                              0x06, 0xc2, 0xb5, 0x1d, 0x80, 0xff, 0x60, 0xb8, 
                              0x77, 0x77, 0x2b, 0xe5, 0xc5, 0xe5, 0x4b, 0x03, 
                              0xaf, 0x76, 0xd5, 0xe1};


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{

    // Initialize timer module.
    uint32_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    // Create timers.

    /* YOUR_JOB: Create any timers to be used by the application.
                 Below is an example of how to create a timer.
                 For every new timer needed, increase the value of the macro APP_TIMER_MAX_TIMERS by
                 one.
       uint32_t err_code;
       err_code = app_timer_create(&m_app_timer_id, APP_TIMER_MODE_REPEATED, timer_timeout_handler);
       APP_ERROR_CHECK(err_code); */
}


/**@brief Function for the Power manager.
 */
static void log_init(void)
{
    uint32_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}
/**
 * main function
 */
int main(void) {
    // Init DFU
    // Initialize the async SVCI interface to bootloader before any interrupts are enabled.
    log_init();

    ret_code_t err_code;
    err_code = ble_dfu_buttonless_async_svci_init();
    APP_ERROR_CHECK(err_code);

    // Variable to hold the data to advertise
    uint8_t *ble_address;
    uint8_t *raw_data;
    uint8_t data_len;

    // Set key to be advertised
    data_len = setAdvertisementKey(public_key, &ble_address, &raw_data);

    // Timers
    timers_init();

    // Init BLE stack
    init_ble();

    // DFU
    peer_manager_init();
    gap_params_init();
    gatt_init();

    // Set bluetooth address
    setMacAddress(ble_address);

    advertising_init(ADVERTISING_INTERVAL);

    // Set advertisement data
    setAdvertisementData(raw_data, data_len);

    // Enable services
    services_init();
    conn_params_init();

    // Start advertising
    startAdvertisement();

    // Go to low power mode
    while (1) {
        power_manage();
    }
}
