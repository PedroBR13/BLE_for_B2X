#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <dk_buttons_and_leds.h>
#include "gnss_module.h" // Include the GNSS module
#include "ble_module.h"   // Include the BLE module

LOG_MODULE_REGISTER(main_logging, LOG_LEVEL_INF); // Register the logging module

// Timers
static struct k_timer timeout_timer;

// Define the runtime variables globally so they can be accessed from other files
uint32_t runtime_ms = 0;
uint32_t previous_runtime_ms = 0;

typedef enum {
    STATE_GNSS_SEARCH,
    STATE_SCANNING,
    STATE_DONE
} app_state_t;

static app_state_t current_state = STATE_GNSS_SEARCH;  // Initialize to GNSS search state

// Callback function for first GNSS fix
void on_first_fix_acquired(void) {
    LOG_INF("Switching from GNSS search to Bluetooth scanning");
    current_state = STATE_SCANNING;
}

// GNSS timeout handler
static void gnss_timeout_handler(struct k_timer *timer_id) {
    LOG_INF("GNSS search timed out, using hardcoded timestamp.");
    
    rtc_time.hour = 0;
    rtc_time.minute = 0;
    rtc_time.second = 0;
    rtc_time.ms = 0;

    LOG_INF("Hardcoded RTC time set to: %02u:%02u:%02u.%03u",
            rtc_time.hour, rtc_time.minute, rtc_time.second, rtc_time.ms);

    current_state = STATE_SCANNING;
}

int main(void) {
    int err;

    dk_leds_init();

    err = nrf_modem_lib_init();
    if (err) {
        LOG_ERR("Failed to initialize modem, error: %d", err);
        return -1;
    }

    // Setup GNSS with the first-fix callback
    if (setup_gnss(on_first_fix_acquired) != 0) {
        LOG_ERR("GNSS setup failed");
        return -1;
    }

    k_timer_init(&timeout_timer, gnss_timeout_handler, NULL);
    k_timer_start(&timeout_timer, K_SECONDS(20), K_FOREVER);

    // Main loop
    while (current_state != STATE_DONE) {
        if (current_state == STATE_SCANNING) {
            // Initialize Bluetooth
            err = ble_init();
            if (err) {
                return -1;
            }

            // Start Bluetooth scanning
            err = ble_start_scanning();
            if (err) {
                return -1;
            }

            current_state = STATE_DONE;  // Indicate that we're done
        }

        // Simulate 1 ms delay
        k_sleep(K_MSEC(1));
    }

    return 0;
}
