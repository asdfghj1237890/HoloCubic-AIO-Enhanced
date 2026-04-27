#ifndef AIO_STUB_HTTPCLIENT_H
#define AIO_STUB_HTTPCLIENT_H
#include "Arduino.h"
#include "WiFi.h"
#include "WiFiClient.h"

#define HTTP_CODE_OK 200
#define HTTP_CODE_NOT_FOUND 404

class HTTPClient {
public:
    void begin(const String &) {}
    void begin(const char *) {}
    void begin(WiFiClient &, const String &) {}
    void begin(WiFiClient &, const char *) {}
    int GET() { return -1; }
    int POST(const String &) { return -1; }
    int POST(const uint8_t *, size_t) { return -1; }
    String getString() { return String(""); }
    void end() {}
    void setTimeout(uint32_t) {}
    void setReuse(bool) {}
    void setUserAgent(const String &) {}
    void addHeader(const String &, const String &, bool = false, bool = true) {}
    String errorToString(int) { return String("stub_error"); }
};

#endif
