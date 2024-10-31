#include "beacon_module.h"

#define PACKET_COPIES 5
#define PACKET_QUEUE_SIZE 5  // Define the size of the queue
#define PACKET_GEN_INTERVAL K_SECONDS(10)  // Frequency X in seconds

LOG_MODULE_REGISTER(beacon_module, LOG_LEVEL_INF);

static adv_mfg_data_type adv_mfg_data = {
    .company_code = {COMPANY_ID_CODE & 0xFF, COMPANY_ID_CODE >> 8},
    .number_press = {0, 0},
    .hello_message = "hello scan",
    .sensor_data = {0, 0}
};

struct packet_content {
    uint16_t sensor_value;
    uint8_t press_count;
};

// Queue buffer and ring buffer struct
RING_BUF_DECLARE(packet_queue, PACKET_QUEUE_SIZE * sizeof(struct packet_content));

static struct k_timer packet_gen_timer;

static struct bt_le_ext_adv *adv_set;
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, (unsigned char *)&adv_mfg_data, sizeof(adv_mfg_data)),
};

static void adv_sent_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info) {
    LOG_INF("Advertising stopped after %u events", info->num_sent);
}

static struct bt_le_ext_adv_cb adv_callbacks = {
    .sent = adv_sent_cb,
};

// Function to generate and enqueue new packet data
static void generate_packet_data(struct k_timer *dummy) {
    struct packet_content new_packet;

    // Populate packet content
    new_packet.sensor_value = adv_mfg_data.sensor_data[0] + 10;  // Implement this function to get sensor data
    new_packet.press_count = adv_mfg_data.number_press[0] + 1;

    // Add packet data to the queue
    if (ring_buf_put(&packet_queue, (uint8_t *)&new_packet, sizeof(new_packet)) != sizeof(new_packet)) {
        LOG_WRN("Packet queue is full. Dropping packet.");
    } else {
        LOG_INF("Generated new packet data with press count %d and sensor value %d",
                 new_packet.press_count, new_packet.sensor_value);
    }
}

// Initialize the timer for packet generation
int application_init(void) {
    k_timer_init(&packet_gen_timer, generate_packet_data, NULL);
    k_timer_start(&packet_gen_timer, PACKET_GEN_INTERVAL, PACKET_GEN_INTERVAL);
    return 0;
}

int advertising_module_init(void) {
    int err;

    LOG_INF("Initializing Advertising Module\n");

    struct bt_le_adv_param adv_param = {
        .options = BT_LE_ADV_OPT_NONE,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
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
    if (ring_buf_get(&packet_queue, (uint8_t *)&current_packet, len) != len) {
        LOG_WRN("No new packet data in the queue. Using previous data.");
    } else {
        // Update adv_mfg_data with new packet content from the queue
        adv_mfg_data.number_press[0] = current_packet.press_count;
        adv_mfg_data.sensor_data[0] = current_packet.sensor_value;
    }

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

    LOG_INF("Advertising started with updated packet content. Press count: %d, Sensor value: %d",
            adv_mfg_data.number_press[0], adv_mfg_data.sensor_data[0]);

    return 0;
}
