#ifndef APP_SERVER_WEB_AUTH_H
#define APP_SERVER_WEB_AUTH_H

#include <Arduino.h>

// Fixed admin username for the device's web configuration UI.
#define WEB_AUTH_USER "admin"

// Per-device admin password for the web UI. Derived from the chip's eFuse
// MAC (low 24 bits -> 6 uppercase hex) so every unit has a distinct,
// reboot-stable default with no shared hard-coded secret. Shown on the
// WebServer screen so the user can read it off the device while connecting.
String web_auth_password();

// HTTP Basic auth gate. Returns true when the request carried valid
// credentials; otherwise issues a 401 challenge and returns false. Call at
// the top of a protected handler — or register routes through auth_on() in
// server.cpp, which wraps this around every handler.
bool web_require_auth();

#endif // APP_SERVER_WEB_AUTH_H
