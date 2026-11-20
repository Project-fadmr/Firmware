//Texas Instruments AWR1843AOPEVM UART Driver for the ESP32-C3-Super-Mini
//Version: 0
//Revised: 20/11/2026 @voxvoltera

// !! GPIO18/19 are USB-device only (no host), so they can't talk to the EVM's CP2105.
// They have to be UART pins wired to the AWR's rawMSS/DSS UART lines instead,
// which kills native USB flashing/console. CLI (TX+RX) + DATA (RX) also needs a 3rd pin.

#include "awr1843_drv.h"

#include <stdbool.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "bsp.h"

static const char *TAG = "awr1843";

static const char *const RADAR_CFG[] = {
    // TODO: copy from debug driver
};

static const uint8_t MAGIC[8] = {0x02, 0x01, 0x04, 0x03, 0x06, 0x05, 0x08, 0x07};
static const char PROMPT[] = "mmwDemo:/>";

#define HEADER_LEN      40
#define TOTAL_LEN_OFF   12
#define BAUD       921600
#define CLI_TIMEOUT     pdMS_TO_TICKS(1000)
#define CMD_RETRIES     2

static SemaphoreHandle_t cli_lock;
static volatile bool enabled;

static esp_err_t send_cmd(const char *cmd)
{
    // TODO
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t awr1843_init(void)
{
    // !! use XTAL as UART source_clk, otherwise BSP's CPU/APB frequency switching breaks the baud rate
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t awr1843_config(void)
{
    // TODO
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t awr1843_enable(void)
{
    // !! check whether plain "sensorStart" after "sensorStop" reapplies the config; SDK 3.x has "sensorStart 0" to skip that
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t awr1843_read(uint8_t *buf, size_t cap, size_t *len, TickType_t timeout)
{
    // TODO
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t awr1843_disable(void)
{
    // !! UART RX stops in light sleep; BSP must disable before sleeping
    return ESP_ERR_NOT_SUPPORTED;
}

void awr1843_deinit(void)
{
    // TODO
}
