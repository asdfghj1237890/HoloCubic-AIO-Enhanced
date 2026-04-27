#ifndef AIO_STUB_WIFICLIENT_H
#define AIO_STUB_WIFICLIENT_H
#include "Arduino.h"
#include "WiFi.h"  // for IPAddress (file_manager's ESP32FtpServer.h needs it
                   // through this header).

class WiFiClient {
public:
    int connect(const char *, uint16_t) { return 0; }
    int connect(const char *, uint16_t, int /*timeout_ms*/) { return 0; }
    int connect(uint32_t, uint16_t) { return 0; }
    void stop() {}
    int connected() { return 0; }
    int available() { return 0; }
    int read() { return -1; }
    int read(uint8_t *, size_t) { return 0; }
    size_t readBytes(uint8_t *, size_t) { return 0; }
    size_t readBytes(char *, size_t) { return 0; }
    size_t write(uint8_t) { return 1; }
    size_t write(const uint8_t *, size_t n) { return n; }
    size_t write(const char *s) { return s ? strlen(s) : 0; }
    size_t write(const char *, size_t n) { return n; }
    size_t print(const char *s) { return s ? strlen(s) : 0; }
    size_t print(const String &s) { return s.length(); }
    size_t print(int) { return 1; }
    size_t println(const char *s) { return s ? strlen(s) + 1 : 1; }
    size_t println(const String &s) { return s.length() + 1; }
    size_t println(int) { return 2; }
    size_t println() { return 1; }
    bool find(const char *) { return false; }
    String readStringUntil(char) { return String(""); }
    String readString() { return String(""); }
    void flush() {}
    void setTimeout(uint32_t) {}
    operator bool() { return false; }
};

class WiFiServer {
public:
    WiFiServer(uint16_t = 80) {}
    void begin() {}
    void begin(uint16_t) {}
    void end() {}
    void close() {}
    void stop() {}
    WiFiClient available() { return WiFiClient(); }
    WiFiClient accept() { return WiFiClient(); }
    void setNoDelay(bool) {}
    bool hasClient() { return false; }
};

#endif
