#ifndef AIO_STUB_ARDUINO_H
#define AIO_STUB_ARDUINO_H

// On real ESP32 these surface ambiently via Arduino-ESP32; on host they
// need to be visible to apps that don't include them explicitly (e.g.
// screen_share calling heap_caps_malloc without an explicit include).
#include "esp_heap_caps.h"

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <string>
#include <chrono>
#include <thread>

typedef uint8_t byte;
typedef bool boolean;

#ifndef HIGH
#define HIGH 1
#define LOW  0
#endif

#define F(x) (x)
#define PROGMEM
#define PGM_P const char *
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))

template <typename T>
static inline T constrain(T x, T low, T high) {
    return (x < low) ? low : ((x > high) ? high : x);
}

template <typename T>
static inline T abs_aio(T x) { return x < 0 ? -x : x; }

static inline uint32_t aio_millis_now() {
    static auto epoch = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - epoch).count();
}

static inline uint32_t millis() { return aio_millis_now(); }
static inline uint32_t micros() { return aio_millis_now() * 1000; }

static inline void delay(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
static inline void delayMicroseconds(uint32_t us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

class String {
public:
    std::string s;
    String() {}
    String(const char *cs) : s(cs ? cs : "") {}
    String(const std::string &x) : s(x) {}
    String(int v) { char b[32]; snprintf(b, 32, "%d", v); s = b; }
    String(unsigned int v) { char b[32]; snprintf(b, 32, "%u", v); s = b; }
    String(long v) { char b[32]; snprintf(b, 32, "%ld", v); s = b; }
    String(unsigned long v) { char b[32]; snprintf(b, 32, "%lu", v); s = b; }
    String(double v, int dec = 2) { char b[64]; snprintf(b, 64, "%.*f", dec, v); s = b; }

    const char *c_str() const { return s.c_str(); }
    size_t length() const { return s.size(); }
    int indexOf(const char *needle) const { auto p = s.find(needle); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(const char *needle, int from) const { auto p = s.find(needle, from); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(char c) const { auto p = s.find(c); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(char c, int from) const { auto p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
    String substring(int from) const { return String(s.substr(from)); }
    String substring(int from, int to) const { return String(s.substr(from, to - from)); }
    String &operator+=(const String &o) { s += o.s; return *this; }
    String &operator+=(const char *o) { s += (o ? o : ""); return *this; }
    String &operator+=(char c) { s += c; return *this; }
    String &operator=(const char *o) { s = (o ? o : ""); return *this; }
    bool operator==(const String &o) const { return s == o.s; }
    bool operator==(const char *o) const { return s == (o ? o : ""); }
    bool operator!=(const char *o) const { return !(*this == o); }
    char operator[](int i) const { return s[i]; }
    // Arduino's concat returns a bool/unsigned char (success flag).
    // ArduinoJson v6's Writer specialisation casts the return to bool.
    unsigned char concat(const String &o) { s += o.s; return 1; }
    unsigned char concat(const char *o) { s += (o ? o : ""); return 1; }
    void trim() {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) s.clear(); else s = s.substr(a, b - a + 1);
    }
    int toInt() const { return atoi(s.c_str()); }
    float toFloat() const { return (float)atof(s.c_str()); }
    double toDouble() const { return atof(s.c_str()); }
    bool startsWith(const char *p) const {
        if (!p) return true;
        return s.compare(0, strlen(p), p) == 0;
    }
    bool startsWith(const String &o) const { return startsWith(o.c_str()); }
    bool endsWith(const char *p) const {
        if (!p) return true;
        size_t n = strlen(p);
        return s.size() >= n && s.compare(s.size() - n, n, p) == 0;
    }
    char charAt(int i) const { return (i < 0 || (size_t)i >= s.size()) ? 0 : s[i]; }
    int lastIndexOf(char c) const {
        auto p = s.find_last_of(c);
        return p == std::string::npos ? -1 : (int)p;
    }
};

inline String operator+(const String &a, const String &b) { String r(a); r += b; return r; }
inline String operator+(const String &a, const char *b) { String r(a); r += b; return r; }
inline String operator+(const char *a, const String &b) { String r(a); r += b; return r; }
inline String operator+(const String &a, int v) { String r(a); r += String(v); return r; }
inline String operator+(const String &a, unsigned int v) { String r(a); r += String(v); return r; }
inline String operator+(const String &a, long v) { String r(a); r += String(v); return r; }
inline String operator+(const String &a, unsigned long v) { String r(a); r += String(v); return r; }
inline String operator+(const String &a, char c) { String r(a); r += c; return r; }

// Arduino's StringSumHelper: concrete subclass of String returned by
// operator+ overloads on the device. ArduinoJson v6's
// ArduinoStringAdapter has a specialisation for it via is_base_of, so
// it must be visible at global scope — even if we never instantiate it.
class StringSumHelper : public String {
public:
    StringSumHelper(const String &s) : String(s) {}
    StringSumHelper(const char *s) : String(s) {}
    StringSumHelper(char c) : String(std::string(1, c)) {}
};

class HardwareSerial {
public:
    void begin(unsigned long) {}
    void println() { puts(""); }
    void println(const char *s) { puts(s ? s : ""); }
    void println(const String &s) { puts(s.c_str()); }
    void println(int v) { printf("%d\n", v); }
    void println(unsigned int v) { printf("%u\n", v); }
    void println(long v) { printf("%ld\n", v); }
    void println(unsigned long v) { printf("%lu\n", v); }
    void println(double v) { printf("%f\n", v); }
    void println(double v, int dec) { printf("%.*f\n", dec, v); }
    void print(const char *s) { fputs(s ? s : "", stdout); }
    void print(double v, int dec) { printf("%.*f", dec, v); }
    void print(const String &s) { fputs(s.c_str(), stdout); }
    void print(int v) { printf("%d", v); }
    void print(unsigned int v) { printf("%u", v); }
    void print(long v) { printf("%ld", v); }
    void print(unsigned long v) { printf("%lu", v); }
    void print(double v) { printf("%f", v); }
    int printf(const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int r = vprintf(fmt, ap);
        va_end(ap);
        return r;
    }
    int write(uint8_t) { return 1; }
    size_t write(const uint8_t *, size_t n) { return n; }
    size_t write(const char *s, size_t n) { return n; }
    int available() { return 0; }
    int read() { return -1; }
    int read(uint8_t *, size_t) { return 0; }
};

extern HardwareSerial Serial;

// ESP global — used by network code, heartbeat for client_id, etc.
class EspClass {
public:
    uint64_t getEfuseMac() { return 0xAABBCCDDEEFFULL; }
    uint32_t getChipId() { return 0xAABBCC; }
    void restart() {}
    void deepSleep(uint64_t) {}
    uint32_t getFreeHeap() { return 0; }
    uint32_t getMinFreeHeap() { return 0; }
};
extern EspClass ESP;

static inline void setCpuFrequencyMhz(uint32_t) {}
static inline uint32_t getCpuFrequencyMhz() { return 240; }
static inline uint32_t getXtalFrequencyMhz() { return 40; }
static inline void yield() {}

static inline void pinMode(int, int) {}
static inline void digitalWrite(int, int) {}
static inline int digitalRead(int) { return 0; }
static inline int analogRead(int) { return 0; }
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

// Arduino analog pin aliases (game_snake uses A0).
#define A0 0
#define A1 1
#define A2 2
#define A3 3

// Arduino's `map` value-range converter. Used by LHLXW's animations.
static inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    if (in_max == in_min) return out_min;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Arduino-style min/max as inline templates (NOT macros — those collide
// with std::vector internals). stockmarket calls bare `min(a, b)` /
// `max(a, b)`. Argument-dependent lookup in global namespace finds these.
template <typename T> static inline T min(T a, T b) { return a < b ? a : b; }
template <typename T> static inline T max(T a, T b) { return a > b ? a : b; }

// Arduino-style RNG mapped onto stdlib for deterministic CI output:
// every harness run starts from the same state because analogRead(25)
// is stubbed to 0, so randomSeed(0) feeds srand(0).
static inline void randomSeed(unsigned long seed) { srand((unsigned)seed); }
static inline long random(long max_excl) {
    return max_excl <= 0 ? 0 : (long)(rand() % max_excl);
}
static inline long random(long min_incl, long max_excl) {
    return max_excl <= min_incl ? min_incl : min_incl + random(max_excl - min_incl);
}

#endif
