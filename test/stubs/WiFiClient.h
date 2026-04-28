#ifndef AIO_STUB_WIFICLIENT_H
#define AIO_STUB_WIFICLIENT_H
#include "Arduino.h"
#include "WiFi.h"  // for IPAddress (file_manager's ESP32FtpServer.h needs it
                   // through this header).

// Stubbed WiFiClient. connect(host,...) is implemented out-of-line in
// stubs_runtime.cpp: it looks up a canned response under
// test/fixtures/socket/<host>.txt (like an HTTP-style reply with a
// blank line separating headers from body). find()/readStringUntil()/
// read() then walk that buffer in place of a real socket exchange.
//
// Apps without a fixture get connect()=0 (the original "no socket"
// failure), so anything that hasn't grown a fixture stays on its old
// path.
//
// Used by pc_resource (raw GET /sse against cfg_data.pc_ipaddr).
// screen_share constructs WiFiClient via WiFiServer.available()
// rather than connect(), so it isn't affected by this fixture.
class WiFiClient {
public:
    WiFiClient() : m_pos(0), m_connected(false) {}

    int connect(const char *host, uint16_t port);
    int connect(const char *host, uint16_t port, int /*timeout_ms*/);
    int connect(uint32_t /*ip*/, uint16_t /*port*/) { return 0; }
    void stop() { m_buf = String(""); m_pos = 0; m_connected = false; }
    int connected() { return m_connected ? 1 : 0; }

    int available() { return (int)(m_buf.length() - m_pos); }
    int read() {
        if (m_pos >= m_buf.length()) return -1;
        return (unsigned char)m_buf.s[m_pos++];
    }
    int read(uint8_t *dst, size_t n) {
        size_t left = m_buf.length() - m_pos;
        size_t take = n < left ? n : left;
        if (take == 0 || !dst) return 0;
        memcpy(dst, m_buf.s.data() + m_pos, take);
        m_pos += take;
        return (int)take;
    }
    size_t readBytes(uint8_t *dst, size_t n) { return (size_t)read(dst, n); }
    size_t readBytes(char *dst, size_t n) { return (size_t)read((uint8_t *)dst, n); }

    bool find(const char *needle);
    String readStringUntil(char terminator);
    String readString();

    // Outbound side: tests don't assert on what was written, so just
    // count bytes for the printf-style helpers.
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
    void flush() {}
    void setTimeout(uint32_t) {}
    operator bool() { return m_connected; }

private:
    String m_buf;
    size_t m_pos;
    bool   m_connected;
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
