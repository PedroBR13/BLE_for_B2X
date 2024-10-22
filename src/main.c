#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <nrf_modem_gnss.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <dk_buttons_and_leds.h>

LOG_MODULE_REGISTER(GNSS_logging, LOG_LEVEL_INF); // Register the logging module

#define GNSS_TIMEOUT_SEC 120  // GNSS timeout duration
#define GNSS_RETRY_INTERVAL_SEC 60 // GNSS retry interval
#define SCAN_INTERVAL 0x0010 // Bluetooth scan interval
#define SCAN_WINDOW 0x0010   // Bluetooth scan window

// Struct to hold GNSS PVT (Position, Velocity, Time) data
static struct nrf_modem_gnss_pvt_data_frame pvt_data;

// Timers
static struct k_timer time_print_timer;

// Flag to check if the first fix is acquired
static bool first_fix = false;

// Structure to hold current RTC time
static struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t ms;
    uint16_t total_ms;
} rtc_time;

// Enum to define application states
typedef enum {
    STATE_GNSS_SEARCH,
    STATE_SCANNING,
    STATE_DONE
} app_state_t;

// Initialize current state to GNSS search state
static app_state_t current_state = STATE_GNSS_SEARCH;

static uint32_t runtime_ms = 0; // Variable to track the runtime in milliseconds
static uint32_t previous_runtime_ms = 0;

// Bluetooth scan callback
static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type, struct net_buf_simple *buf) {
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    runtime_ms = k_uptime_get() - previous_runtime_ms;


    // Calculate total runtime and RTC-based time
    uint32_t total_ms = rtc_time.ms + runtime_ms;
    uint8_t current_hour = rtc_time.hour;
    uint8_t current_minute = rtc_time.minute;
    uint8_t current_second = rtc_time.second + total_ms / 1000;
    uint16_t current_ms = total_ms % 1000;

    // Adjust time if needed (overflow handling)
    if (current_second >= 60) {
        current_minute += current_second / 60;
        current_second %= 60;
    }
    if (current_minute >= 60) {
        current_hour += current_minute / 60;
        current_minute %= 60;
    }
    if (current_hour >= 24) {
        current_hour %= 24;
    }

    // LOG_INF("Device found: %s (RSSI %d) at %02u:%02u:%02u.%03u - runtime: %llu",
    //         addr_str, rssi, current_hour, current_minute, current_second, current_ms, k_uptime_get());
}

// Function to update RTC from GNSS
static void update_rtc_from_gnss() {
    previous_runtime_ms = k_uptime_get();
    rtc_time.hour = pvt_data.datetime.hour;
    rtc_time.minute = pvt_data.datetime.minute;
    rtc_time.second = pvt_data.datetime.seconds;
    rtc_time.ms = pvt_data.datetime.ms;

    rtc_time.total_ms = (pvt_data.datetime.hour * 3600 * 1000) + (pvt_data.datetime.minute * 60 * 1000) + (pvt_data.datetime.seconds * 1000) + pvt_data.datetime.ms;

    LOG_INF("RTC updated from GNSS: %02u:%02u:%02u.%03u",
            rtc_time.hour, rtc_time.minute, rtc_time.second, rtc_time.ms);

    dk_set_led_on(DK_LED1); // Indicate fix acquisition with LED
}

// Event handler for GNSS events
static void gnss_event_handler(int event) {
    int err;

    switch (event) {
        case NRF_MODEM_GNSS_EVT_PVT:
            if (!first_fix) {
                LOG_INF("Searching...");
                int num_satellites = 0;
                for (int i = 0; i < 12 ; i++) {
                    if (pvt_data.sv[i].signal != 0) {
                        // LOG_INF("sv: %d, cn0: %d, signal: %d", pvt_data.sv[i].sv, pvt_data.sv[i].cn0, pvt_data.sv[i].signal);
                        num_satellites++;
			        }
		        }
		        LOG_INF("Number of current satellites: %d", num_satellites);
            }

            err = nrf_modem_gnss_read(&pvt_data, sizeof(pvt_data), NRF_MODEM_GNSS_DATA_PVT);
            if (err) {
                LOG_ERR("nrf_modem_gnss_read failed, err %d", err);
                return;
            }

            if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
                // update_rtc_from_gnss();
                // LOG_INF("Latitude: %.06f", pvt_data.latitude);
                // LOG_INF("Longitude: %.06f", pvt_data.longitude);
                // LOG_INF("Altitude: %.01f m", pvt_data.altitude);

                // Get the total uptime in milliseconds
                uint32_t uptime_ms = k_uptime_get() - previous_runtime_ms;

                // Calculate total RTC-based time by adding uptime to the RTC milliseconds
                uint32_t total_ms = rtc_time.ms + (uptime_ms % 1000);   // Add ms part
                uint32_t total_seconds = rtc_time.second + (uptime_ms / 1000);  // Add seconds from uptime

                // Adjust milliseconds overflow if total_ms exceeds 1000 ms
                if (total_ms >= 1000) {
                    total_seconds += total_ms / 1000;
                    total_ms %= 1000;
                }

                // Convert both times to milliseconds
                // int32_t pvt_ms = (pvt_data.datetime.hour * 3600 * 1000) + (pvt_data.datetime.minute * 60 * 1000) + (pvt_data.datetime.seconds * 1000) + pvt_data.datetime.ms;
                // int32_t rtc_ms = rtc_time.total_ms + uptime_ms;
                int32_t diff_ms = pvt_data.datetime.ms - total_ms;

                // Add seconds to minutes and handle overflow
                uint32_t total_minutes = rtc_time.minute + (total_seconds / 60);
                uint32_t current_seconds = total_seconds % 60;  // Update the current seconds

                // Add minutes to hours and handle overflow
                uint32_t total_hours = rtc_time.hour + (total_minutes / 60);
                uint32_t current_minutes = total_minutes % 60;  // Update the current minutes

                // Handle hour overflow (24-hour format)
                uint32_t current_hours = total_hours % 24;  // Keep the hours in a 24-hour format

                LOG_INF("Time (GNSS):  %02u:%02u:%02u.%03u    Time(RTC):  %02u:%02u:%02u.%03u   Error: %d ms \n",
                        pvt_data.datetime.hour,
                        pvt_data.datetime.minute,
                        pvt_data.datetime.seconds,
                        pvt_data.datetime.ms,
                        current_hours,
                        current_minutes,
                        current_seconds,
                        total_ms,
                        diff_ms);

                if (!first_fix) {
                    first_fix = true;
                    LOG_INF("First fix acquired.");
                    update_rtc_from_gnss();
                    LOG_INF("Latitude: %.06f", pvt_data.latitude);
                    LOG_INF("Longitude: %.06f", pvt_data.longitude);
                    LOG_INF("Altitude: %.01f m", pvt_data.altitude);
                }

                // current_state = STATE_SCANNING;
            }
            break;

        case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT:
            LOG_INF("GNSS timeout, entering sleep.");
            break;

        case NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP:
            LOG_INF("GNSS has woken up");
            break;

        case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX:
            LOG_INF("GNSS enter sleep after fix");
            break;

        default:
            break;
    }
}

// GNSS timeout handler
// static void gnss_timeout_handler(struct k_timer *timer_id) {
//     LOG_INF("GNSS search timed out, using hardcoded timestamp.");
    
//     rtc_time.hour = 11;
//     rtc_time.minute = 11;
//     rtc_time.second = 0;
//     rtc_time.ms = 0;

//     LOG_INF("Hardcoded RTC time set to: %02u:%02u:%02u.%03u",
//             rtc_time.hour, rtc_time.minute, rtc_time.second, rtc_time.ms);

//     k_work_submit(&gnss_stop_work);
//     current_state = STATE_SCANNING;
// }

// GNSS setup function
static int setup_gnss() {
    int err;

    err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS);
    if (err) {
        LOG_ERR("Failed to activate GNSS functional mode");
        return err;
    }

    err = nrf_modem_gnss_event_handler_set(gnss_event_handler);
    if (err) {
        LOG_ERR("Failed to set GNSS event handler");
        return err;
    }

    if (nrf_modem_gnss_fix_interval_set(1) != 0) {
        LOG_ERR("Failed to set GNSS fix interval");
        return -1;
    }

    if (nrf_modem_gnss_fix_retry_set(120) != 0) {
        LOG_ERR("Failed to set GNSS fix retry");
        return -1;
    }

    return nrf_modem_gnss_start();
}

// Function to print current RTC time
static void print_current_time() {
    // Get the total uptime in milliseconds
    uint32_t uptime_ms = k_uptime_get() - previous_runtime_ms;

    // Calculate total RTC-based time by adding uptime to the RTC milliseconds
    uint32_t total_ms = rtc_time.ms + (uptime_ms % 1000);   // Add ms part
    uint32_t total_seconds = rtc_time.second + (uptime_ms / 1000);  // Add seconds from uptime

    // Adjust milliseconds overflow if total_ms exceeds 1000 ms
    if (total_ms >= 1000) {
        total_seconds += total_ms / 1000;
        total_ms %= 1000;
    }

    // Add seconds to minutes and handle overflow
    uint32_t total_minutes = rtc_time.minute + (total_seconds / 60);
    uint32_t current_seconds = total_seconds % 60;  // Update the current seconds

    // Add minutes to hours and handle overflow
    uint32_t total_hours = rtc_time.hour + (total_minutes / 60);
    uint32_t current_minutes = total_minutes % 60;  // Update the current minutes

    // Handle hour overflow (24-hour format)
    uint32_t current_hours = total_hours % 24;  // Keep the hours in a 24-hour format

    // Log the calculated current time
    LOG_INF("Current time: %02u:%02u:%02u.%03u",
            current_hours, current_minutes, current_seconds, total_ms);
}

// Time print handler
static void time_print_handler(struct k_timer *timer_id) {
    print_current_time();
}

int main(void) {
    int err;

    dk_leds_init();

    err = nrf_modem_lib_init();
    if (err) {
        LOG_ERR("Failed to initialize modem, error: %d", err);
        return -1;
    }

    // Setup GNSS
    if (setup_gnss() != 0) {
        LOG_ERR("GNSS setup failed");
        return -1;
    }

    // Initialize and start the time print timer (every 10 seconds)
    // k_timer_init(&time_print_timer, time_print_handler, NULL);
    // k_timer_start(&time_print_timer, K_SECONDS(60), K_SECONDS(1));

    // Main loop
    while (current_state != STATE_DONE) {
        if (current_state == STATE_SCANNING) {
            err = bt_enable(NULL);
            if (err) {
                LOG_ERR("Bluetooth init failed (err %d)", err);
                return -1;
            }

            struct bt_le_scan_param scan_param = {
                .type = BT_HCI_LE_SCAN_PASSIVE,
                .options = BT_LE_SCAN_OPT_NONE,
                .interval = SCAN_INTERVAL,
                .window = SCAN_WINDOW,
            };

            err = bt_le_scan_start(&scan_param, scan_cb);
            if (err) {
                LOG_ERR("Starting scanning failed (err %d)", err);
                return -1;
            }

            current_state = STATE_DONE;
        }

        // Simulate 1 ms delay
        k_sleep(K_MSEC(1));
    }

    return 0;
}
