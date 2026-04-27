#ifndef AIO_STUB_WEBSERVER_H
#define AIO_STUB_WEBSERVER_H
#include "Arduino.h"
#include "WiFi.h"
#include <functional>

// Minimal subset of the ESP32 WebServer API. Pulled in transitively by
// firmware network.h via apps that include "../../common.h".

#define HTTP_GET 1
#define HTTP_POST 2
#define HTTP_PUT 3
#define HTTP_DELETE 4

class WebServer {
public:
    WebServer(int = 80) {}
    void begin() {}
    void begin(uint16_t) {}
    void close() {}
    void stop() {}
    void handleClient() {}

    typedef std::function<void(void)> THandlerFunction;
    void on(const String &, THandlerFunction) {}
    void on(const String &, int, THandlerFunction) {}
    void on(const String &, int, THandlerFunction, THandlerFunction) {}
    void onNotFound(THandlerFunction) {}
    void onFileUpload(THandlerFunction) {}

    void send(int = 200, const char * = "text/plain", const String & = "") {}
    void send(int, const String &, const String &) {}
    void sendHeader(const String &, const String &, bool = false) {}

    int args() { return 0; }
    String arg(const String &) { return String(""); }
    String arg(int) { return String(""); }
    String argName(int) { return String(""); }
    bool hasArg(const String &) { return false; }

    String uri() { return String("/"); }
    int method() { return HTTP_GET; }
    String header(const String &) { return String(""); }

    void setContentLength(size_t) {}
    void sendContent(const String &) {}
};

#endif
