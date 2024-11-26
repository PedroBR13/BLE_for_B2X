#ifndef BEACON_MODULE_H
#define BEACON_MODULE_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
// #include <zephyr/sys/ring_buffer.h>

int advertising_module_init(void);
int advertising_start(void);
int application_init(void);
bool get_adv_progress(void);
bool check_update_availability(void);
int advertising_stop(void);

#endif // BEACON_MODULE_H
