// Shared HTTP fetch helpers for the firmware's six HTTP-using apps
// (weather, bilibili_fans, anniversary, stockmarket, settings,
// and any future addition). Wraps the boilerplate
//   HTTPClient http;
//   http.setTimeout(N);
//   http.begin(url);
//   int code = http.GET();
//   if (code == HTTP_CODE_OK) String body = http.getString();
//   http.end();
// into one call so each app stops re-implementing the
// network-error / status-check / cleanup chain (and stops drifting on
// the details — PR-1.5 / PR-1.6 had to chase parser bugs that all came
// from "this app's slightly different copy of the same fetch flow").
//
// Introduced in Phase 2 PR-2.1 with no callers; the per-app
// migrations (PR-2.2a/b, PR-2.3a/b, PR-2.4) move each call site over.

#ifndef AIO_HTTP_UTIL_H
#define AIO_HTTP_UTIL_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Fetch `url` as a raw string. Returns the HTTP status code on success,
// or -1 on network failure / HTTPClient::begin failure. The response
// body is written to `out` only when the returned code is in the 2xx
// range (or 301 Moved Permanently, which the existing call sites all
// treat as success). On any other code or on network failure, `out`
// is set to the empty string.
//
// Caller owns `out`'s storage; this function does not pre-allocate.
//
// Defaults match the most common existing call sites: 5000 ms timeout
// is generous enough for the Beijing-hosted Sina / Taobao / AccuWeather
// endpoints under poor WiFi; per-call overrides preserve the tighter
// values used by weather (2000-3000 ms) and settings (1000 ms).
int http_fetch_string(const char *url, String &out, uint32_t timeout_ms = 5000);

// Fetch `url` and deserialize the body into `doc`. Returns true only
// when (a) HTTP code is 2xx (or 301), AND (b) ArduinoJson parsed the
// body without error. Returns false on any other condition. The HTTP
// status code is written to `*http_code_out` when non-null, regardless
// of return value, so callers that need to distinguish network failure
// (-1) from server error (5xx) from parse failure (200 but garbage)
// can do so.
//
// `doc` is caller-owned (DynamicJsonDocument or StaticJsonDocument);
// pick its capacity to fit the expected payload — this helper does not
// re-allocate. On failure, `doc`'s contents are unspecified (caller
// should not read fields without checking the return value).
bool http_fetch_json(const char *url, JsonDocument &doc,
                     uint32_t timeout_ms = 5000,
                     int *http_code_out = nullptr);

#endif
