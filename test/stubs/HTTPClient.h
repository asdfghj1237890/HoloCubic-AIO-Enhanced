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

// Stubbed HTTPClient. begin() records the URL; GET() looks up a canned
// response under test/fixtures/http/<host>/<path>.json (mirroring the
// URL structure). If the fixture file is missing, GET() returns -1 —
// the same "no response" sentinel the previous always-offline stub
// used, so apps without fixtures continue to behave exactly as before.
//
// GET() and getString() are out-of-line so the implementation can do
// host filesystem IO from stubs_runtime.cpp without dragging <fstream>
// into every TU that includes this header.
class HTTPClient {
public:
    bool begin(const String &url) { m_url = url; return true; }
    bool begin(const char *url)   { m_url = url ? url : ""; return true; }
    bool begin(WiFiClient &, const String &url) { m_url = url; return true; }
    bool begin(WiFiClient &, const char *url)   { m_url = url ? url : ""; return true; }

    int GET();
    String getString() { return m_payload; }

    int POST(const String &) { return -1; }
    int POST(const uint8_t *, size_t) { return -1; }
    void end() {}
    void setTimeout(uint32_t) {}
    void setReuse(bool) {}
    void setUserAgent(const String &) {}
    void addHeader(const String &, const String &, bool = false, bool = true) {}
    String errorToString(int) { return String("stub_error"); }

    // Test hook: lets a scenario assert which URL was last requested
    // without parsing serial logs.
    String lastUrl() const { return m_url; }

private:
    String m_url;
    String m_payload;
};

#endif
