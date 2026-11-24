//ESP-NOW Link Layer for the ESP32-C3-Super-Mini
//Version: 0
//Revised: 24/11/2026 @molly

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#define LL_MAX_FRAME 1024

// The rx callback runs in the Wi-Fi task, keep it short.
// Do not call radio_on, radio_off or set_tx_power from it: deadlock risk with the send task.
typedef void (*link_layer_rx_cb_t)(const uint8_t *src_mac, const uint8_t *data, size_t len, int8_t rssi);

// If radio_on fails here, retry with link_layer_radio_on. A second init call returns ESP_OK without retrying.
esp_err_t link_layer_init(void);
// Not reentrant (static staging buffer): call from one task only.
esp_err_t link_layer_send(const uint8_t *data, size_t length);
esp_err_t link_layer_set_tx_power(int8_t quarter_dbm);
esp_err_t link_layer_set_rx_cb(link_layer_rx_cb_t cb);
esp_err_t link_layer_radio_on(void);
esp_err_t link_layer_radio_off(void);
