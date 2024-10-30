#include "beacon_module.h"

#define PACKET_COPIES 5

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
    LOG_INF("Advertising stopped after %u events", info->num_sent);

    // Stop advertising after a certain number of events
    // if (info->num_sent >= PACKET_COPIES) {
    //   int err = bt_le_ext_adv_stop(adv_set);
    //   if (err) {
    //       LOG_ERR("Failed to stop advertising (err %d)", err);
    //   } else {
    //       LOG_INF("Advertising stopped after %u events", info->num_sent);
    //   }
    // }
}

static struct bt_le_ext_adv_cb adv_callbacks = {
    .sent = adv_sent_cb,
};

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
    struct bt_le_ext_adv_start_param start_param = {
        .timeout = 0,
        .num_events = PACKET_COPIES    // Set the desired number of packets to send
    };

    adv_mfg_data.number_press[0] += 1;
    LOG_INF("Current count: %d\n", adv_mfg_data.number_press[0]);
    bt_le_ext_adv_set_data(adv_set, ad, ARRAY_SIZE(ad), NULL, 0);
    int err = bt_le_ext_adv_start(adv_set, &start_param);
    if (err) {
        LOG_ERR("Failed to start extended advertising (err %d)\n", err);
        return 0;
    }

    LOG_INF("Extended advertising started for %d events\n", start_param.num_events);

    return 0;
}
