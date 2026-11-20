// Text-command front end for the display driver, over native USB Serial.

#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "display.h"

#define LINE_MAX 32

static char line[LINE_MAX + 1];
static uint8_t lineLen = 0;
static bool lineTooLong = false;

static void printHelp() {
    Serial.println("commands:");
    Serial.println("  s none|warn|green|yellow");
    Serial.println("  v <number>           (3.4 or 3,4)");
    Serial.println("  a up|down [RRGGBB]   (0x or # prefix ok)");
    Serial.println("  c                    clear");
    Serial.println("  h or ?               help");
}

static void fail(const char *reason) {
    Serial.print("err: ");
    Serial.println(reason);
}

// Shows the frame and confirms. Only called after all input has been validated.
static void done() {
    sm_show();
    Serial.println("ok");
}

// Accepts RRGGBB with an optional 0x or # prefix.
static bool parseColor(char *s, uint32_t *out) {
    if (s[0] == '#') s++;
    else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (strlen(s) != 6) return false;
    for (uint8_t i = 0; i < 6; i++)
        if (!isxdigit((uint8_t)s[i])) return false;
    *out = strtoul(s, NULL, 16);
    return true;
}

static void handle(char *text) {
    char *cmd = strtok(text, " ");
    if (!cmd) return;

    if (strcmp(cmd, "?") == 0 || strcasecmp(cmd, "h") == 0) {
        printHelp();
        return;
    }

    if (strcasecmp(cmd, "s") == 0) {
        char *arg = strtok(NULL, " ");
        if (!arg) return fail("missing status");
        if (strtok(NULL, " ")) return fail("too many args");
        Status s;
        if (strcasecmp(arg, "none") == 0)        s = STATUS_NONE;
        else if (strcasecmp(arg, "warn") == 0)   s = STATUS_WARNING;
        else if (strcasecmp(arg, "green") == 0)  s = STATUS_SMILEY_GREEN;
        else if (strcasecmp(arg, "yellow") == 0) s = STATUS_SMILEY_YELLOW;
        else return fail("bad status");
        sm_status(s);
        return done();
    }

    if (strcasecmp(cmd, "v") == 0) {
        char *arg = strtok(NULL, " ");
        if (!arg) return fail("missing number");
        if (strtok(NULL, " ")) return fail("too many args");
        for (char *p = arg; *p; p++)
            if (*p == ',') *p = '.';
        char *end;
        double v = strtod(arg, &end);
        float f = (float)v;
        if (end == arg || *end != '\0' || !isfinite(f)) return fail("bad number");
        sm_value(f);
        return done();
    }

    if (strcasecmp(cmd, "a") == 0) {
        char *dirArg = strtok(NULL, " ");
        if (!dirArg) return fail("missing direction");
        Direction d;
        if (strcasecmp(dirArg, "up") == 0)        d = DIR_UP;
        else if (strcasecmp(dirArg, "down") == 0) d = DIR_DOWN;
        else return fail("bad direction");
        uint32_t color = SM_WHITE;
        char *colArg = strtok(NULL, " ");
        if (colArg && !parseColor(colArg, &color)) return fail("bad color");
        if (strtok(NULL, " ")) return fail("too many args");
        sm_arrow(d, color);
        return done();
    }

    if (strcasecmp(cmd, "c") == 0) {
        if (strtok(NULL, " ")) return fail("too many args");
        sm_clear();
        return done();
    }

    fail("unknown command");
}

// Collects one line; a line longer than LINE_MAX is dropped and reported when it ends.
static void feed(char c) {
    if (c == '\r' || c == '\n') {
        if (lineTooLong) {
            fail("too long");
        } else if (lineLen > 0) {
            line[lineLen] = '\0';
            handle(line);
        }
        lineLen = 0;
        lineTooLong = false;
        return;
    }
    if (lineTooLong) return;
    if (lineLen < LINE_MAX) line[lineLen++] = c;
    else lineTooLong = true;
}

void setup() {
    Serial.begin(115200);       // ignored by native USB, but harmless
    sm_begin(8, 10);            // data pin, brightness
    sm_clear();
    sm_show();
    printHelp();
}

void loop() {
    while (Serial.available() > 0)
        feed((char)Serial.read());
}
