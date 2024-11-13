#include <stdlib.h>
#include <zephyr/random/random.h>
#include "beacon_module.h"
#include "gnss_module.h"

#define PACKET_COPIES 3
#define PACKET_QUEUE_SIZE 20  // Define the size of the queue
#define PACKET_GEN_INTERVAL K_MSEC(200)  // Frequency X in seconds
#define ADV_INTERVAL 32 // 20ms

LOG_MODULE_REGISTER(beacon_module, LOG_LEVEL_INF);

static adv_mfg_data_type adv_mfg_data = {
    .company_code = {COMPANY_ID_CODE & 0xFF, COMPANY_ID_CODE >> 8},
    .number_press = {0, 0},
    .timestamp = "hello scan",
    .tx_delay = {0, 0}
};

struct packet_content {
    uint8_t tx_delay;
    uint16_t press_count;
    char timestamp[10]; // Fixed size to hold "MM:SS.mmm\0"
};

extern uint32_t runtime_ms;
extern uint32_t previous_runtime_ms;

// Queue buffer and ring buffer struct
RING_BUF_DECLARE(packet_queue, PACKET_QUEUE_SIZE * sizeof(struct packet_content));

static struct k_timer packet_gen_timer;
static struct k_work packet_work;

static struct bt_le_ext_adv *adv_set;
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, (unsigned char *)&adv_mfg_data, sizeof(adv_mfg_data)),
};

static bool advertising_complete_flag = false; // Flag for advertising completion
static bool update_availability_flag = false; // Flag for content availability
static int consecutive_full_counter = 0;

bool get_adv_progress(void) {
    return advertising_complete_flag;
}

bool check_update_availability(void) {
    return update_availability_flag;
}

static void adv_sent_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info) {
    // LOG_INF("Advertising stopped after %u events", info->num_sent);
    advertising_complete_flag = true;
};

static struct bt_le_ext_adv_cb adv_callbacks = {
    .sent = adv_sent_cb,
};

uint32_t random_delay(uint32_t min_ms, uint32_t max_ms) {
    uint32_t delay = min_ms + (sys_rand32_get() % (max_ms - min_ms + 1));
    k_sleep(K_MSEC(delay));
		return delay;
}

// Function to get current time as a formatted string without hour
static char* get_current_time_string(void) {
    runtime_ms = k_uptime_get_32() - previous_runtime_ms;
    uint32_t total_ms = rtc_time.ms + runtime_ms;
    uint8_t current_minute = rtc_time.minute;
    uint8_t current_second = rtc_time.second + total_ms / 1000;
    uint16_t current_ms = total_ms % 1000;

    // Adjust time for overflow handling
    if (current_second >= 60) {
        current_minute += current_second / 60;
        current_second %= 60;
    }
    if (current_minute >= 60) {
        current_minute %= 60;
    }

    // Allocate memory for the time string and format it
    static char time_string[10]; // Enough space for "MM:SS.mmm\0"
    snprintf(time_string, 10, "%02u:%02u.%03u", current_minute, current_second, current_ms);
    return time_string;
}

// Function to generate and enqueue new packet data
static void delayed_packet_enqueue(struct k_work *work) {
    struct packet_content new_packet;

    // Populate packet content
    new_packet.tx_delay = k_uptime_get_32();  // Implement this function to get sensor data
    new_packet.press_count = adv_mfg_data.number_press[0] + 1;

    // Simulate network layer delay
    random_delay(11, 20);

    // Add packet data to the queue
    if (ring_buf_put(&packet_queue, (uint8_t *)&new_packet, sizeof(new_packet)) != sizeof(new_packet)) {
        LOG_WRN("Packet queue is full. Dropping packet.");

        // Increment the consecutive full counter
        consecutive_full_counter++;

        // Check if the queue has been full MAX_CONSECUTIVE_FULL times in a row
        if (consecutive_full_counter >= 4) {
            LOG_WRN("Queue full %d times in a row. Resetting queue.", 4);

            // Reset the queue
            ring_buf_reset(&packet_queue);

            // Reset the counter
            consecutive_full_counter = 0;
        }

    } else {
        // LOG_INF("Generated new packet data with press count %d and sensor value %d",
        //          new_packet.press_count, new_packet.sensor_value);

        // Successfully added packet to the queue, reset the full counter
        consecutive_full_counter = 0;
    }

    // notify availability of data
    update_availability_flag = true;
}

static void generate_packet_data(struct k_timer *dummy) {
    k_work_submit(&packet_work);  // Schedule work to handle delay and queue
}

// Initialize the timer for packet generation
int application_init(void) {
    k_work_init(&packet_work, delayed_packet_enqueue); // Initialize work item
    k_timer_init(&packet_gen_timer, generate_packet_data, NULL);
    k_timer_start(&packet_gen_timer, PACKET_GEN_INTERVAL, PACKET_GEN_INTERVAL);
    return 0;
}

int advertising_module_init(void) {
    int err;

    // LOG_INF("Initializing Advertising Module\n");

    struct bt_le_adv_param adv_param = {
        .options = BT_LE_ADV_OPT_NONE,
        .interval_min = ADV_INTERVAL,
        .interval_max = ADV_INTERVAL,
        .peer = NULL,
    };

    err = bt_le_ext_adv_create(&adv_param, &adv_callbacks, &adv_set);
    if (err) {
        LOG_ERR("Failed to create extended advertising set (err %d)\n", err);
        return 0;
    }

    err = bt_le_ext_adv_set_data(adv_set, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Failed to set extended advertising data (err %d)\n", err);
        return 0;
    }

    return 0;
}

int advertising_start(void) {
    struct packet_content current_packet;
    uint32_t len = sizeof(current_packet);

    // Check if there is data in the queue
    if (!update_availability_flag || ring_buf_get(&packet_queue, (uint8_t *)&current_packet, len) != len) {
        LOG_WRN("No new packet data in the queue. Back to scanning mode.");
        update_availability_flag = false;
        advertising_complete_flag = true;
    } else {
        // Update adv_mfg_data with new packet content from the queue
        adv_mfg_data.number_press[0] = current_packet.press_count;
        adv_mfg_data.tx_delay[0] = k_uptime_get_32() - current_packet.tx_delay; // delay between generation and transmission

        // Get current time string and set it in hello_message - TIMESTAMP OF MGS TRANSMISSION
        char *time_string = get_current_time_string();
        if (time_string) {
            snprintf(adv_mfg_data.timestamp, sizeof(adv_mfg_data.timestamp), "%s", time_string);
        } else {
            LOG_ERR("Failed to allocate memory for timestamp string.");
        }
    }

    if (update_availability_flag) {
        struct bt_le_ext_adv_start_param start_param = {
            .timeout = 0,
            .num_events = PACKET_COPIES
        };

        // Start advertising with the updated data
        int err = bt_le_ext_adv_set_data(adv_set, ad, ARRAY_SIZE(ad), NULL, 0);
        if (err) {
            LOG_ERR("Failed to set advertising data (err %d)\n", err);
            return err;
        }

        err = bt_le_ext_adv_start(adv_set, &start_param);
        if (err) {
            LOG_ERR("Failed to start advertising (err %d)\n", err);
            return err;
        }

        // LOG_INF("Advertising started with updated packet content. Press count: %d, Sensor value: %d",
        //         adv_mfg_data.number_press[0], adv_mfg_data.sensor_data[0]);
    }

    return 0;
}

int advertising_stop(void) {
    // Stop the advertising
    if (update_availability_flag) {
        int err = bt_le_ext_adv_stop(adv_set);
        if (err) {
            LOG_ERR("Failed to stop advertising (err %d)\n", err);
            return err;
        }

        // LOG_INF("Advertising stopped successfully.");
        update_availability_flag = false;
    }
    
    advertising_complete_flag = true; // Set the flag to indicate advertising has stopped

    return 0;
}
