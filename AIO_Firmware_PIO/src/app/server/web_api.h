// JSON API endpoints for the Glass web settings UI's dynamic panels:
//   GET /api/stats       device telemetry for the dashboard hero
//   GET /api/wifi-scan   WiFi.scanNetworks() snapshot for the system page
//   GET /api/settings    current sys_cfg / rgb_cfg / mpu_cfg as JSON
//
// HTML pages stay in web_setting{,_forms,_handlers}.cpp; this TU is
// JSON-only so the Glass UI can refresh in-place without re-rendering
// the whole page.
//
// `/api/settings` is the read side of the B15 fix — replaces the broken
// serial SettingMsg protocol. Studio's Settings tab calls this to populate
// fields, then POSTs changes to the existing /save<Cat>Conf form handlers.

#ifndef WEB_API_H
#define WEB_API_H

void api_stats(void);
void api_wifi_scan(void);
void api_settings(void);

#endif
