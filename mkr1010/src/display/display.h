#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

typedef enum { STATUS_NONE, STATUS_GREEN, STATUS_YELLOW, STATUS_RED } Status;
typedef enum { DIR_UP, DIR_DOWN } Direction;

// Colors (0xRRGGBB), used internally by the driver
#define SM_RED    0xC1292E
#define SM_YELLOW 0xEA9010
#define SM_GREEN  0x08A045
#define SM_WHITE  0xFDFFFC


void sm_begin(uint8_t brightness);
void sm_show(void);
void sm_clear(void);

void sm_status(Status s);                    // left icon
void sm_value(float v);                      // 0.0-9.9 -> "d,d", 10 or more -> "10", NAN clears
void sm_arrow(Direction d, Status c);        // right arrow: green, yellow or red
void sm_arrow_clear(void);                   // blanks the right arrow

#endif