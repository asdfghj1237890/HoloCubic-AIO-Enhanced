#ifndef AIO_STUB_WEBSERVER_H
#define AIO_STUB_WEBSERVER_H
#include "Arduino.h"
#include "WiFi.h"
#include "WiFiClient.h"
#include "FS.h"
#include <functional>

// Minimal subset of the ESP32 WebServer API. Pulled in transitively by
// firmware network.h via apps that include "../../common.h".

#define HTTP_GET 1
#define HTTP_POST 2
#define HTTP_PUT 3
#define HTTP_DELETE 4

#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)
#define CONTENT_LENGTH_NOT_SET ((size_t)-2)

enum HTTPUploadStatus {
    UPLOAD_FILE_START,
    UPLOAD_FILE_WRITE,
    UPLOAD_FILE_END,
    UPLOAD_FILE_ABORTED
};

// HTTPUpload mirrors the ESP32 WebServer's per-chunk upload struct.
// In the harness, the upload handler is never invoked because the
// WebServer never receives a real client, so the fields stay zeroed.
struct HTTPUpload {
    HTTPUploadStatus status = UPLOAD_FILE_END;
    String filename;
    String name;
    String type;
    size_t totalSize = 0;
    size_t currentSize = 0;
    uint8_t buf[2] = {0, 0};
};

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

    // Used by web_setting.cpp's upload + download handlers. None of these
    // run in the harness (no real client connects), so the stubs stay
    // inert — but the symbols must exist for the firmware code to link.
    WiFiClient &client() { static WiFiClient c; return c; }
    HTTPUpload &upload() { static HTTPUpload u; return u; }
    size_t streamFile(File &, const String &) { return 0; }
    size_t streamFile(File &, const char *) { return 0; }
};

#endif
