#ifndef BEACON_MODULE_H
#define BEACON_MODULE_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>

#define COMPANY_ID_CODE 0x0059
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
#define RUN_INTERVAL 30000

typedef struct adv_mfg_data {
    uint8_t company_code[2];
    uint16_t number_press[2];
    char hello_message[10];
    uint16_t sensor_data[2];
} adv_mfg_data_type;

int advertising_module_init(void);
int advertising_start(void);

#endif // BEACON_MODULE_H
