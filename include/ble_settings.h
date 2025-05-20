#ifndef BLE_SETTINGS_H
#define BLE_SETTINGS_H

// ADVERTISING PARAMETERS

#define PACKET_COPIES 5
#define INTERVAL 200
#define PACKET_GEN_INTERVAL K_MSEC(INTERVAL)  // Frequency X in seconds
#define ADV_INTERVAL 32 // 20ms
#define ROLE 1 // 1=master , 0=slave

// SCAN PARAMERTERS
#define SCAN_INTERVAL 80 // 80 = 50ms, 128 = 80 ms - scan setting on the scan module
#define SCAN_WINDOW 80 // 80 = 50ms, 128 = 80 ms - scan setting on the scan module
#define SCAN_WINDOW_MAIN 50  //ms - scan setting in the main file


// PACKET STRUCTURE
typedef struct adv_mfg_data {
    // uint16_t company_code[2];    // 2 bytes (1 * uint16_t)
    uint16_t number_press[1];    // 2 bytes (1 * uint16_t)
    uint32_t timestamp[1];          // 4 bytes
    uint8_t tx_delay[1];         // 1 bytes (1 * uint8_t)
    uint32_t latitude[1];            // 4 bytes (scaled integer for latitude)
    uint32_t longitude[1];           // 4 bytes (scaled integer for longitude)
} adv_mfg_data_type;

//PACKET CONTENT
#define COMPANY_ID_CODE 0x0059
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#endif  // BLE_SETTINGS_H