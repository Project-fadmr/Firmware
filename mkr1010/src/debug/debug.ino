// Text-command front end for the display driver, over native USB Serial.

#include <math.h>
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
    Serial.println("  s none|green|yellow|red");
    Serial.println("  v <number>|none                (3.4 or 3,4; none blanks)");
    Serial.println("  a up|down [green|yellow|red]   (default green)");
    Serial.println("  a none                        blank the arrow");
    Serial.println("  c                              clear");
    Serial.println("  h or ?                         help");
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

// Maps a name (case-insensitive) to a Status. Shared by the s and a commands.
static bool parseStatus(const char *s, Status *out) {
    if (strcasecmp(s, "none") == 0)        *out = STATUS_NONE;
    else if (strcasecmp(s, "green") == 0)  *out = STATUS_GREEN;
    else if (strcasecmp(s, "yellow") == 0) *out = STATUS_YELLOW;
    else if (strcasecmp(s, "red") == 0)    *out = STATUS_RED;
    else return false;
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
        if (!parseStatus(arg, &s)) return fail("bad status");
        sm_status(s);
        return done();
    }

    if (strcasecmp(cmd, "v") == 0) {
        char *arg = strtok(NULL, " ");
        if (!arg) return fail("missing number");
        if (strtok(NULL, " ")) return fail("too many args");
        if (strcasecmp(arg, "none") == 0) {
            sm_value(NAN);
            return done();
        }
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
        if (strcasecmp(dirArg, "none") == 0) {
            if (strtok(NULL, " ")) return fail("too many args");
            sm_arrow_clear();
            return done();
        }
        Direction d;
        if (strcasecmp(dirArg, "up") == 0)        d = DIR_UP;
        else if (strcasecmp(dirArg, "down") == 0) d = DIR_DOWN;
        else return fail("bad direction");
        Status c = STATUS_GREEN;
        char *colArg = strtok(NULL, " ");
        if (colArg && (!parseStatus(colArg, &c) || c == STATUS_NONE)) return fail("bad color");
        if (strtok(NULL, " ")) return fail("too many args");
        sm_arrow(d, c);
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
