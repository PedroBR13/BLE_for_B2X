#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include "ble_module.h"
#include "gnss_module.h"

LOG_MODULE_REGISTER(ble_module, LOG_LEVEL_INF);  // Separate logging module for Bluetooth

#define SCAN_INTERVAL 0x0010
#define SCAN_WINDOW 0x0010

extern uint32_t runtime_ms;
extern uint32_t previous_runtime_ms;

// Initialize Bluetooth
int ble_init(void) {
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }
    LOG_INF("Bluetooth initialized");
    return 0;
}

// Start Bluetooth scanning
int ble_start_scanning(void) {
    struct bt_le_scan_param scan_param = {
        .type = BT_HCI_LE_SCAN_PASSIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = SCAN_INTERVAL,
        .window = SCAN_WINDOW,
    };

    int err = bt_le_scan_start(&scan_param, scan_cb);
    if (err) {
        LOG_ERR("Starting scanning failed (err %d)", err);
        return err;
    }

    LOG_INF("Bluetooth scanning started");
    return 0;
}

// Bluetooth scan callback
void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type, struct net_buf_simple *buf) {
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

    LOG_INF("Device found: %s (RSSI %d) at %02u:%02u:%02u.%03u - runtime: %llu",
            addr_str, rssi, current_hour, current_minute, current_second, current_ms, k_uptime_get());
}
