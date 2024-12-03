#include <zephyr/kernel.h>
#include <nrf_modem_gnss.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
// #include <dk_buttons_and_leds.h>
#include <zephyr/logging/log.h>
#include "gnss_module.h" // Include the header file for GNSS functionality

LOG_MODULE_REGISTER(GNSS_module, LOG_LEVEL_INF); // Use the same logging module

static struct nrf_modem_gnss_pvt_data_frame pvt_data;  // PVT data
static bool first_fix = false;  // Flag for first fix
static gnss_fix_callback_t fix_cb;  // Callback for first fix

// Structure to hold current RTC time
struct rtc_time_s rtc_time;
struct gnss_s last_gnss_data = {52243187,6856186};
static uint32_t previous_runtime_ms = 0;

// GNSS event handler
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
                // LOG_INF("Time (GNSS):  %02u:%02u:%02u.%03u",
                //         pvt_data.datetime.hour, pvt_data.datetime.minute, 
                //         pvt_data.datetime.seconds, pvt_data.datetime.ms);

                if (!first_fix) {
                    first_fix = true;
                    update_rtc_from_gnss();
                    LOG_INF("First fix acquired.");
                    LOG_INF("Latitude:       %f", pvt_data.latitude);
                    LOG_INF("Longitude:      %f", pvt_data.longitude);
                    LOG_INF("Altitude:       %.01f m", pvt_data.altitude);

                    if (fix_cb) {
                        fix_cb();  // Call the callback function to notify the main app
                    }
                } else {
                    update_rtc_from_gnss();
                }
            }
            break;

        default:
            break;
    }
}

// GNSS timeout handler
static void gnss_timeout_handler(struct k_timer *timer_id) {
    LOG_INF("GNSS search timed out, using hardcoded timestamp.");
    
    rtc_time.hour = 11;
    rtc_time.minute = 11;
    rtc_time.second = 0;
    rtc_time.ms = 0;

    LOG_INF("Hardcoded RTC time set to: %02u:%02u:%02u.%03u",
            rtc_time.hour, rtc_time.minute, rtc_time.second, rtc_time.ms);

    if (fix_cb) {
        fix_cb();  // Call the callback function to notify the main app
    };
}

// Function to update RTC from GNSS
void update_rtc_from_gnss(void) {
    previous_runtime_ms = k_uptime_get();
    rtc_time.hour = pvt_data.datetime.hour;
    rtc_time.minute = pvt_data.datetime.minute;
    rtc_time.second = pvt_data.datetime.seconds;
    rtc_time.ms = pvt_data.datetime.ms;

    // Store the latest latitude and longitude in the gnss_data structure
    last_gnss_data.latitude = (uint32_t)(pvt_data.latitude * 1000000);
    last_gnss_data.longitude = (uint32_t)(pvt_data.longitude * 1000000);

    LOG_INF("RTC updated from GNSS: %02u:%02u:%02u.%03u - Coordinates: %u, %u", 
            rtc_time.hour, rtc_time.minute, rtc_time.second, rtc_time.ms, last_gnss_data.latitude, last_gnss_data.longitude);

    // dk_set_led_on(DK_LED1); // Indicate fix acquisition with LED
}

// GNSS setup function that accepts a callback for the first fix
int setup_gnss(gnss_fix_callback_t fix_callback) {
    int err;

    // Store the callback function
    fix_cb = fix_callback;

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

    err = nrf_modem_gnss_fix_interval_set(10);
    if (err) {
        LOG_ERR("Failed to set GNSS fix interval");
        return err;
    }

    err = nrf_modem_gnss_fix_retry_set(120);
    if (err) {
        LOG_ERR("Failed to set GNSS fix retry");
        return err;
    }

    return nrf_modem_gnss_start();
}
