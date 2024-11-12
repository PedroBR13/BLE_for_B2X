#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include "scan_module.h"
#include "gnss_module.h"

LOG_MODULE_REGISTER(scan_module, LOG_LEVEL_INF);  // Separate logging module for Bluetooth

#define SCAN_INTERVAL 0x0010
#define SCAN_WINDOW 0x0010

extern uint32_t runtime_ms;
extern uint32_t previous_runtime_ms;



typedef struct adv_mfg_data {
	uint8_t company_code[2]; /* Company Identifier Code. */
	uint16_t  number_press[2]; /* Number of times Button 1 is pressed */
	char hello_message[10];      /* "Hello" message */
	uint16_t  sensor_data[2];    /* Example sensor data */
} adv_mfg_data_type;

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
void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad) {
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    runtime_ms = k_uptime_get() - previous_runtime_ms;

    // Parse the advertisement data
	uint16_t *data = ad->data;
	int len = ad->len;
	char *name = NULL;
	const uint16_t *manufacturer_data = NULL;
	int manufacturer_data_len = 0;

	parse_advertisement_data(ad->data, ad->len, &name, &manufacturer_data, &manufacturer_data_len);

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

    // LOG_INF("Device found: %s (RSSI %d), type %u, AD data len %u, device name: %s\n",
    //     addr_str, rssi, type, ad->len, name ? name : "(unknown)");

    // LOG_INF("Device found: %s (RSSI %d) at %02u:%02u:%02u.%03u - runtime: %llu",
    //         addr_str, rssi, current_hour, current_minute, current_second, current_ms, k_uptime_get());

    if (name && strcmp(name, "B2B") == 0) {
			printk("Device found: %s (RSSI %d), type %u, AD data len %u, device name: %s\n",
							addr_str, rssi, type, ad->len,name);

			if (manufacturer_data && manufacturer_data_len >= sizeof(adv_mfg_data_type)) {
            adv_mfg_data_type *data = (adv_mfg_data_type *)manufacturer_data;

            uint16_t company_code = (data->company_code[1] << 8) | data->company_code[0];
            uint16_t number_press = (data->number_press[1] << 8) | data->number_press[0];
            char hello_message[11];
            memcpy(hello_message, data->hello_message, sizeof(data->hello_message));
            hello_message[sizeof(data->hello_message)] = '\0';
            uint16_t sensor_data = (data->sensor_data[1] << 8) | data->sensor_data[0];

            printk("Manufacturer ID: 0x%04x\n", company_code);
            printk("Number of presses: %u\n", number_press);
            printk("Hello message: %s\n", hello_message);
            printk("Sensor data: %u\n", sensor_data);
        } else {
            printk("Invalid manufacturer-specific data length\n");
        }
		
	}

	if (name) {
			free(name);
	}
}
