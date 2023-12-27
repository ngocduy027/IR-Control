#include <driver/gpio.h>
#include <driver/rmt.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define IR_TX_GPIO GPIO_NUM_4
#define IR_RX_GPIO GPIO_NUM_14

uint32_t ac_on_code = 0xAC_ON_CODE;
uint32_t ac_off_code = 0xAC_OFF_CODE;

bool ac_is_on = false;

rmt_channel_t rmt_tx_channel = RMT_CHANNEL_0;

void app_main() {
  // Initialize RMT transmitter and receiver
  rmt_config_t rmt_tx_config = RMT_DEFAULT_CONFIG_TX(IR_TX_GPIO, rmt_tx_channel);
  rmt_config(&rmt_tx_config);
  rmt_driver_install(rmt_tx_channel, 0, 0);  // Set 0-length RX buffer for transmitter only

  // Print initial state to the console
  ESP_LOGI("AC", "AC is OFF");

  rmt_rx_start(rmt_tx_channel, true);  // Start receiving IR signals

  while (true) {
    size_t rx_size = 0;
    rmt_item32_t* rx_items = rmt_get_ringbuf_item(rmt_tx_channel, &rx_size);

    if (rx_size > 0) {
      uint32_t received_code = nec_decode(rx_items, rx_size);
      ESP_LOGI("AC", "IR code received: 0x%08x", received_code);

      // Handle received code
      if (received_code == ac_on_code) {
        ac_is_on = true;
        ESP_LOGI("AC", "AC turned ON");
      } else if (received_code == ac_off_code) {
        ac_is_on = false;
        ESP_LOGI("AC", "AC turned OFF");
      } else {
        ESP_LOGI("AC", "Unknown IR code received");
      }
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void send_ir_code(uint32_t code) {
  rmt_item32_t items[32];  // Maximum NEC code length
  int item_count = nec_encode(code, items);
  rmt_write_items(rmt_tx_channel, items, item_count, false);
  rmt_wait_tx_done(rmt_tx_channel, portMAX_DELAY);
  ESP_LOGI("AC", "IR code sent: 0x%08x", code);
}
