//Texas Instruments AWR1843AOPEVM UART Driver for the ESP32-C3-Super-Mini
//Version: 0
//Revised: 20/11/2026 @voxvoltera

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

// PC driver allows 64 KiB; real frames are 224-352 B (see espnow_ll.md)
#define AWR1843_MAX_FRAME_LEN  1024

esp_err_t awr1843_init(void);
esp_err_t awr1843_config(void);
esp_err_t awr1843_enable(void);
esp_err_t awr1843_disable(void);
esp_err_t awr1843_read(uint8_t *buf, size_t cap, size_t *len, TickType_t timeout);
void awr1843_deinit(void);
