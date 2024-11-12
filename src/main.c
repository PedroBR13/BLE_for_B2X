#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
// #include <dk_buttons_and_leds.h>
#include <zephyr/drivers/gpio.h>
#include "gnss_module.h" // Include the GNSS module
#include "scan_module.h"   // Include the BLE module
#include "beacon_module.h"  // Include the BLE beacon module

LOG_MODULE_REGISTER(main_logging, LOG_LEVEL_INF); // Register the logging module

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

// Timers
static struct k_timer timeout_timer;

// Define the runtime variables globally so they can be accessed from other files
uint32_t runtime_ms = 0;
uint32_t previous_runtime_ms = 0;

typedef enum {
    STATE_GNSS_SEARCH,
    STATE_SCANNING,
    STATE_ADVERTISING,
    STATE_DONE
} app_state_t;

static app_state_t current_state = STATE_SCANNING;  // Initialize to GNSS search state

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

    current_state = STATE_DONE;
}

int main(void) {
    int err;
    LOG_INF("Starting B2B device...");

    // dk_leds_init();

    int ret;
    if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

    // err = nrf_modem_lib_init();
    // if (err) {
    //     LOG_ERR("Failed to initialize modem, error: %d", err);
    //     return -1;
    // }

    // // Setup GNSS with the first-fix callback
    // if (setup_gnss(on_first_fix_acquired) != 0) {
    //     LOG_ERR("GNSS setup failed");
    //     return -1;
    // }

    // k_timer_init(&timeout_timer, gnss_timeout_handler, NULL);
    // k_timer_start(&timeout_timer, K_SECONDS(20), K_FOREVER);

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }
    LOG_INF("Bluetooth initialized");

    err = application_init();
    if (err) {
        LOG_ERR("Application init failed");
        return err;
    }

    // Initialize and start Bluetooth advertising
    err = advertising_module_init();
    if (err) {
        LOG_ERR("Advertising module init failed");
        return err;
    }

    // Main loop
    while (true) {
        switch (current_state) {
            case STATE_SCANNING:
                // Initialize and start Bluetooth scanning
                err = ble_start_scanning();
                if (err) {
                    LOG_ERR("BLE scanning start failed");
                    return err;
                }

                // Simulate scanning duration
                k_sleep(K_MSEC(1000));

                // Stop scanning before transitioning to advertising
                err = bt_le_scan_stop();
                if (err) {
                    LOG_ERR("Stopping scanning failed (err %d)\n", err);
                    return err;
                }

                // // Move to ADVERTISING state
                current_state = STATE_ADVERTISING;
                break;

            case STATE_ADVERTISING:
                err = advertising_start();
                if (err) {
                    LOG_ERR("Advertising start failed");
                    return err;
                }

                // Wait here until advertising completes
                while (!get_adv_progress()) {
                    k_sleep(K_MSEC(10)); // Small delay to avoid CPU overuse
                }

                advertising_stop();

                current_state = STATE_SCANNING;
                break;

            case STATE_DONE:
                LOG_INF("Process done. Going to scanning state");
                current_state = STATE_SCANNING;
                break;

            default:
                LOG_ERR("Unknown state");
                return -1;
        }

        // Simulate 1 ms delay to prevent CPU overload
        k_sleep(K_MSEC(1));
    }

    return 0;
}
