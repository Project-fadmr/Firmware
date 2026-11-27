#ifndef BSP_H
#define BSP_H

#include <Arduino.h>
#include "bsp_sched.h"

#define BSP_PIN_LED_DATA    8
#define BSP_PIN_NINA_BUSY   NINA_ACK
#define BSP_NINA_BAUD       460800
#define BSP_IDLE_TIMEOUT_MS 5000

void bsp_init(void);
void bsp_nina_on(void);
void bsp_nina_off(void);
void bsp_nina_restart(void);
void bsp_nina_busy(bool on);
void bsp_activity(void);
bool bsp_in_standby(void);
void bsp_loop(void);

#endif
