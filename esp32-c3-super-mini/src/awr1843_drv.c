//Texas Instruments AWR1843AOPEVM UART Driver for the ESP32-C3-Super-Mini
//Version: 2
//Revised: 27/11/2026 @voxvoltera (connect/probe + diagnostics)


#include "awr1843_drv.h"

#include <stdbool.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "bsp.h"

static const char *TAG = "awr1843";

static const char *const RADAR_CFG[] = {
    "dfeDataOutputMode 1",
    "channelCfg 15 7 0",
    "adcCfg 2 1",
    "adcbufCfg -1 0 1 1 1",
    "profileCfg 0 77 8 7 18.64 0 0 30 1 133 12499 0 0 30",
    "chirpCfg 0 0 0 0 0 0 0 1",
    "chirpCfg 1 1 0 0 0 0 0 2",
    "chirpCfg 2 2 0 0 0 0 0 4",
    "frameCfg 0 2 16 0 100 1 0",
    "lowPower 0 0",
    "guiMonitor -1 1 0 0 0 0 0",
    "cfarCfg -1 0 2 8 4 3 0 15 1",
    "cfarCfg -1 1 0 4 2 3 1 15 1",
    "multiObjBeamForming -1 1 0.5",
    "clutterRemoval -1 0",
    "calibDcRangeSig -1 0 -5 8 256",
    "extendedMaxVelocity -1 0",
    "lvdsStreamCfg -1 0 0 0",
    "compRangeBiasAndRxChanPhase 0.0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0",
    "measureRangeBiasAndRxChanPhase 0 1.5 0.2",
    "CQRxSatMonitor 0 3 4 31 0",
    "CQSigImgMonitor 0 65 4",
    "analogMonitor 0 0",
    "aoaFovCfg -1 -90 90 -90 90",
    "cfarFovCfg -1 0 0 49.99",
    "cfarFovCfg -1 1 -12.18 12.18",
    "calibData 0 0 0",
};

static const uint8_t MAGIC[8] = {0x02, 0x01, 0x04, 0x03, 0x06, 0x05, 0x08, 0x07};
static const char PROMPT[] = "mmwDemo:/>";

#define HEADER_LEN      40
#define TOTAL_LEN_OFF   12
#define BAUD       892857
#define RADAR_UART      UART_NUM_1
#define CLI_TIMEOUT     pdMS_TO_TICKS(1000)
#define CMD_RETRIES     2
#define START_TIMEOUT   pdMS_TO_TICKS(3000)   // sensorStart runs calibration before it prints Done

static SemaphoreHandle_t cli_lock;
static volatile bool enabled;
static int s_baud = BAUD;

// !! sensorStop's reply can arrive behind 1-2 in-flight frames; static is safe because callers hold cli_lock
static char s_rx[4 * AWR1843_MAX_FRAME_LEN];

// pyserial's read_until(PROMPT): stops at the prompt, a full buffer, or after `timeout` in total
static size_t read_until_prompt(char *buf, size_t cap, TickType_t timeout)
{
    const size_t plen = sizeof(PROMPT) - 1;
    size_t len = 0;
    TimeOut_t start;
    vTaskSetTimeOutState(&start);

    while (len < cap - 1 && !xTaskCheckForTimeOut(&start, &timeout)) {
        if (uart_read_bytes(RADAR_UART, &buf[len], 1, timeout) == 1) {
            len++;
            if (len >= plen && memcmp(&buf[len - plen], PROMPT, plen) == 0) {
                break;
            }
        }
    }
    buf[len] = '\0';
    return len;
}

static esp_err_t send_cmd_ex(const char *cmd, TickType_t timeout, int retries)
{
    // !! sensorStop's reply can arrive behind 1-2 in-flight frames; static is safe because callers hold cli_lock
    static char reply[4 * AWR1843_MAX_FRAME_LEN];
    size_t len = 0;
    esp_err_t err = ESP_FAIL;

    // !! the radar replies over UART, which light sleep would drop: hold the lock for the whole exchange
    bsp_power_no_sleep_acquire();

    for (int i = 0; i < retries; i++) {
        uart_flush_input(RADAR_UART);
        uart_write_bytes(RADAR_UART, cmd, strlen(cmd));
        uart_write_bytes(RADAR_UART, "\r\n", 2);   // !! bare "\n" only got an echo back
        len = read_until_prompt(s_rx, sizeof(s_rx), timeout);

        // memmem, not strstr: frames still in flight around sensorStop put 0x00 bytes before the text
        if (memmem(s_rx, len, "Done", 4)) {
            err = ESP_OK;
            break;
        }
        ESP_LOGW(TAG, "'%s' no Done (%u bytes back), retrying", cmd, (unsigned)len);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "'%s' failed, board replied: %.*s", cmd, (int)len, s_rx);
        if (len > 0) {
            ESP_LOG_BUFFER_HEXDUMP(TAG, s_rx, len < 48 ? len : 48, ESP_LOG_ERROR);
        }
    }
    bsp_power_no_sleep_release();
    return err;
}

static esp_err_t send_cmd(const char *cmd)
{
    return send_cmd_ex(cmd, CLI_TIMEOUT, CMD_RETRIES);
}

// Poke the CLI with empty lines until it answers with its prompt. Unlike a real command this works in any radar
// state: idle, still booting, mid-stream, or with half a command stuck in its line buffer.
static bool cli_probe(TickType_t per_try, int tries, size_t *got)
{
    size_t len = 0;
    for (int i = 0; i < tries; i++) {
        uart_flush_input(RADAR_UART);
        uart_write_bytes(RADAR_UART, "\r\n", 2);
        len = read_until_prompt(s_rx, sizeof(s_rx), per_try);
        if (memmem(s_rx, len, PROMPT, sizeof(PROMPT) - 1)) {
            *got = len;
            vTaskDelay(pdMS_TO_TICKS(50));   // let the second prompt (CR and LF can count as two lines) land...
            uart_flush_input(RADAR_UART);    // ...and throw it away
            return true;
        }
    }
    *got = len;
    return false;
}

esp_err_t awr1843_init(void)
{
    if (cli_lock == NULL) {   // survives a retried init without leaking a second mutex
        cli_lock = xSemaphoreCreateMutex();
    }
    ESP_RETURN_ON_FALSE(cli_lock, ESP_ERR_NO_MEM, TAG, "cli_lock");

    const uart_config_t uart_config = {
        .baud_rate  = s_baud,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        // !! use XTAL as UART source_clk, otherwise BSP's CPU/APB frequency switching breaks the baud rate
        .source_clk = UART_SCLK_XTAL,
    };
    ESP_RETURN_ON_ERROR(uart_param_config(RADAR_UART, &uart_config), TAG, "uart_param_config");

    ESP_RETURN_ON_ERROR(uart_set_pin(RADAR_UART, BSP_UART_TX, BSP_UART_RX,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "uart_set_pin");
    ESP_RETURN_ON_ERROR(bsp_power_gpio_init(), TAG, "bsp_power_gpio_init");   // after uart_set_pin: it resets the TX sleep pull

    // RX buffer holds ~4 max-size frames so a slow reader doesn't drop bytes; TX 0 = writes block until sent
    ESP_RETURN_ON_ERROR(uart_driver_install(RADAR_UART, 4 * AWR1843_MAX_FRAME_LEN, 0, 0, NULL, 0),
                        TAG, "uart_driver_install");
    return ESP_OK;
}

esp_err_t awr1843_connect(void)
{
    ESP_RETURN_ON_FALSE(cli_lock, ESP_ERR_INVALID_STATE, TAG, "awr1843_init first");

    size_t len = 0;
    esp_err_t err = ESP_ERR_NOT_FOUND;

    xSemaphoreTake(cli_lock, portMAX_DELAY);
    bsp_power_no_sleep_acquire();

    if (cli_probe(pdMS_TO_TICKS(500), 3, &len)) {
        ESP_LOGI(TAG, "CLI answers at %d baud", s_baud);
        err = ESP_OK;
        goto out;
    }

    // --- no answer: say *why*, because the fix depends on it ---
    if (len == 0) {
        ESP_LOGE(TAG, "no bytes at all from the radar. Check, in this order: 1) switches S1.1 ON, S1.2 OFF, S2.3 OFF, then press SW2; "
                      "2) J7 pin 1 (radar TX) -> GPIO%d and pin 2 (radar RX) -> GPIO%d, try swapping them; "
                      "3) common GND and J7 pin 3 tied to GND; 4) radar powered and booted",
                 (int)BSP_UART_RX, (int)BSP_UART_TX);
    } else {
        ESP_LOGE(TAG, "got %u bytes but no '%s' prompt: wrong baud, floating line or the BT chip driving the pins. First bytes:",
                 (unsigned)len, PROMPT);
        ESP_LOG_BUFFER_HEXDUMP(TAG, s_rx, len < 48 ? len : 48, ESP_LOG_ERROR);
    }

    // does it answer at another speed?
    static const int alt_bauds[] = {921600, 115200};
    for (size_t i = 0; i < sizeof(alt_bauds) / sizeof(alt_bauds[0]); i++) {
        uart_set_baudrate(RADAR_UART, alt_bauds[i]);
        if (!cli_probe(pdMS_TO_TICKS(300), 2, &len)) {
            continue;
        }
        if (alt_bauds[i] == 115200) {
            ESP_LOGE(TAG, "radar answers at 115200: it still runs the STOCK firmware (or the flash did not take). "
                          "Frames would go to the other UART. Reflash SINGLEUART (see j7_cable_bringup.md)");
            err = ESP_ERR_INVALID_RESPONSE;
        } else {
            ESP_LOGW(TAG, "radar answers at %d but not %d: using %d", alt_bauds[i], s_baud, alt_bauds[i]);
            s_baud = alt_bauds[i];
            err = ESP_OK;
        }
        break;
    }
    if (err != ESP_OK) {
        uart_set_baudrate(RADAR_UART, s_baud);
    }

out:
    bsp_power_no_sleep_release();
    xSemaphoreGive(cli_lock);
    return err;
}

esp_err_t awr1843_config(void)
{
    const size_t n = sizeof(RADAR_CFG) / sizeof(RADAR_CFG[0]);
    esp_err_t err;

    xSemaphoreTake(cli_lock, portMAX_DELAY);

    err = send_cmd("sensorStop");
    if (err != ESP_OK) {
        goto out;
    }
    enabled = false;

    err = send_cmd("flushCfg");
    for (size_t i = 0; i < n && err == ESP_OK; i++) {
        err = send_cmd(RADAR_CFG[i]);
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Applied %u configs", (unsigned)n);
    }

out:
    xSemaphoreGive(cli_lock);
    return err;
}

esp_err_t awr1843_enable(void)
{
    esp_err_t err = ESP_OK;

    xSemaphoreTake(cli_lock, portMAX_DELAY);
    if (!enabled) {
        // set before sending: the radar may have started even if the reply was lost
        enabled = true;
        // !! check whether plain "sensorStart" after "sensorStop" reapplies the config; SDK 3.x has "sensorStart 0" to skip that
        err = send_cmd_ex("sensorStart", START_TIMEOUT, 1);
    }
    xSemaphoreGive(cli_lock);
    return err;
}

esp_err_t awr1843_read(uint8_t *buf, size_t cap, size_t *len, TickType_t timeout)
{
    if (buf == NULL || len == NULL || cap < HEADER_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    *len = 0;

    TimeOut_t start;
    vTaskSetTimeOutState(&start);
    esp_err_t err = ESP_ERR_TIMEOUT;

    // !! commands and frames share one UART: hold the lock so send_cmd can't steal frame bytes (or vice versa)
    xSemaphoreTake(cli_lock, portMAX_DELAY);
    if (!enabled) {
        err = ESP_ERR_INVALID_STATE;
        goto out;
    }

    // sync: skip bytes until the magic word (its 8 bytes are all different, so a mismatch can only restart at MAGIC[0])
    size_t matched = 0;
    while (matched < sizeof(MAGIC)) {
        uint8_t b;
        if (xTaskCheckForTimeOut(&start, &timeout) || uart_read_bytes(RADAR_UART, &b, 1, timeout) != 1) {
            ESP_LOGD(TAG, "No frame within timeout");
            goto out;
        }
        matched = (b == MAGIC[matched]) ? matched + 1 : (b == MAGIC[0]);
    }
    memcpy(buf, MAGIC, sizeof(MAGIC));

    // rest of the header
    const size_t hrest = HEADER_LEN - sizeof(MAGIC);
    if (xTaskCheckForTimeOut(&start, &timeout) ||
        uart_read_bytes(RADAR_UART, &buf[sizeof(MAGIC)], hrest, timeout) != (int)hrest) {
        ESP_LOGW(TAG, "Timed out mid-header");
        goto out;
    }

    // totalPacketLen, little-endian uint32
    const uint8_t *p = &buf[TOTAL_LEN_OFF];
    const size_t total = p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
    if (total < HEADER_LEN || total > cap) {
        ESP_LOGW(TAG, "Invalid frame length %u, resyncing", (unsigned)total);
        err = ESP_ERR_INVALID_SIZE;
        goto out;
    }

    // body
    const size_t body = total - HEADER_LEN;
    if (body > 0 && (xTaskCheckForTimeOut(&start, &timeout) ||
                     uart_read_bytes(RADAR_UART, &buf[HEADER_LEN], body, timeout) != (int)body)) {
        ESP_LOGW(TAG, "Timed out mid-frame");
        goto out;
    }

    *len = total;
    err = ESP_OK;

out:
    xSemaphoreGive(cli_lock);
    return err;
}

esp_err_t awr1843_disable(void)
{
    esp_err_t err = ESP_OK;

    xSemaphoreTake(cli_lock, portMAX_DELAY);
    if (enabled) {
        err = send_cmd("sensorStop");
        if (err == ESP_OK) {
            enabled = false;
        }
    }
    xSemaphoreGive(cli_lock);
    return err;
}

void awr1843_deinit(void)
{
    if (cli_lock == NULL) {
        return;   // init never ran
    }

    awr1843_disable();
    enabled = false;

    uart_driver_delete(RADAR_UART);
    vSemaphoreDelete(cli_lock);
    cli_lock = NULL;
}
