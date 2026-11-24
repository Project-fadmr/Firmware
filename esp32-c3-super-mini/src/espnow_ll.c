//ESP-NOW Link Layer for the ESP32-C3-Super-Mini
//Version: 0
//Revised: 24/11/2026 @molly

#include "espnow_ll.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#define TAG "espnow_ll"

#define ESPNOW_CHANNEL 6
#define LL_HDR_LEN 3
#define LL_PAYLOAD_MAX (ESP_NOW_MAX_DATA_LEN - LL_HDR_LEN)
#define LL_QUEUE_DEPTH 4
#define LL_SEND_TIMEOUT_MS 100
#define LL_TX_DEFAULT_QDBM 34

typedef struct {
    uint16_t length;
    uint8_t data[LL_MAX_FRAME];
} frame_item_t;

static const uint8_t broadcast_mac[6] = {
    0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF
};

static bool s_inited = false;
static volatile bool s_radio_on = false;
static int8_t s_tx_power = LL_TX_DEFAULT_QDBM;
static link_layer_rx_cb_t s_rx_cb = NULL;
static QueueHandle_t s_queue = NULL;
static SemaphoreHandle_t s_send_sem = NULL;
static SemaphoreHandle_t s_lock = NULL;
static esp_now_send_status_t s_send_status;
static uint8_t s_frame_seq = 0;
// Single staging buffer, so link_layer_send must be called from one task only.
static frame_item_t s_stage;

static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    s_send_status = status;
    xSemaphoreGive(s_send_sem);
}

static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len)
{
    if (s_rx_cb != NULL) {
        s_rx_cb(info->src_addr, data, (size_t)data_len, info->rx_ctrl->rssi);
    }
}

static void link_tx_task(void *arg)
{
    (void)arg;
    static frame_item_t item;
    static uint8_t pkt[ESP_NOW_MAX_DATA_LEN];

    for (;;) {
        xQueueReceive(s_queue, &item, portMAX_DELAY);

        uint8_t count = (uint8_t)((item.length + LL_PAYLOAD_MAX - 1) / LL_PAYLOAD_MAX);

        for (uint8_t i = 0; i < count; i++) {
            size_t off = (size_t)i * LL_PAYLOAD_MAX;
            size_t n = item.length - off;
            if (n > LL_PAYLOAD_MAX) n = LL_PAYLOAD_MAX;

            pkt[0] = s_frame_seq;
            pkt[1] = i;
            pkt[2] = count;
            memcpy(&pkt[LL_HDR_LEN], &item.data[off], n);

            xSemaphoreTake(s_lock, portMAX_DELAY);
            esp_err_t err = ESP_ERR_INVALID_STATE;
            if (s_radio_on) {
                // Drop a late callback from an earlier timed-out packet.
                xSemaphoreTake(s_send_sem, 0);
                err = esp_now_send(broadcast_mac, pkt, LL_HDR_LEN + n);
            }
            xSemaphoreGive(s_lock);

            if (err == ESP_OK) {
                if (xSemaphoreTake(s_send_sem, pdMS_TO_TICKS(LL_SEND_TIMEOUT_MS)) != pdTRUE) {
                    err = ESP_ERR_TIMEOUT;
                } else if (s_send_status != ESP_NOW_SEND_SUCCESS) {
                    err = ESP_FAIL;
                }
            }

            if (err != ESP_OK) {
                ESP_LOGW(TAG, "frame seq %u packet %u/%u failed: %s, dropping frame",
                         s_frame_seq, i, count, esp_err_to_name(err));
                break;
            }
        }

        s_frame_seq++;
    }
}

esp_err_t link_layer_init(void)
{
    if (s_inited) return ESP_OK;

    esp_err_t err;

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    wifi_init_config_t wifi_config =
        WIFI_INIT_CONFIG_DEFAULT();

    err = esp_wifi_init(&wifi_config);
    if (err != ESP_OK) return err;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) return err;

    s_queue = xQueueCreate(LL_QUEUE_DEPTH, sizeof(frame_item_t));
    if (s_queue == NULL) return ESP_ERR_NO_MEM;

    s_send_sem = xSemaphoreCreateBinary();
    if (s_send_sem == NULL) return ESP_ERR_NO_MEM;

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) return ESP_ERR_NO_MEM;

    if (xTaskCreate(link_tx_task, "espnow_ll_tx", 4096, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_inited = true;

    return link_layer_radio_on();
}

esp_err_t link_layer_radio_on(void)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_lock, portMAX_DELAY);

    esp_err_t err = ESP_OK;
    if (s_radio_on) goto done;

    err = esp_wifi_start();
    if (err != ESP_OK) goto done;

    err = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
    if (err != ESP_OK) goto fail;

    err = esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) goto fail;

    err = esp_wifi_set_max_tx_power(s_tx_power);
    if (err != ESP_OK) goto fail;

    err = esp_now_init();
    if (err != ESP_OK) goto fail;

    err = esp_now_register_send_cb(espnow_send_cb);
    if (err != ESP_OK) goto fail;

    err = esp_now_register_recv_cb(espnow_recv_cb);
    if (err != ESP_OK) goto fail;

    esp_now_peer_info_t peer = {0};

    memcpy(peer.peer_addr, broadcast_mac, 6);

    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    err = esp_now_add_peer(&peer);
    if (err != ESP_OK) goto fail;

    s_radio_on = true;
    goto done;

fail:
    esp_now_deinit();
    esp_wifi_stop();
done:
    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t link_layer_radio_off(void)
{
    if (!s_inited) return ESP_OK;

    xSemaphoreTake(s_lock, portMAX_DELAY);

    esp_err_t err = ESP_OK;
    if (s_radio_on) {
        s_radio_on = false;
        err = esp_now_deinit();
        if (err == ESP_OK) err = esp_wifi_stop();
        xQueueReset(s_queue);
    }

    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t link_layer_send(const uint8_t *data, size_t length)
{
    if (!s_radio_on) return ESP_ERR_INVALID_STATE;

    if (data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (length > LL_MAX_FRAME) {
        return ESP_ERR_INVALID_SIZE;
    }

    s_stage.length = (uint16_t)length;
    memcpy(s_stage.data, data, length);

    if (xQueueSend(s_queue, &s_stage, 0) != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t link_layer_set_tx_power(int8_t quarter_dbm)
{
    if (quarter_dbm < 8 || quarter_dbm > 84) return ESP_ERR_INVALID_ARG;

    if (!s_inited) {
        s_tx_power = quarter_dbm;
        return ESP_OK;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    esp_err_t err = ESP_OK;
    if (s_radio_on) {
        err = esp_wifi_set_max_tx_power(quarter_dbm);
    }
    if (err == ESP_OK) {
        s_tx_power = quarter_dbm;
    }

    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t link_layer_set_rx_cb(link_layer_rx_cb_t cb)
{
    s_rx_cb = cb;
    return ESP_OK;
}
