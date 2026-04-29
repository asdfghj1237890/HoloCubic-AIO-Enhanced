#ifndef AIO_FTP_STUB_WIFI_H
#define AIO_FTP_STUB_WIFI_H
#include "Arduino.h"

// IPAddress shim. FtpServer's `dataIp` field uses it; tests don't
// actually compare IPs across the boundary so the body can be trivial.
class IPAddress {
public:
    uint8_t a[4] = {0, 0, 0, 0};
    IPAddress() {}
    IPAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) { a[0] = b0; a[1] = b1; a[2] = b2; a[3] = b3; }
    String toString() const {
        char b[32];
        snprintf(b, 32, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
        return String(b);
    }
    uint8_t operator[](int i) const { return a[i & 3]; }
    uint8_t &operator[](int i) { return a[i & 3]; }
};

// Minimal WiFi singleton. FtpServer only touches WiFi.localIP() to
// fill in dataIp on PASV mode; we return a fixed AP address.
class WiFiClass {
public:
    IPAddress localIP() { return IPAddress(192, 168, 4, 1); }
};
extern WiFiClass WiFi;

#endif
