#ifndef BEACON_MODULE_H
#define BEACON_MODULE_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/sys/ring_buffer.h>

#define COMPANY_ID_CODE 0x0059
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
#define RUN_INTERVAL 30000

// timestamp structure
// typedef struct adv_mfg_data {
//     uint16_t company_code[2];
//     uint16_t number_press[2];
//     char timestamp[10];
//     uint8_t tx_delay[2];
// } adv_mfg_data_type;

typedef struct adv_mfg_data {
    uint16_t company_code[2];    // 2 bytes (1 * uint16_t)
    uint16_t number_press[1];    // 2 bytes (1 * uint16_t)
    // char timestamp[10];          // 10 bytes (char array, assuming ASCII)
    uint8_t tx_delay[1];         // 1 bytes (1 * uint8_t)
    uint32_t latitude[1];            // 4 bytes (scaled integer for latitude)
    uint32_t longitude[1];           // 4 bytes (scaled integer for longitude)
} adv_mfg_data_type;

int advertising_module_init(void);
int advertising_start(void);
int application_init(void);
bool get_adv_progress(void);
bool check_update_availability(void);
int advertising_stop(void);

#endif // BEACON_MODULE_H
