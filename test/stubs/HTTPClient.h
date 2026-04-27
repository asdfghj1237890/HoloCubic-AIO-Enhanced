#ifndef AIO_STUB_HTTPCLIENT_H
#define AIO_STUB_HTTPCLIENT_H
#include "Arduino.h"
#include "WiFi.h"
#include "WiFiClient.h"

#define HTTP_CODE_OK 200
#define HTTP_CODE_MOVED_PERMANENTLY 301
#define HTTP_CODE_FOUND 302
#define HTTP_CODE_NOT_FOUND 404
#define HTTP_CODE_INTERNAL_SERVER_ERROR 500

class HTTPClient {
public:
    bool begin(const String &) { return true; }
    bool begin(const char *) { return true; }
    bool begin(WiFiClient &, const String &) { return true; }
    bool begin(WiFiClient &, const char *) { return true; }
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
