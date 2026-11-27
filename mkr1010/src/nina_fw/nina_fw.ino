#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <espnow_proto.h>

#define BUSY_PIN 33
#define QUEUE_LEN 8

typedef struct {
  ep_frame_t meta;
  uint8_t data[EP_MAX_FRAME];
} item_t;

static QueueHandle_t q;
static ep_reasm_t reasm;
static item_t rx_item;   // callback scratch (Wi-Fi task only)
static item_t tx_item;   // loop scratch
static item_t drop_item;  // drop-oldest sink, Wi-Fi task only
static uint8_t tx_buf[EP_MAX_FRAME + EP_OVERHEAD];

// Runs in the Wi-Fi task: reassemble and enqueue only, never touch the UART.
static void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  ep_frame_t f;
  if (!ep_reasm_push(&reasm, info->src_addr, info->rx_ctrl->rssi, data, len,
                     millis(), &f))
    return;
  // f.data points into the slot and dies on the next push, so copy now.
  rx_item.meta = f;
  memcpy(rx_item.data, f.data, f.len);
  if (xQueueSend(q, &rx_item, 0) != pdTRUE) {
    xQueueReceive(q, &drop_item, 0);
    xQueueSend(q, &rx_item, 0);
  }
}

void setup() {
  Serial.begin(460800);
  pinMode(BUSY_PIN, INPUT_PULLDOWN);
  ep_reasm_init(&reasm);
  q = xQueueCreate(QUEUE_LEN, sizeof(item_t));  // must exist before the callback
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
  esp_now_init();
  esp_now_register_recv_cb(onRecv);
}

void loop() {
  if (digitalRead(BUSY_PIN) == LOW && xQueuePeek(q, &tx_item, 0) == pdTRUE) {
    // Re-check after the peek: SAMD may have raised busy meanwhile.
    if (digitalRead(BUSY_PIN) != LOW) return;
    xQueueReceive(q, &tx_item, 0);
    size_t n = ep_encode(tx_item.meta.mac, tx_item.meta.rssi, tx_item.data,
                         tx_item.meta.len, tx_buf, sizeof(tx_buf));
    if (n) Serial.write(tx_buf, n);
  } else {
    delay(1);
  }
}
