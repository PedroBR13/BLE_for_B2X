#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
// #include <modem/nrf_modem_lib.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/bluetooth/hci.h>
#include "gnss_module.h" // Include the GNSS module
#include "scan_module.h"   // Include the BLE module
#include "beacon_module.h"  // Include the BLE beacon module
#include "ble_settings.h"
#include "sdcard_module.h"
#include "uart_module.h"

LOG_MODULE_REGISTER(main_logging, LOG_LEVEL_INF); // Register the logging module

// Define the runtime variables globally so they can be accessed from other files
uint32_t runtime_ms = 0;
uint32_t previous_runtime_ms = 0;

typedef enum {
    STATE_GNSS_SEARCH,
    STATE_SCANNING,
    STATE_ADVERTISING,
    STATE_DONE,
    STATE_NEW_TEST_FILE,
    STATE_ERROR,
    STATE_UART_SYNC
} app_state_t;

static app_state_t current_state = STATE_DONE;  // Initialize to GNSS search state

// static int time = 0;
// static uint32_t start_time = 0;
// static uint32_t elapsed_cycles = 0;
// static uint32_t target_cycles = 0; 

#if ROLE
static uint8_t test_count = 0;
#endif

void error_callback(const char *error_message)
{
    LOG_ERR("SD Card Error: %s", error_message);
    reset_packet_queue();
    append_error();
    current_state = STATE_NEW_TEST_FILE;
}

#if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)

        #define EXPECTED_PULSE_COUNT 10 

        /* The devicetree node identifier for the "led0" alias. */
        #define LED0_NODE DT_ALIAS(led0)
        #define LED1_NODE DT_ALIAS(led1)
        #define LED2_NODE DT_ALIAS(led2)
        #define LED3_NODE DT_ALIAS(led3)
        #define BUTTON_NODE DT_ALIAS(sw0)
        #define SYNC_PIN  DT_ALIAS(sync)

        static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
        static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
        static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
        static const struct gpio_dt_spec led4 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);
        static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
        static const struct gpio_dt_spec sync = GPIO_DT_SPEC_GET(SYNC_PIN, gpios);
        
        // Timers
        static struct k_timer timeout_timer;
        #if ROLE
            void sync_pulse(void) {
                gpio_pin_toggle_dt(&sync);
                gpio_pin_toggle_dt(&sync);
                gpio_pin_toggle_dt(&led3);
                LOG_INF("Sync pulse sent at %u ms", k_uptime_get());
            }
        #endif

        #if !ROLE
            static struct gpio_callback sync_cb_data;
            static bool synchronized = false;
            static bool synchronized_done = false;
            static uint32_t pulse_count = 0; // Track pulse count
            static uint32_t last_sync_time = 0; // Timestamp of the last sync pulse
            static uint32_t sync_period = 0; // Time difference between sync pulses

            // Callback function for sync pulse
            void sync_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
                uint32_t current_time = k_uptime_get();
                
                // LOG_INF("Sync pulse received at %u ms", current_time);

                if (!synchronized) {
                    // Start synchronization after the first pulse
                    synchronized = true;
                    pulse_count = 1;
                    last_sync_time = current_time;
                } else {
                    // Calculate the time difference between consecutive sync pulses
                    sync_period = current_time - last_sync_time;
                    last_sync_time = current_time;
                    pulse_count++;

                    // Log sync period (frequency of sync pulse)
                    LOG_INF("Sync period: %u ms", sync_period);

                    // If we've received 10 pulses, stop listening and start toggling the LED
                    if (pulse_count >= EXPECTED_PULSE_COUNT) {
                        // LOG_INF("Received 10 pulses. Starting LED toggle.");
                        gpio_pin_interrupt_configure_dt(&sync, GPIO_INT_DISABLE); // Disable further interrupts
                    }
                }
            }
        #endif

        static struct k_timer led_timer;

        static struct gpio_callback button_cb_data;

        // Function to turn off the LED
        static void led_off(struct k_timer *timer) {
            gpio_pin_configure_dt(&led4, GPIO_OUTPUT_INACTIVE);
        }
        
        // Reset the LED timer and turn on the LED
        static void reset_led_timer(void) {
            gpio_pin_configure_dt(&led4, GPIO_OUTPUT_ACTIVE);  // Turn on the LED
            k_timer_start(&led_timer, K_MSEC(100), K_NO_WAIT);  // Reset the timer (1 second)
        }

        // #if ROLE
            static bool first_test = true;
        // #endif

        static bool recording_status = false;

        #define DEBOUNCE_DELAY_MS 500
        static uint32_t last_press_time = 0;

        void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
        {
            uint32_t current_time = k_uptime_get();

            if ((current_time - last_press_time) > DEBOUNCE_DELAY_MS) {
                last_press_time = current_time;
                #if NLOS_TEST
                    // #if ROLE
                    if (recording_status) {
                        LOG_INF("Stop recording");
                        append_stop();
                        switch_recording(false);
                        gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
                        recording_status = false;
                    } else {
                        // append_null();
                        LOG_INF("Start recording");
                        switch_recording(true);
                        gpio_pin_configure_dt(&led2, GPIO_OUTPUT_ACTIVE);
                        recording_status = true;
                    }
                    // #else
                    //     gpio_pin_toggle_dt(&led2);
                    //     LOG_INF("Button pressed");
                    // #endif
                #endif
            }
        }

    // #if ROLE
        static void timer_handler(struct k_timer *timer_id) {
            current_state = STATE_NEW_TEST_FILE; 
        }     
    // #endif
#endif



// Callback function for first GNSS fix
// void on_first_fix_acquired(void) {
//     LOG_INF("Switching from GNSS search to Bluetooth scanning");
//     int err = application_init();
//     if (err) {
//         LOG_ERR("Application init failed");
//         return;
//     }
//     current_state = STATE_DONE;
// }

int main(void) {
    int err;
    LOG_INF("Starting B2B device...");

    #if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
        int ret;
        if (!gpio_is_ready_dt(&led1)) {
            return 0;
        }
        if (!gpio_is_ready_dt(&led2)) {
            return 0;
        }
        if (!gpio_is_ready_dt(&led3)) {
            return 0;
        }
        if (!gpio_is_ready_dt(&led4)) {
            return 0;
        }
        if (!gpio_is_ready_dt(&button)) {
            return 0;
        }

        if (!gpio_is_ready_dt(&sync)) {
            return 0;
        }

        ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
        if (ret < 0) {
            return -1;
        }

        gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);   
        gpio_pin_configure_dt(&led2, GPIO_OUTPUT_ACTIVE);
        
        #if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
            #if ROLE
                gpio_pin_configure_dt(&sync, GPIO_OUTPUT);
                // Register the error callback with the SD card module
                set_error_handler(error_callback);
                
                err = sdcard_init();
                if (err) {
                    LOG_ERR("Failed to initialize SD Card, error: %d", err);
                    return 0;
                } else {
                    create_csv();
                } 

                append_csv(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); 
            #endif
            #if !ROLE
                // Configure sync pin (input with pull-up) and interrupt for rising edge
                gpio_pin_configure_dt(&sync, GPIO_INPUT | GPIO_PULL_UP);
                gpio_pin_interrupt_configure_dt(&sync, GPIO_INT_EDGE_TO_ACTIVE);

                // Initialize callback for sync pulse
                gpio_init_callback(&sync_cb_data, sync_callback, BIT(sync.pin));
                gpio_add_callback(sync.port, &sync_cb_data);
            #endif
        #endif

            gpio_pin_configure_dt(&led3, GPIO_OUTPUT_ACTIVE);
            gpio_pin_configure_dt(&led4, GPIO_OUTPUT_ACTIVE);
        
    #endif

    // err = nrf_modem_lib_init();
    // if (err) {
    //     LOG_ERR("Failed to initialize modem, error: %d", err);
    //     return -1;
    // }

    // // Setup GNSS with the first-fix callback
    // if (setup_gnss(on_first_fix_acquired) != 0) {
    //     LOG_ERR("GNSS setup failed");
    //     return -1;
    // }

    // Main loop
    while (true) {
        switch (current_state) {
            case STATE_GNSS_SEARCH:
                // LOG_INF("Searching first fix");
                break;

            case STATE_SCANNING:
                // Initialize and start Bluetooth scanning
                // time = k_uptime_get();
                // start_time = k_cycle_get_32(); // Get CPU cycle count
                // int scan_duration = 0;
                // LOG_INF("Starting scan: %d", k_uptime_get());
                reset_packet_received();
                #if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
                    ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
                    if (ret < 0) {
                        return 0;
                    }
                #endif

                err = ble_start_scanning();
                if (err) {
                    LOG_ERR("BLE scanning start failed");
                    return err;
                }
                
                k_sleep(K_MSEC(SCAN_WINDOW_MAIN));
                // scan_duration = scan_duration + SCAN_WINDOW_MAIN;
                #if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
                    if (is_packet_received()) {
                        reset_led_timer();
                    }
                #endif

                // keep scanning if no available data to send
                while (!check_update_availability()) {
                    // LOG_INF("Scan interval reset due to no data to send. \n");
                    k_sleep(K_MSEC(SCAN_WINDOW_MAIN));
                    // scan_duration = scan_duration + SCAN_WINDOW_MAIN;
                    #if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
                        if (is_packet_received()) {
                            reset_led_timer();
                        }
                    #endif
                }
                // LOG_INF("Active scan: %d", k_uptime_get()-time1);

                // LOG_INF("Stoppping scan: %d", k_uptime_get());
                // int time2 = k_uptime_get();
                // err = bt_le_scan_stop();
                // if (err) {
                //     LOG_ERR("Stopping scanning failed (err %d)\n", err);
                //     return 0;
                // }
                // start_time = k_cycle_get_32(); // Get CPU cycle count

                int err = bt_le_scan_stop();
                if (err) {
                    LOG_ERR("Stopping scanning failed (err %d)\n", err);
                    return 0;
                }

                // elapsed_cycles = k_cycle_get_32() - start_time;
                // target_cycles = k_ms_to_cyc_ceil32(3); // Convert 1 ms to CPU cycles

                // if (elapsed_cycles < target_cycles) {
                //     k_busy_wait(((target_cycles - elapsed_cycles) * 1000000) / sys_clock_hw_cycles_per_sec());
                // }

                // LOG_INF("Scan stop duration: %d", k_uptime_get()-time2);

                // Move to ADVERTISING state
                if (current_state != STATE_NEW_TEST_FILE){
                    current_state = STATE_ADVERTISING;
                }

                // elapsed_cycles = k_cycle_get_32() - start_time - k_ms_to_cyc_ceil32(scan_duration-5);
                // target_cycles = k_ms_to_cyc_ceil32(5); // Convert 1 ms to CPU cycles

                // if (elapsed_cycles < target_cycles) {
                //     k_busy_wait(((target_cycles - elapsed_cycles) * 1000000) / sys_clock_hw_cycles_per_sec());
                // }

                // LOG_INF("Scan duration: %d", k_uptime_get()-time);  
                break;

            case STATE_ADVERTISING:
                // LOG_INF("Starting beacon: %d", k_uptime_get());
                // time = k_uptime_get();
                #if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
                    ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
                    if (ret < 0) {
                        return 0;
                    }
                    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);

                    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin)); 	
                    gpio_add_callback(button.port, &button_cb_data);
                #endif
                
                #if NLOS_TEST
                    #if ROLE
                        if(recording_status) {
                            err = advertising_start(false);
                        } else {
                            err = advertising_start(true);
                        }
                    #else
                        err = advertising_start(false); 
                    #endif
                #else
                    err = advertising_start(false);
                #endif
                if (err) {
                    LOG_ERR("Advertising start failed");
                    #ifdef CONFIG_BOARD_NRF9160DK_NRF52840
                    return err;
                    #else
                    return err;
                    append_error();
                    current_state = STATE_NEW_TEST_FILE;
                    break;
                    #endif
                }

                // LOG_INF("Beacon start duration: %d", k_uptime_get()-time);

                // Wait here until advertising completes
                while (!get_adv_progress()) {
                    k_sleep(K_MSEC(1)); // Small delay to avoid CPU overuse
                    // LOG_INF("transmission not done");
                }

                advertising_stop();

                if (current_state != STATE_NEW_TEST_FILE){
                    current_state = STATE_SCANNING;
                }
                // LOG_INF("Advertising duration: %d", k_uptime_get()-time);   
                break;

            case STATE_DONE:
                // LOG_INF("Process done. Going to scanning state");
                #if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
                    gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
                    gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
                    gpio_pin_configure_dt(&led3, GPIO_OUTPUT_ACTIVE);
                    gpio_pin_configure_dt(&led4, GPIO_OUTPUT_INACTIVE);
                #endif

                // Enable BLE 
                err = bt_enable(NULL);
                if (err) {
                    LOG_ERR("Bluetooth init failed (err %d)", err);
                    return err;
                }
                
                
                // Initialize and start Bluetooth advertising
                err = advertising_module_init();
                if (err) {
                    LOG_ERR("Advertising module init failed");
                    return err;
                }
                
                LOG_INF("Bluetooth initialized");
                LOG_INF("Msg generation: %d ms / Number of copies: %d / Scan Window: %d ms / Test: %s / Time shift: %d ms / Role: %d", 
                    INTERVAL, PACKET_COPIES,SCAN_WINDOW_MAIN,CSV_TEST_NAME, TEST_SHIFT,ROLE);
                
                #ifdef CONFIG_BOARD_NRF9160DK_NRF52840
                    current_state = STATE_SCANNING;
                #else
                    k_timer_init(&led_timer, led_off, NULL);
                    current_state = STATE_UART_SYNC;
                #endif
                break;
            
            #if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
                case STATE_UART_SYNC:
                    LOG_INF("Start UART sychronization");
                    err = uart_init();
                    if (err) {
                        LOG_ERR("UART init failed");
                        return err;
                    }

                    #if ROLE
                        // **Keep sending "HELLO" until slave responds**
                        detect_slave();
                        // uart_send("SYNC");
                        // int i = 0;
                        // while (i < EXPECTED_PULSE_COUNT) {
                        //     k_sleep(K_MSEC(200));
                        //     sync_pulse();
                        //     i++;
                        // }
                    #else
                        LOG_INF("Searching for master...");
                        wait_for_response("HELLO");
                        // LOG_INF("Master found. Starting synchronization...");
                        // while (!synchronized_done) {
                        //     if (synchronized && pulse_count >= EXPECTED_PULSE_COUNT) {
                        //         // After receiving 10 pulses, blink LED at the sync period frequency
                        //         if (sync_period > 0) {
                        //             // blink_led();
                        //             // LOG_INF("Blinking LED at %u ms", sync_period);
                        //             k_sleep(K_MSEC(sync_period));  // Sleep for the sync period to blink the LED
                        //             synchronized_done=true;
                        //         }
                        //     } else {
                        //         // If not yet synchronized, just wait
                        //         k_sleep(K_MSEC(1));  // Sleep to avoid tight loop
                        //     }
                        // }
                        // synchronized = false;  // Reset flag for next use
                        // pulse_count = 0; // Reset pulse count for next use
                        // sync_period = 0; // Reset sync period for next use
                        // synchronized_done=false;
                    #endif
                    
                    #if NLOS_TEST
                        switch_recording(false);
                    #else 
                        switch_recording(true);
                    #endif
                    
                    #if !defined(CONFIG_BOARD_NRF9160DK_NRF52840)
                        // #if ROLE
                            #if !(NLOS_TEST)
                                k_timer_init(&timeout_timer, timer_handler, NULL);
                                // k_timer_start(&timeout_timer, K_SECONDS(RUNAWAY_PERIOD), K_SECONDS(TEST_PERIOD));
                                #if ROLE
                                if (first_test) {
                                    first_test = false;
                                } else {
                                    LOG_INF("Time %u shift added",TEST_SHIFT*test_count);
                                    // trigger_time_shift();
                                    k_sleep(K_MSEC(TEST_SHIFT*test_count));
                                }

                                #endif
                                k_timer_start(&timeout_timer, K_SECONDS(TEST_PERIOD), K_NO_WAIT);
                            #endif
                        // #endif

                        gpio_pin_configure_dt(&led3, GPIO_OUTPUT_INACTIVE);
                    #endif

                    reset_last_packet_time();

                    // LOG_INF("App starting...");
                    err = application_init();
                    if (err) {
                        LOG_ERR("Application init failed");
                        return err;
                    }
                    // LOG_INF("App started");

                    // LOG_INF("Msg generation: %d ms / Number of copies: %d / Scan Window: %d ms / Test: %s / Time shift: %d ms / Role: %d", 
                    // INTERVAL, PACKET_COPIES,SCAN_WINDOW_MAIN,CSV_TEST_NAME, TEST_SHIFT,ROLE);

                    current_state = STATE_SCANNING;
                    break;

            // #if ROLE
                case STATE_NEW_TEST_FILE:
                    #if ROLE
                    append_null();
                    reset_last_packet_time();
                    #endif
                    k_timer_stop(&timeout_timer);

                    err = application_stop();
                    if (err) {
                        LOG_ERR("Application stop failed");
                        return err;
                    }
                    #if ROLE
                    switch_recording(false);
                    // if (first_test) {
                    //     first_test = false;
                    // } else {
                    //     trigger_time_shift();
                    //     LOG_INF("Time shift added at: %lld", k_uptime_get());
                    // }  
                    #endif

                    
                    
                    
                    
                    #if ROLE
                    // switch_recording(true);
                    test_count++;
                    LOG_INF("New test started. Test count: %u \n", test_count);
                    #endif

                    // err = application_init();
                    // if (err) {
                    //     LOG_ERR("Application init failed");
                    //     return err;
                    // }
                    current_state = STATE_UART_SYNC;
                    break;
                // #endif
            #endif


            default:
                LOG_ERR("Unknown state");
                return -1;
        }

        // Simulate 1 ms delay to prevent CPU overload
        k_sleep(K_MSEC(1));
    }

    return 0;
}
