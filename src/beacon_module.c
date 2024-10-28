#include "beacon_module.h"

static adv_mfg_data_type adv_mfg_data = {
    .company_code = {COMPANY_ID_CODE & 0xFF, COMPANY_ID_CODE >> 8},
    .number_press = {0, 0},
    .hello_message = "hello scan",
    .sensor_data = {1234, 0}
};

LOG_MODULE_REGISTER(beacon_module, LOG_LEVEL_INF);

static struct bt_le_ext_adv *adv_set;
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, (unsigned char *)&adv_mfg_data, sizeof(adv_mfg_data)),
};

static void adv_sent_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info) {
    LOG_INF("Advertising event #%u sent", info->num_sent);

    // Stop advertising after a certain number of events
    if (info->num_sent >= 5) {
      k_sleep(K_MSEC(1)); //small delay to garantee that all packets are sent
      int err = bt_le_ext_adv_stop(adv_set);
      if (err) {
          LOG_ERR("Failed to stop advertising (err %d)", err);
      } else {
          LOG_INF("Advertising stopped after %u events", info->num_sent);
      }
    }
}

static struct bt_le_ext_adv_cb adv_callbacks = {
    .sent = adv_sent_cb,
};

// static size_t calculate_ble_data_size(const struct bt_data *data, size_t data_len) {
//     size_t total_size = 0;
//     for (size_t i = 0; i < data_len; i++) {
//         total_size += data[i].data_len + 2; // 1 byte for length field, 1 byte for type field
//     }
//     return total_size;
// }

int advertising_module_init(void) {
    int err;

    LOG_INF("Initializing Advertising Module\n");

    // err = bt_enable(NULL);
    // if (err) {
    //     LOG_ERR("Bluetooth init failed (err %d)\n", err);
    //     return;
    // }

    // LOG_INF("Bluetooth initialized\n");

    // size_t ad_total_size = calculate_ble_data_size(ad, ARRAY_SIZE(ad));
    // LOG_INF("Advertising Data Length: %zu bytes", ad_total_size);

    struct bt_le_adv_param adv_param = {
        .id = BT_ID_DEFAULT,
        .sid = 0,
        .secondary_max_skip = 0U,
        .options = BT_LE_ADV_OPT_EXT_ADV,
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
    struct bt_le_ext_adv_start_param start_param = {
        .timeout = 0,
        .num_events = 5    // Set the desired number of packets to send
    };

    // for (;;) {
    adv_mfg_data.number_press[0] += 1;
    LOG_INF("Current count: %d\n", adv_mfg_data.number_press[0]);
    bt_le_ext_adv_set_data(adv_set, ad, ARRAY_SIZE(ad), NULL, 0);
    int err = bt_le_ext_adv_start(adv_set, &start_param);
    if (err) {
        LOG_ERR("Failed to start extended advertising (err %d)\n", err);
        return 0;
    }

    LOG_INF("Extended advertising started for %d events\n", start_param.num_events);
    // k_sleep(K_MSEC(RUN_INTERVAL));
    // }

    return 0;
}
