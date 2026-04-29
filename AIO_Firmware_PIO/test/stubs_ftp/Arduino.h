#ifndef AIO_FTP_STUB_ARDUINO_H
#define AIO_FTP_STUB_ARDUINO_H

// Arduino-flavour shims for the FTP unit-test build (env:native_ftp).
// Heavier than stubs_unit/Arduino.h because ESP32FtpServer pulls in
// String, IPAddress, File, Stream-style print/println methods. Kept
// separate from stubs_unit so the lean unit-test surface stays small.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <cstdarg>

typedef bool boolean;
typedef uint8_t byte;

#define F(s) (s)
#define PROGMEM

// Arduino String — just a thin wrapper over std::string. Only the
// surface FtpServer touches: c_str(), length(), substring(), indexOf(),
// concat via operator+/+=, ctor from int/long for response codes.
class String {
public:
    std::string s;
    String() {}
    String(const char *c) : s(c ? c : "") {}
    String(const std::string &x) : s(x) {}
    String(int v) { char b[32]; snprintf(b, 32, "%d", v); s = b; }
    String(long v) { char b[32]; snprintf(b, 32, "%ld", v); s = b; }
    String(unsigned int v) { char b[32]; snprintf(b, 32, "%u", v); s = b; }
    String(unsigned long v) { char b[32]; snprintf(b, 32, "%lu", v); s = b; }
    String(char c) { s = std::string(1, c); }
    const char *c_str() const { return s.c_str(); }
    size_t length() const { return s.length(); }
    String substring(size_t from) const { return String(s.substr(from)); }
    String substring(size_t from, size_t to) const { return String(s.substr(from, to - from)); }
    int indexOf(char c) const { auto p = s.find(c); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(const char *needle) const { auto p = s.find(needle); return p == std::string::npos ? -1 : (int)p; }
    String &operator+=(const String &o) { s += o.s; return *this; }
    String &operator+=(const char *c) { s += (c ? c : ""); return *this; }
    String &operator+=(char c) { s += c; return *this; }
    String operator+(const String &o) const { String r(*this); r += o; return r; }
    String operator+(const char *c) const { String r(*this); r += c; return r; }
    bool operator==(const String &o) const { return s == o.s; }
    bool operator==(const char *c) const { return s == (c ? c : ""); }
    bool operator!=(const char *c) const { return !(*this == c); }
    char operator[](size_t i) const { return s[i]; }
    void replace(char from, char to) { for (auto &c : s) if (c == from) c = to; }
    void trim() {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) s.erase(s.begin());
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    }
};

inline String operator+(const char *c, const String &o) { String r(c); r += o; return r; }

// Print/Stream surface used by client.print/println(int|String|"literal").
class Print {
public:
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buf, size_t n) {
        size_t total = 0;
        for (size_t i = 0; i < n; i++) total += write(buf[i]);
        return total;
    }
    size_t write(const char *s) { return s ? write((const uint8_t *)s, strlen(s)) : 0; }
    size_t print(const char *s) { return write(s); }
    size_t print(const String &s) { return write(s.c_str()); }
    size_t print(int v) { char b[32]; int n = snprintf(b, 32, "%d", v); return write(b); }
    size_t println() { return write("\r\n"); }
    size_t println(const char *s) { size_t n = print(s); return n + println(); }
    size_t println(const String &s) { size_t n = print(s); return n + println(); }
    size_t println(int v) { size_t n = print(v); return n + println(); }
    void printf(const char *fmt, ...) {
        char buf[256];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        write(buf);
    }
};

// HardwareSerial: route to a sink so FTP_DEBUG output is captured but
// not printed to the test runner stdout (keeps Unity output clean).
class HardwareSerial : public Print {
public:
    void begin(unsigned long) {}
    size_t write(uint8_t) override { return 1; }
};
extern HardwareSerial Serial;

// Time + delay shims. The FTP server uses millis() for timeouts.
// Test harness calls advance_millis() to step time deterministically.
extern unsigned long g_fake_millis;
inline unsigned long millis() { return g_fake_millis; }
inline void delay(unsigned long ms) { g_fake_millis += ms; }
inline void advance_millis(unsigned long ms) { g_fake_millis += ms; }

#endif
