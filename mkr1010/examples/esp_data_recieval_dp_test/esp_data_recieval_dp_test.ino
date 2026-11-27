#include <Adafruit_NeoPixel.h>

#define W 32
#define H 8
#define IDLE_MS 20
#define ON_COLOR 0x00FF00  // green only: one channel keeps worst-case current inside the 2 A supply

static Adafruit_NeoPixel strip(W * H, 8, NEO_GRB + NEO_KHZ800);

// Column-major zigzag, LED 0 bottom-left: even columns run bottom-to-top, odd top-to-bottom. y=0 is the top row.
static uint16_t xy(uint8_t x, uint8_t y) {
  return x * H + ((x & 1) ? y : (H - 1 - y));
}

static uint16_t n = 0;         // bit index within the current frame
static uint32_t lastMs = 0;    // time the last byte arrived
static bool frameOpen = false;

void setup() {
  pinMode(NINA_GPIO0, OUTPUT);
  pinMode(NINA_RESETN, OUTPUT);
  digitalWrite(NINA_GPIO0, HIGH);
  digitalWrite(NINA_RESETN, HIGH);
  delay(100);
  digitalWrite(NINA_RESETN, LOW);
  Serial1.begin(115200);

  strip.begin();
  strip.setBrightness(75);
  strip.clear();
  strip.show();
}

void loop() {
  // show() disables interrupts for several ms, so it only runs after the frame goes idle; mid-burst it would drop UART bytes.
  uint32_t now = millis();
  if (frameOpen && now - lastMs >= IDLE_MS) {
    strip.show();
    frameOpen = false;
  }

  while (Serial1.available()) {
    uint8_t b = Serial1.read();  // always drained, even when discarded
    if (!frameOpen) {
      strip.clear();
      n = 0;
      frameOpen = true;
    }
    if (n < W * H) {  // bytes past the first 32 are read and dropped
      for (uint8_t bit = 0; bit < 8; bit++) {
        if (b & (0x80 >> bit)) {
          uint16_t i = n + bit;
          strip.setPixelColor(xy(i % W, i / W), ON_COLOR);
        }
      }
      n += 8;
    }
    lastMs = now;
  }
}
