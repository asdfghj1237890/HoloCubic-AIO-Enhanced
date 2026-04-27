#ifndef AIO_STUB_WIFIMULTI_H
#define AIO_STUB_WIFIMULTI_H
#include "WiFi.h"

// Minimal subset of the ESP32 WiFiMulti API. Pulled in transitively by
// firmware network.h when an app uses a relative include for common.h
// (bypassing the test/stubs version).

class WiFiMulti {
public:
    bool addAP(const char *, const char * = nullptr) { return false; }
    int run(uint32_t = 5000) { return WL_DISCONNECTED; }
};

#endif
