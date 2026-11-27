#ifndef ESPNOW_DRV_H
#define ESPNOW_DRV_H

#include <stdint.h>
#include "espnow_proto.h"

typedef struct {
  uint32_t rx;
  uint32_t bad;
} espnow_drv_stats_t;

void espnow_drv_init(void);
// out->data is valid until the next poll or hold (sm_show() holds).
bool espnow_drv_poll(ep_frame_t *out);
void espnow_drv_hold(void);
void espnow_drv_release(void);
// Serial1 overflow is undetectable on this core; byte loss shows up as bad.
void espnow_drv_stats(espnow_drv_stats_t *s);

#endif
