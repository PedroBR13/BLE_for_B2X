#ifndef SDCARD_MODULE_H
#define SDCARD_MODULE_H

#define CSV_TEST_NAME "sw50n3" //sw50n3, msg400n5, sw80n3, msg400
#define TEST_PERIOD 5 //seconds

int sdcard_init(void);
int disk_unmount(void);
int create_csv(void);
int append_csv(uint16_t number_press, uint16_t tx_delay, uint32_t latitude, uint32_t longitude, 
              uint8_t tx_hour, uint8_t tx_minute, uint8_t tx_second, uint16_t tx_ms,int8_t rssi,
              uint8_t rx_hour, uint8_t rx_minute, uint8_t rx_second, uint16_t rx_ms);


#endif // SDCARD_MODULE_H
