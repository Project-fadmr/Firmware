#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "sent ok" : "sent fail");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // SuperMini antenna mismatch: full power barely radiates
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR); // nina_fw listens LR-only
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
  esp_now_init();
  esp_now_register_send_cb(onSent);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, bcast, 6);
  peer.channel = 6;
  esp_now_add_peer(&peer);
}

void loop() {
  static uint32_t counter = 0;
  uint8_t buf[7] = {0x00, 0x00, 0x01}; // reasm header: seq 0, idx 0, count 1 -> one whole frame
  memcpy(buf + 3, &counter, 4);
  counter++;
  esp_now_send(bcast, buf, sizeof buf);
  delay(1000);
}
