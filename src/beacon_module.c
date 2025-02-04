#include <stdlib.h>
#include <zephyr/random/random.h>
#include "ble_settings.h"
#include "beacon_module.h"
#include "gnss_module.h"
#include "sdcard_module.h"

LOG_MODULE_REGISTER(beacon_module, LOG_LEVEL_INF);

static adv_mfg_data_type adv_mfg_data;

static struct rtc_time_s rtc_time = {0,0,0,0,0};
static struct gnss_s last_gnss_data  = {52243187,6856186};

struct packet_content {
    uint8_t tx_delay;
    uint16_t press_count;
    uint32_t timestamp;
};

static struct packet_content current_packet; // Single-packet buffer
static bool packet_pending = false; // Indicates if a packet is waiting to be served
static uint32_t override_interval = 0;  // 0 means no override

static struct k_timer packet_gen_timer;
static struct k_work packet_work;

#ifdef CONFIG_BOARD_NRF9160DK_NRF52840
    #define DEVICE_NAME "B2B2"
#else
    #if ROLE
        #define DEVICE_NAME "B2B1"
    #else
        #define DEVICE_NAME "B2B2"
    #endif
#endif

#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static struct bt_le_ext_adv *adv_set;
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, (unsigned char *)&adv_mfg_data, sizeof(adv_mfg_data)),
};

static bool advertising_complete_flag = false; // Flag for advertising completion
static bool update_availability_flag = false; // Flag for content availability
bool get_adv_progress(void) {
    return advertising_complete_flag;
}

bool check_update_availability(void) {
    return update_availability_flag;
}

static void adv_sent_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info) {
    // LOG_INF("Advertising stopped after %u events", info->num_sent);
    advertising_complete_flag = true;
    update_availability_flag = false; // Reset availability after advertising
    packet_pending = false; // Mark packet as processed
};

static struct bt_le_ext_adv_cb adv_callbacks = {
    .sent = adv_sent_cb,
};

uint32_t random_delay(uint32_t min_ms, uint32_t max_ms) {
    uint32_t delay = min_ms + (sys_rand32_get() % (max_ms - min_ms + 1));
    k_sleep(K_MSEC(delay));
		return delay;
}

// Function to get current time as a single integer
static uint32_t get_current_time_packed(void) {
    // Calculate runtime milliseconds since last update
    uint32_t runtime_ms = k_uptime_get() - rtc_time.last_runtime;
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

// Function to generate and enqueue new packet data
static void delayed_packet_enqueue(struct k_work *work) {
    if (packet_pending) {
        // LOG_WRN("Packet dropped: A previous packet is still being processed.");
        return;
    }

    // Populate new packet content
    current_packet.tx_delay = k_uptime_get_32();
    current_packet.press_count = adv_mfg_data.number_press[0] + 1;
    // LOG_INF("Packet generated");

    // Simulate network layer delay
    random_delay(11, 20);

    // Mark the packet as ready to be advertised
    packet_pending = true;
    update_availability_flag = true;
}

static void generate_packet_data(struct k_timer *dummy) {
    k_work_submit(&packet_work);  // Schedule work to handle delay and queue

    // Check if an override interval is set
    if (override_interval) {
        k_timer_start(&packet_gen_timer, K_MSEC(override_interval), PACKET_GEN_INTERVAL);
        override_interval = 0;  // Reset after one-time use
    }
}

// Function to modify the interval for only one iteration
void trigger_time_shift(void) {
    override_interval = TEST_SHIFT + INTERVAL;
}

// Initialize the timer for packet generation
int application_init(void) {
    k_work_init(&packet_work, delayed_packet_enqueue); // Initialize work item
    k_timer_init(&packet_gen_timer, generate_packet_data, NULL);
    k_timer_start(&packet_gen_timer, PACKET_GEN_INTERVAL, PACKET_GEN_INTERVAL);
    return 0;
}

int application_stop(void) {
    // Stop the packet generation timer
    k_timer_stop(&packet_gen_timer);

    // Cancel any pending work in the work queue
    int ret = k_work_cancel(&packet_work);
    if (ret == -EINPROGRESS) {
        // If the work is currently being executed, wait for it to finish
        k_work_flush(&packet_work, NULL);
    }

    LOG_INF("Application stopped: Timer and work queue reset");
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

    return 0;
}

int advertising_start(void) {
    if (!packet_pending) {
        LOG_WRN("No packet available to advertise. Back to scanning mode.");
        advertising_complete_flag = true;
        return 0;
    }
    advertising_complete_flag = false;

    // Update adv_mfg_data with the current packet content
    adv_mfg_data.number_press[0] = current_packet.press_count;
    adv_mfg_data.tx_delay[0] = k_uptime_get_32() - current_packet.tx_delay;
    adv_mfg_data.latitude[0] = last_gnss_data.latitude;
    adv_mfg_data.longitude[0] = last_gnss_data.longitude;
    adv_mfg_data.timestamp[0] = get_current_time_packed();

    // Debug log to show packet content and its size
    // LOG_INF("Advertising packet content:");
    // LOG_INF("  Press count: %d (Size: %zu bytes)", adv_mfg_data.number_press[0], sizeof(adv_mfg_data.number_press));
    // LOG_INF("  TX delay: %d (Size: %zu bytes)", adv_mfg_data.tx_delay[0], sizeof(adv_mfg_data.tx_delay));
    // LOG_INF("  Latitude: %d (Size: %zu bytes)", adv_mfg_data.latitude[0], sizeof(adv_mfg_data.latitude));
    // LOG_INF("  Longitude: %d (Size: %zu bytes)", adv_mfg_data.longitude[0], sizeof(adv_mfg_data.longitude));
    // LOG_INF("  Timestamp: %d (Size: %zu bytes)", adv_mfg_data.timestamp[0], sizeof(adv_mfg_data.timestamp));
    // LOG_INF("Total packet size: %zu bytes", sizeof(adv_mfg_data));

    int err = bt_le_ext_adv_set_data(adv_set, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Failed to set advertising data (err %d)\n", err);
        return err;
    }
    
    struct bt_le_ext_adv_start_param start_param = {
        .timeout = 0,
        .num_events = PACKET_COPIES
    };

    struct bt_le_adv_param adv_param = {
        .options = BT_LE_ADV_OPT_NONE,
        .interval_min = ADV_INTERVAL,
        .interval_max = ADV_INTERVAL,
        .peer = NULL,
    };

    err = bt_le_ext_adv_start(adv_set, &start_param);
    if (err) {
        LOG_ERR("Failed to start advertising (err %d)", err);
        return err;
    }

    return 0;
}

int advertising_stop(void) {
    // Stop the advertising
    // LOG_INF("Advertising stopped successfully.");
    advertising_complete_flag = true;
    update_availability_flag = false; // Reset availability after advertising
    packet_pending = false; // Mark packet as processed

    return 0;
}
