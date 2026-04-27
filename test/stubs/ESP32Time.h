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
    String getTime(const char * = "%H:%M:%S") { return String("00:00:00"); }
    struct tm getTimeStruct() {
        time_t t = 0;
        return *localtime(&t);
    }
};

#endif
