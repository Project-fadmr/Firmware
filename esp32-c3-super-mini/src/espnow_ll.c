//ESP-NOW Link Layer for the ESP32-C3-Super-Mini
//Version: 0
//Revised: 24/11/2026 @molly


#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "esp_err.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#define ESPNOW_CHANNEL 6
#define MAX_PAYLOAD_SIZE 250

static const uint8_t broadcast_mac[6] = {
    0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF
};

esp_err_t link_layer_init(void)
{
    esp_err_t err;

    err = esp_netif_init();
    if (err != ESP_OK) return err;

    err = esp_event_loop_create_default();
    if (err != ESP_OK) return err;

    wifi_init_config_t wifi_config =
        WIFI_INIT_CONFIG_DEFAULT();

    err = esp_wifi_init(&wifi_config);
    if (err != ESP_OK) return err;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) return err;

    err = esp_wifi_start();
    if (err != ESP_OK) return err;

    err = esp_wifi_set_channel(
        ESPNOW_CHANNEL,
        WIFI_SECOND_CHAN_NONE
    );
    if (err != ESP_OK) return err;

    err = esp_now_init();
    if (err != ESP_OK) return err;

    esp_now_peer_info_t peer = {0};

    memcpy(peer.peer_addr, broadcast_mac, 6);

    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    return esp_now_add_peer(&peer);
}

esp_err_t link_layer_send(
    const uint8_t *data,
    size_t length
)
{
    if (data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (length > MAX_PAYLOAD_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    return esp_now_send(
        broadcast_mac,
        data,
        length
    );
}
