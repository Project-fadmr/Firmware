
#include "display.h"
 
void setup() {
    sm_begin(8, 22);             // data pin, brightness
 
    sm_status(STATUS_SMILEY_GREEN);
    sm_value(1.3);                // shows 1,3   (10 shows "10")
    sm_arrow(DIR_UP, SM_GREEN);   // SM_RED, SM_YELLOW, SM_GREEN or any 0xRRGGBB
    sm_show();
}
 
void loop() {
}
 
