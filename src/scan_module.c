#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <stdlib.h>
#include "scan_module.h"
#include "gnss_module.h"
#include "ble_settings.h"
#include "sdcard_module.h"

LOG_MODULE_REGISTER(scan_module, LOG_LEVEL_INF);  // Separate logging module for Bluetooth

struct packet_data {
    uint16_t number_press;
    uint16_t tx_delay;
    uint32_t latitude;
    uint32_t longitude;
    uint8_t tx_hour;
    uint8_t tx_minute;
    uint8_t tx_second;
    uint16_t tx_ms;
    uint8_t rx_hour;
    uint8_t rx_minute;
    uint8_t rx_second;
    uint16_t rx_ms;
    int8_t rssi;
    uint32_t aoi;
};

static struct packet_data null_pkt = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static struct packet_data error_pkt = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

#if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
    #if ROLE
        K_MSGQ_DEFINE(packet_msgq, sizeof(struct packet_data), 10, 4);
    #endif
#endif

static bool packet_received = false;

static bool sd_record = false;

static struct rtc_time_s rtc_time = {0,0,0,0,0};

// Add a variable to store the timestamp of the last packet
static uint32_t last_packet_time = 0;

void reset_last_packet_time(void) {
    last_packet_time = k_uptime_get();
}

void switch_recording(bool state) {
    sd_record = state;
}

static void parse_advertisement_data(const uint8_t *data, int len, char **name, const uint8_t **manufacturer_data, int *manufacturer_data_len) {
    while (len > 0) {
        uint8_t field_len = data[0];
        if (field_len == 0) {
            break;
        }
        uint8_t field_type = data[1];
        const uint8_t *field_data = data + 2;
        int field_data_len = field_len - 1;

        if (field_type == BT_DATA_NAME_COMPLETE) {  // Complete Local Name
            *name = (char *)malloc(field_data_len + 1);
            memcpy(*name, field_data, field_data_len);
            (*name)[field_data_len] = '\0';  // Null-terminate the string
        }

        if (field_type == BT_DATA_MANUFACTURER_DATA) {  // Manufacturer Specific Data
            *manufacturer_data = field_data;
            *manufacturer_data_len = field_data_len;
        }

        // Move to the next field
        data += field_len + 1;
        len -= field_len + 1;
    }
}

bool is_packet_received(void) {
    return packet_received;
}

void reset_packet_received(void) {
    packet_received = false;
}

// Function to get current time as a single integer
static uint32_t get_current_time_packed(void) {
    // Calculate runtime milliseconds since last update
    uint32_t runtime_ms = k_uptime_get() - rtc_time.last_runtime;
    // LOG_INF("uptime: %llu - last_runtime: %u = %u", k_uptime_get(), rtc_time.last_runtime, runtime_ms);
    uint32_t total_ms = rtc_time.ms + runtime_ms;

    // Calculate the components of the current time
    uint16_t current_hour = rtc_time.hour;
    uint16_t current_minute = rtc_time.minute;
    uint16_t current_second = rtc_time.second + total_ms / 1000;
    uint16_t current_ms = total_ms % 1000;

    // Adjust time for overflow handling
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

    // Pack the time into a single 32-bit integer
    uint32_t packed_time = (current_hour << 27) | (current_minute << 21) |
                           (current_second << 15) | (current_ms);

    return packed_time;
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

    // LOG_INF("Bluetooth scanning started");
    return 0;
}

// Bluetooth scan callback
void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad) {
    if (sd_record == true) {
        char addr_str[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
        struct packet_data pkt;

        // Get the current time as a string
        uint32_t current_time = get_current_time_packed();
        uint32_t uptime = k_uptime_get();

        // Parse the advertisement data
        uint16_t *data = ad->data;
        int len = ad->len;
        char *name = NULL;
        const uint16_t *manufacturer_data = NULL;
        int manufacturer_data_len = 0;

        parse_advertisement_data(ad->data, ad->len, &name, &manufacturer_data, &manufacturer_data_len);

        // LOG_INF("Device found: %s (RSSI %d), type %u, AD data len %u, device name: %s\n",
        //     addr_str, rssi, type, ad->len, name ? name : "(unknown)");

        #ifdef CONFIG_BOARD_NRF9160DK_NRF52840
        if (name && strcmp(name, "B2B1") == 0) {
        #else
            #if ROLE
                if (name && strcmp(name, "B2B2") == 0) {
            #else
                if (name && strcmp(name, "B2B1") == 0) {
            #endif
        #endif
            // LOG_INF("Device found: %s (RSSI %d), type %u, AD data len %u, device name: %s\n",
            //                 addr_str, rssi, type, ad->len,name);

            if (manufacturer_data && manufacturer_data_len >= sizeof(adv_mfg_data_type)) {
            adv_mfg_data_type *data = (adv_mfg_data_type *)manufacturer_data;

            // Calculate the duration since the last packet
            uint32_t duration_since_last_packet = 0;
            
            if (last_packet_time != 0) {
                if (uptime >= last_packet_time) {
                    duration_since_last_packet = uptime - last_packet_time;
                } else {
                    // Handle wrap-around case for a 32-bit timestamp
                    duration_since_last_packet = (UINT32_MAX - last_packet_time) + uptime + 1;
                }
            }
            last_packet_time = uptime; // Update the last packet time

            uint16_t number_press = data->number_press[0];
            uint16_t tx_delay = data->tx_delay[0];
            uint32_t longitude = data->longitude[0];
            uint32_t tx_timestamp = data->timestamp[0];

            // Raw byte values for latitude
            uint32_t raw_latitude[4];
            memcpy(raw_latitude, &data->latitude[0], sizeof(raw_latitude));

            // Print raw byte values of latitude
            // LOG_INF("Raw Latitude Bytes: 0x%02X 0x%02X 0x%02X 0x%02X", raw_latitude[0], raw_latitude[1], raw_latitude[2], raw_latitude[3]);

            // Convert raw bytes to uint32_t
            uint32_t latitude = data->latitude[0];

            uint8_t current_hour = (current_time >> 27) & 0x1F;
            uint8_t current_minute = (current_time >> 21) & 0x3F;
            uint8_t current_second = (current_time >> 15) & 0x3F;
            uint16_t current_ms = current_time & 0x3FF;

            uint8_t timestamp_hour = (tx_timestamp >> 27) & 0x1F;
            uint8_t timestamp_minute = (tx_timestamp >> 21) & 0x3F;
            uint8_t timestamp_second = (tx_timestamp >> 15) & 0x3F;
            uint16_t timestamp_ms = tx_timestamp & 0x3FF;

            // LOG_INF("%02u:%02u:%02u.%03u %02u:%02u:%02u.%03u %s %u %u %u %u", 
            //         current_hour, current_minute, current_second, current_ms,
            //         timestamp_hour, timestamp_minute, timestamp_second, timestamp_ms, 
            //         addr_str, number_press, tx_delay, latitude, longitude);

            pkt.number_press = number_press;
            pkt.tx_delay = tx_delay;
            pkt.latitude = latitude;
            pkt.longitude = longitude;
            pkt.tx_hour = timestamp_hour;
            pkt.tx_minute = timestamp_minute;
            pkt.tx_second = timestamp_second;
            pkt.tx_ms = timestamp_ms;
            pkt.rx_hour = current_hour;
            pkt.rx_minute = current_minute;
            pkt.rx_second = current_second;
            pkt.rx_ms = current_ms;
            pkt.rssi = rssi;
            pkt.aoi = duration_since_last_packet;
            
            #if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
                #if ROLE
                    if (k_msgq_put(&packet_msgq, &pkt, K_NO_WAIT) != 0) {
                        LOG_ERR("Message queue full. Dropping packet.");
                    }
                #endif

                // Mark that a packet was received
                packet_received = true;
            #endif

            } else {
                // LOG_INF("Invalid manufacturer-specific data length\n");
                LOG_INF("Invalid manufacturer data length: %d (expected %d)", manufacturer_data_len, sizeof(adv_mfg_data_type));
            }   
        }

        if (name) {
            free(name);
            name = NULL;
        }
    }
}

#if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
    #if ROLE
        void append_null(void) {
            struct packet_data pkt;
            pkt = null_pkt;
            if (k_msgq_put(&packet_msgq, &pkt, K_NO_WAIT) != 0) {
                        LOG_ERR("Message queue full. Dropping packet.");
                    }

        }

        void append_error(void) {
            struct packet_data pkt;
            pkt = error_pkt;
            if (k_msgq_put(&packet_msgq, &pkt, K_NO_WAIT) != 0) {
                        LOG_ERR("Message queue full. Dropping packet.");
                    }

        }

        void sdcard_thread(void) {
            struct packet_data pkt;
            while (true) {
                if (k_msgq_get(&packet_msgq, &pkt, K_FOREVER) == 0) {
                    // Perform SD card write operation
                    append_csv(pkt.number_press, pkt.tx_delay, pkt.latitude, pkt.longitude,
                                pkt.tx_hour, pkt.tx_minute, pkt.tx_second, pkt.tx_ms, pkt.rssi,
                                pkt.rx_hour, pkt.rx_minute, pkt.rx_second, pkt.rx_ms,pkt.aoi);
                }
            }
        }

        K_THREAD_DEFINE(sdcard_tid, 2048, sdcard_thread, NULL, NULL, NULL, 5, 0, 0);

        void reset_packet_queue(void)
        {
            k_msgq_purge(&packet_msgq); // Clears all pending messages in the queue
            LOG_INF("Packet message queue has been reset.");
        }
    #endif
#endif