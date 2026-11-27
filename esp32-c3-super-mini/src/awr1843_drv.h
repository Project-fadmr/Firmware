//Texas Instruments AWR1843AOPEVM UART Driver for the ESP32-C3-Super-Mini
//Version: 1
//Revised: 27/11/2026 @voxvoltera

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

// PC driver allows 64 KiB; real frames are 224-352 B (see espnow_ll.md)
#define AWR1843_MAX_FRAME_LEN  1024
#define AWR1843_FRAME_PERIOD_MS 100   // also sets the frameCfg periodicity in RADAR_CFG

esp_err_t awr1843_init(void);
// Wake the radar's CLI and confirm it answers (prompt seen). Call after awr1843_init, before awr1843_config.
// On failure it logs which of wiring / baud / firmware is the likely culprit. Safe to call repeatedly.
esp_err_t awr1843_connect(void);
esp_err_t awr1843_config(void);
esp_err_t awr1843_enable(void);
esp_err_t awr1843_disable(void);
esp_err_t awr1843_read(uint8_t *buf, size_t cap, size_t *len, TickType_t timeout);
void awr1843_deinit(void);