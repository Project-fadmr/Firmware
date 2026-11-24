#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  Serial.write(data, len);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
  esp_now_init();
  esp_now_register_recv_cb(onRecv);
}

void loop() {}
