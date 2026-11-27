// the Adafruit_NeoPixel library is C++ so the libary is two
#include "display.h"
#include <Adafruit_NeoPixel.h>

#define W 32
#define H 8

#define RED    SM_RED
#define GREEN  SM_GREEN
#define YELLOW SM_YELLOW
#define WHITE  SM_WHITE

// Section positions
#define STATUS_X 0   // 9 wide
#define VALUE_X  9   // 18 wide
#define ARROW_X  27  // 5 wide

// All bitmaps: one uint16_t per row, leftmost pixel = highest used bit.
static const uint16_t WARNING[8]       = {0x010,0x028,0x054,0x054,0x082,0x092,0x101,0x1FF};
static const uint16_t SMILEY_GREEN[8]  = {0x07C,0x082,0x129,0x101,0x145,0x139,0x082,0x07C};
static const uint16_t SMILEY_YELLOW[8] = {0x07C,0x082,0x129,0x101,0x17D,0x101,0x082,0x07C};
static const uint16_t ARROW_UP[8]      = {0x04,0x0E,0x15,0x04,0x04,0x04,0x04,0x04};
static const uint16_t ARROW_DOWN[8]    = {0x04,0x04,0x04,0x04,0x04,0x15,0x0E,0x04};
static const uint16_t COMMA[8]         = {0,0,0,0,0,0,0x01,0x02};
static const uint16_t DIGITS[10][8] = {
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // 0
    {0x04,0x0C,0x14,0x04,0x04,0x04,0x04,0x1F}, // 1
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x10,0x1F}, // 2
    {0x0E,0x11,0x01,0x06,0x01,0x01,0x11,0x0E}, // 3
    {0x11,0x11,0x11,0x1F,0x01,0x01,0x01,0x01}, // 4
    {0x1F,0x10,0x10,0x1E,0x01,0x01,0x11,0x0E}, // 5
    {0x0E,0x11,0x10,0x1E,0x11,0x11,0x11,0x0E}, // 6
    {0x1F,0x01,0x02,0x02,0x04,0x04,0x08,0x08}, // 7
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x11,0x0E}, // 8
    {0x0E,0x11,0x11,0x11,0x0F,0x01,0x11,0x0E}  // 9
};

static Adafruit_NeoPixel strip(W * H, 4, NEO_GRB + NEO_KHZ800);

// Column-major zigzag, LED 0 bottom-left: even columns run bottom-to-top, odd top-to-bottom. y=0 is the top row.
static uint16_t xy(uint8_t x, uint8_t y) {
    return x * H + ((x & 1) ? y : (H - 1 - y));
}

static void clearArea(uint8_t x0, uint8_t w) {
    for (uint8_t y = 0; y < H; y++)
        for (uint8_t x = x0; x < x0 + w && x < W; x++)
            strip.setPixelColor(xy(x, y), 0);
}

static void draw(uint8_t x0, uint8_t w, const uint16_t *rows, uint32_t color) {
    for (uint8_t y = 0; y < H; y++)
        for (uint8_t x = 0; x < w; x++)
            if (rows[y] & (1 << (w - 1 - x)))
                strip.setPixelColor(xy(x0 + x, y), color);
}

void sm_begin(uint8_t dataPin, uint8_t brightness) {
    strip.setPin(dataPin);
    strip.begin();
    strip.setBrightness(brightness);
    strip.clear();
    strip.show();
}

void sm_show(void)  { strip.show(); }
void sm_clear(void) { strip.clear(); }
// Adafruit rescales the pixel buffer in place, so dimming is lossy and 0 wipes the frame.
void sm_brightness(uint8_t b) { strip.setBrightness(b); }

void sm_status(Status s) {
    clearArea(STATUS_X, 9);
    if (s == STATUS_RED)    draw(STATUS_X, 9, WARNING, RED);
    if (s == STATUS_GREEN)  draw(STATUS_X, 9, SMILEY_GREEN, GREEN);
    if (s == STATUS_YELLOW) draw(STATUS_X, 9, SMILEY_YELLOW, YELLOW);
}

void sm_value(float v) {
    clearArea(VALUE_X, 18);
    if (isnan(v)) return;
    if (v < 0) v = 0;

    if (v >= 9.95f) {                       // 10 (or more): "10"
        draw(VALUE_X + 3, 5, DIGITS[1], WHITE);
        draw(VALUE_X + 9, 5, DIGITS[0], WHITE);
        return;
    }

    uint8_t t = (uint8_t)(v * 10.0f + 0.5f);  // tenths, 0..99
    draw(VALUE_X + 2,  5, DIGITS[t / 10], WHITE);
    draw(VALUE_X + 8,  2, COMMA,          WHITE);
    draw(VALUE_X + 11, 5, DIGITS[t % 10], WHITE);
}

void sm_arrow_clear(void) { clearArea(ARROW_X, 5); }

void sm_arrow(Direction d, Status c) {
    sm_arrow_clear();
    if (c == STATUS_NONE) return;
    uint32_t color = c == STATUS_GREEN ? GREEN : c == STATUS_YELLOW ? YELLOW : RED;
    draw(ARROW_X, 5, d == DIR_UP ? ARROW_UP : ARROW_DOWN, color);
}