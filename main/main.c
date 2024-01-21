#include <driver/rmt.h>
#include <driver/gpio.h>
#include <esp_log.h>

#include "components\dht\dht.h"

#define IR_TX_PIN GPIO_NUM_4  
#define IR_RX_PIN GPIO_NUM_5  

dht_sensor_type_t sensor_type = DHT_TYPE_DHT11;  
gpio_num_t dht_pin = GPIO_NUM_6;  

rmt_channel_t tx_channel;
rmt_channel_t rx_channel;

// Example signal data
uint32_t ac_on_signal[] = {9000, 4500, 4500, 560, 560, 4500, 560, 4500, 4500, 560, 560, 4500, 560, 4500, 560, 4500, 4500, 560, 4500, 560, 560, 4500, 4500, 560, 560, 4500, 560, 4500, 560};   //0x1234
uint32_t ac_off_signal[] = {9000, 4500, 4500, 560, 560, 4500, 560, 4500, 560, 560, 4500, 4500, 560, 4500, 560, 560, 4500, 4500, 560, 4500, 560, 4500, 4500, 560, 560, 4500, 560, 4500, 4500, 560, 560};   //0x4321

void send_signal(uint32_t signal[], size_t length) {
    esp_err_t err = rmt_transmit(tx_channel, NULL, signal, length, NULL);
    if (err != ESP_OK) {
        ESP_LOGE("send_signal", "Failed to transmit signal: %s", esp_err_to_name(err));
    }
}

void rx_task(void *arg) {
    while (1) {
        esp_err_t err = rmt_rx_start(rx_channel, true);
        if (err != ESP_OK) {
            ESP_LOGE("rx_task", "Failed to receive signal: %s", esp_err_to_name(err));
        } else {
            size_t length = 0;
            // Access received data
            rmt_item32_t *items = rmt_get_items(rx_channel, &length);
            // Process received signal data

            //Print raw data for analysis
            for (int i = 0; i < length; i++) {
                printf("%u ", items[i].duration0);
            }
            printf("\n");
        }
        rmt_rx_stop(rx_channel);
        //vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    rmt_config_t tx_config = {
        .channel = RMT_CHANNEL_0,  
        .gpio_num = IR_TX_PIN,
        .mem_block_num = 1,
        .clk_div = 80,  
        .rmt_mode = RMT_MODE_TX,
        .tx_config = {
            .carrier_freq_hz = 38000,
            .carrier_duty_percent = 50,
            .carrier_level = 1,
            .carrier_en = 1,
            .idle_level = 0,
            .idle_output_en = 1,
        },
    };

    rmt_config_t rx_config = {
        .channel = RMT_CHANNEL_1, 
        .gpio_num = IR_RX_PIN,
        .mem_block_num = 1,
        .clk_div = 80,
        .rmt_mode = RMT_MODE_RX,
        .rx_config = {
            .filter_en = true,
            .filter_ticks_thresh = 100, 
            .idle_threshold = 10000,  
        },
    };

    esp_err_t err = rmt_config(&tx_config);
    if (err != ESP_OK) {
        ESP_LOGE("app_main", "Failed to configure TX RMT: %s", esp_err_to_name(err));
        return;  // Handle error, e.g., halt execution, indicate failure
    }

    err = rmt_config(&rx_config);
    if (err != ESP_OK) {
        ESP_LOGE("app_main", "Failed to configure RX RMT: %s", esp_err_to_name(err));
        return;  // Handle error, e.g., halt execution, indicate failure
    }

    err = rmt_driver_install(tx_config.channel, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE("app_main", "Failed to install RMT driver: %s", esp_err_to_name(err));
        return;  // Handle error
    }

    err = rmt_driver_install(rx_config.channel, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE("app_main", "Failed to install RMT driver: %s", esp_err_to_name(err));
        return;  // Handle error
    }

    // Get RMT channel handles
    err = rmt_get_channel_handle(tx_config.channel, &tx_channel);
    if (err != ESP_OK) {
        ESP_LOGE("app_main", "Failed to get TX channel handle: %s", esp_err_to_name(err));
        return;  // Handle error
    }

    err = rmt_get_channel_handle(rx_config.channel, &rx_channel);
    if (err != ESP_OK) {
        ESP_LOGE("app_main", "Failed to get RX channel handle: %s", esp_err_to_name(err));
        return;  // Handle error
    }
    
    int16_t humidity;
    int16_t temperature;
    esp_err_t err = dht_read_data(sensor_type, dht_pin, &humidity, &temperature);
    if (err != ESP_OK) {
        ESP_LOGE("app_main", "Failed to read DHT11 data: %s", esp_err_to_name(err));
    } else {
        float temp_celsius = temperature / 10.0f;  // Convert to Celsius
        ESP_LOGI("app_main", "Temperature: %.1f°C", temp_celsius);

        // Control AC based on temperature
        if (temp_celsius > 25.0f) {  // Adjust threshold as needed
            send_signal(ac_on_signal, sizeof(ac_on_signal) / sizeof(*ac_on_signal));
        } else {
            send_signal(ac_off_signal, sizeof(ac_off_signal) / sizeof(*ac_off_signal));
        }
    }


    xTaskCreate(rx_task, "rx_task", 2048, NULL, 10, NULL);  // Start RX task
}


