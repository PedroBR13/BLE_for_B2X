#ifndef SCAN_MODULE_H
#define SCAN_MODULE_H

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

// Function to initialize Bluetooth
int ble_init(void);

// Function to start Bluetooth scanning
int ble_start_scanning(void);

// Callback for Bluetooth scan results
void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type, struct net_buf_simple *buf);

#endif // SCAN_MODULE_H
