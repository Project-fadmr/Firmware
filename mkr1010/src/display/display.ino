
#include "display.h"
 
void setup() {
    sm_begin(22);                 // brightness
 
    sm_status(STATUS_GREEN);
    sm_value(1.3);                // shows 1,3   (10 shows "10")
    sm_arrow(DIR_UP, STATUS_GREEN); // STATUS_GREEN, STATUS_YELLOW or STATUS_RED; sm_arrow_clear() blanks it
    sm_show();
}
 
void loop() {
}
 
