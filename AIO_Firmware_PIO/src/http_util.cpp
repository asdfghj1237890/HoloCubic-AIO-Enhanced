#include "http_util.h"
#include <HTTPClient.h>
#include <WiFi.h>

namespace {

// AccuWeather + Sina + several other call sites accept either the
// canonical 200 OK or a 301 Moved Permanently response (the latter is
// what AccuWeather hands back on apikey query-string variants). Match
// the existing call sites' is_ok check.
inline bool is_ok_code(int code)
{
    return code == HTTP_CODE_OK || code == HTTP_CODE_MOVED_PERMANENTLY;
}

}  // namespace

int http_fetch_string(const char *url, String &out, uint32_t timeout_ms,
                      const char *extra_header_name,
                      const char *extra_header_value)
{
    out = "";
    if (NULL == url || url[0] == '\0') {
        return -1;
    }
    if (WL_CONNECTED != WiFi.status()) {
        return -1;
    }

    HTTPClient http;
    http.setConnectTimeout(timeout_ms);
    http.setTimeout(timeout_ms);
    http.setReuse(false);
    if (!http.begin(url)) {
        return -1;
    }

    if (extra_header_name && extra_header_value) {
        http.addHeader(extra_header_name, extra_header_value);
    }

    int code = http.GET();
    if (code > 0 && is_ok_code(code)) {
        out = http.getString();
    }
    http.end();
    return code;
}

bool http_fetch_json(const char *url, JsonDocument &doc,
                     uint32_t timeout_ms, int *http_code_out,
                     const char *extra_header_name,
                     const char *extra_header_value)
{
    String body;
    int code = http_fetch_string(url, body, timeout_ms,
                                 extra_header_name, extra_header_value);
    if (http_code_out) {
        *http_code_out = code;
    }
    if (code <= 0 || !is_ok_code(code)) {
        return false;
    }
    DeserializationError err = deserializeJson(doc, body);
    return !err;
}
