#include <bsp.h>
#include <display.h>
#include <espnow_drv.h>
#include <radar_parser.h>
#include <math.h>

#define BRIGHTNESS 22
#define MOUNT_DEG 15.0f
#define RED_M 0.5f
#define YELLOW_M 1.5f

static radar_parser_t parser;
static bool pending;
static bool pending_target;
static float pending_lat;

static void on_radar_frame(const radar_frame_t *frame, void *) {
  radar_stats_t mean;
  float lat, lon;
  pending_target = radar_calc_mean(frame, &mean) == 0;
  if (pending_target) {
    radar_to_vehicle(&mean, MOUNT_DEG, &lat, &lon);
    pending_lat = fabsf(lat);
    pending_target = !isnan(lat);  // corrupt floats would otherwise render green
  }
  pending = true;
}

// Processing plugs in here. f->data is only valid during this call.
// Keep it under ~4 ms, or wrap heavy work in espnow_drv_hold()/release().
static void on_frame(const ep_frame_t *f) {
  radar_feed(&parser, f->data, f->len);
  if (!pending) return;
  pending = false;
  // Rendered here, not in on_radar_frame: sm_show() holds the ESP-NOW buffer radar_feed was reading.
  if (!pending_target) {
    sm_clear();
    sm_show();
    return;
  }
  Status s = pending_lat < RED_M ? STATUS_RED
           : pending_lat <= YELLOW_M ? STATUS_YELLOW
           : STATUS_GREEN;
  sm_status(s);
  sm_value(pending_lat);
  sm_arrow(DIR_UP, s);
  sm_show();
}

static void frame_job(void *) {
  ep_frame_t f;
  while (espnow_drv_poll(&f)) on_frame(&f);
}

void setup() {
  Serial.begin(115200);
  bsp_init();
  sm_begin(BRIGHTNESS);
  radar_init(&parser, NULL, on_radar_frame, NULL);
  espnow_drv_init();
  bsp_sched_add(frame_job, nullptr, 1);
}

void loop() { bsp_loop(); }
