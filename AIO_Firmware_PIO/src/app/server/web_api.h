// JSON API endpoints for the Glass web settings UI's dynamic panels:
//   GET /api/stats       device telemetry for the dashboard hero
//   GET /api/wifi-scan   WiFi.scanNetworks() snapshot for the system page
//
// HTML pages stay in web_setting{,_forms,_handlers}.cpp; this TU is
// JSON-only so the Glass UI can refresh in-place without re-rendering
// the whole page.

#ifndef WEB_API_H
#define WEB_API_H

void api_stats(void);
void api_wifi_scan(void);

#endif
