#ifndef AIO_STUB_DNSSERVER_H
#define AIO_STUB_DNSSERVER_H
#include "Arduino.h"
#include "WiFi.h"

// Minimal DNSServer shim. server's captive-portal flow uses this; the
// harness never actually serves DNS.

class DNSServer {
public:
    bool start(uint16_t = 53, const String & = "*", const IPAddress & = IPAddress(0,0,0,0)) { return true; }
    void stop() {}
    void processNextRequest() {}
    void setTTL(uint32_t) {}
    void setErrorReplyCode(int) {}
};

#endif
