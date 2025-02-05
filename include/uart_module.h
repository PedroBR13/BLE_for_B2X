#ifndef UART_MODULE_H
#define UART_MODULE_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

void uart_send(const char *msg);
void wait_for_response(const char *expected);
void detect_slave(void);
int uart_init(void);

#endif // UART_MODULE_H