#include <zephyr/logging/log.h>
#include "uart_module.h"

LOG_MODULE_REGISTER(uart_module, LOG_LEVEL_INF);

const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart2));
uint8_t rx_buf[5];  // Buffer for received messages


static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data) {
    switch (evt->type) {
        case UART_RX_RDY:
            if (evt->data.rx.len > 0) {
              LOG_INF("Received: %s", rx_buf);

              #if (!ROLE)
              if (strncmp((char *)rx_buf, "HELLO", 5) == 0) {
                    uart_send("ACK");
                    // wait_for_response("SYNC");
                } else if (strncmp((char *)rx_buf, "SYNC ", 4) == 0) {
                    // uart_send("SYNC");
                    LOG_INF("Synchronization Complete!");
              }
              #endif

							memset(rx_buf, 0, sizeof(rx_buf));  // Clear buffer	
            }
            // Re-enable UART RX
            uart_rx_enable(uart, rx_buf, sizeof(rx_buf), SYS_FOREVER_US);
            break;

        case UART_RX_DISABLED:
            LOG_WRN("UART RX disabled. Re-enabling...");
            uart_rx_enable(uart, rx_buf, sizeof(rx_buf), SYS_FOREVER_US);
            break;

        case UART_RX_STOPPED:
            LOG_ERR("UART RX stopped unexpectedly!");
            break;

        default:
            break;
    }
}

void uart_send(const char *msg) {
    uart_tx(uart, (uint8_t *)msg, strlen(msg), SYS_FOREVER_US);
    // LOG_INF("Sent: %s", msg);
}

void wait_for_response(const char *expected) {
    LOG_INF("Waiting for response: %s", expected);
    
    while (1) {
        k_sleep(K_MSEC(1));  // Small delay to avoid CPU overuse
        if (strncmp((char *)rx_buf, expected, strlen(expected)) == 0) {
            LOG_INF("Response received: %s", rx_buf);
            memset(rx_buf, 0, sizeof(rx_buf));  // Clear buffer after match
            return;  // Exit loop once response is received
        }
    }
}

void detect_slave(void) {
    LOG_INF("Searching for slave...");
    
    while (1) {
        uart_send("HELLO");
        k_sleep(K_MSEC(1000));  // Wait before retrying
        if (strncmp((char *)rx_buf, "ACK", 3) == 0) {
            LOG_INF("Slave detected!");
            memset(rx_buf, 0, sizeof(rx_buf));  // Clear buffer
            return;
        }
        // LOG_WRN("No response, retrying...");
    }

    
    uart_send("SYNC");
}

int uart_init(void) {
  if (!device_is_ready(uart)) {
        LOG_ERR("UART device not found!");
        return -1;
  } 
  
  int err = uart_callback_set(uart, uart_cb, NULL);
  if (err) {
      LOG_ERR("Failed to set UART callback: %d", err);
      return err;
  }

  // Enable UART RX
  uart_rx_enable(uart, rx_buf, sizeof(rx_buf), SYS_FOREVER_US);
  LOG_INF("UART init done");
  return 0;
}

