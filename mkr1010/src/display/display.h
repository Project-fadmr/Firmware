#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

typedef enum { STATUS_NONE, STATUS_WARNING, STATUS_SMILEY_GREEN, STATUS_SMILEY_YELLOW } Status;
typedef enum { DIR_UP, DIR_DOWN } Direction;

// Colors (0xRRGGBB) - use these or any color of your own
#define SM_RED    0xC1292E
#define SM_YELLOW 0xEA9010
#define SM_GREEN  0x08A045
#define SM_WHITE  0xFDFFFC


void sm_begin(uint8_t dataPin, uint8_t brightness);
void sm_show(void);
void sm_clear(void);

void sm_status(Status s);                    // left icon
void sm_value(float v);                      // 0.0-9.9 -> "d,d", 10 or more -> "10"
void sm_arrow(Direction d, uint32_t color);  // right arrow, any color

#endif