#ifndef AIO_STUB_ESP32TIME_H
#define AIO_STUB_ESP32TIME_H
#include "Arduino.h"
#include <time.h>

// Tomato includes <ESP32Time.h> but never actually calls into the library;
// other firmware files (e.g. weather) do, so a thin stub is enough to keep
// scoped builds linking. Methods below match the ESP32Time public surface
// loosely so that any app that does call them gets sensible defaults
// rather than link errors.

class ESP32Time {
public:
    ESP32Time(int = 0) {}
    void setTime(unsigned long) {}
    void setTime(unsigned long, int) {}
    unsigned long getEpoch() { return 0; }
    int getYear() { return 1970; }
    int getMonth() { return 0; }
    int getDay() { return 1; }
    int getHour(bool = true) { return 0; }
    int getMinute() { return 0; }
    int getSecond() { return 0; }
    int getDayofWeek() { return 4; } // 1970-01-01 was a Thursday
    // strftime against the unix epoch (1970-01-01 00:00:00 UTC) so harness
    // output mirrors what the real ESP32Time library does when no NTP sync
    // has happened yet — keeps the host-rendered goldens semantically
    // identical to the cold-boot device render, regardless of host TZ.
    static String _strftime_epoch(const char *fmt) {
        char buf[64];
        time_t t = 0;
        struct tm *tm0 = gmtime(&t);
        strftime(buf, sizeof(buf), fmt, tm0);
        return String(buf);
    }
    String getTime(const char *fmt = "%H:%M:%S") { return _strftime_epoch(fmt); }
    String getTime(const String &fmt) { return _strftime_epoch(fmt.c_str()); }
    String getDate(const char *fmt = "%Y-%m-%d") { return _strftime_epoch(fmt); }
    String getDate(const String &fmt) { return _strftime_epoch(fmt.c_str()); }
    String getDateTime(const char *fmt = "%Y-%m-%d %H:%M:%S") { return _strftime_epoch(fmt); }
    String getDateTime(const String &fmt) { return _strftime_epoch(fmt.c_str()); }
    struct tm getTimeStruct() {
        time_t t = 0;
        return *localtime(&t);
    }
};

#endif
