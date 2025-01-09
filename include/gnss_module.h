#ifndef GNSS_MODULE_H
#define GNSS_MODULE_H

#include <stdint.h>

// RTC structure
struct rtc_time_s {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t ms;
    uint32_t last_runtime;
};

// RTC structure
struct gnss_s {
    uint32_t latitude;  
    uint32_t longitude;
};

// Extern declaration so it can be accessed from other files
// extern struct rtc_time_s rtc_time = {0,0,0,0,0};
// extern struct gnss_s last_gnss_data  = {52243187,6856186};


// GNSS-related function declarations
// typedef void (*gnss_fix_callback_t)(void);  // Define callback type

// int setup_gnss(gnss_fix_callback_t fix_cb); // Function that accepts a callback
// void update_rtc_from_gnss(void);

#endif // GNSS_MODULE_H
