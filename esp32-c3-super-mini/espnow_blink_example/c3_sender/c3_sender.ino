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
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_now_init();
  esp_now_register_send_cb(onSent);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, bcast, 6);
  peer.channel = 1;
  esp_now_add_peer(&peer);
}

void loop() {
  uint8_t b = 'B';
  esp_now_send(bcast, &b, 1);
  delay(1000);
}
