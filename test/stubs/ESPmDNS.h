#ifndef AIO_STUB_ESPMDNS_H
#define AIO_STUB_ESPMDNS_H
#include "Arduino.h"
#include "WiFi.h"

// Minimal subset of the ESP32 mDNS API. Pulled in transitively by
// firmware network.h.

class MDNSResponder {
public:
    bool begin(const char *) { return true; }
    bool begin(const String &) { return true; }
    void end() {}
    void addService(const char *, const char *, uint16_t) {}
    void addService(const String &, const String &, uint16_t) {}
    int queryService(const char *, const char *) { return 0; }
    String hostname(int) { return String(""); }
    IPAddress IP(int) { return IPAddress(0,0,0,0); }
    uint16_t port(int) { return 0; }
};

extern MDNSResponder MDNS;

#endif
