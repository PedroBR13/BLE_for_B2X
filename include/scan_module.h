#ifndef SCAN_MODULE_H
#define SCAN_MODULE_H

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

// Function to start Bluetooth scanning
int ble_start_scanning(void);


bool is_packet_received(void);
void reset_packet_received(void);
void reset_last_packet_time(void);
void switch_recording(bool state);
void reset_packet_queue(void);

#if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
void append_null(void);
void append_error(void);
#endif

// Callback for Bluetooth scan results
void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type, struct net_buf_simple *buf);

#endif // SCAN_MODULE_H
