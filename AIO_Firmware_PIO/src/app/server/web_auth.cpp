#include "web_auth.h"
#include "server.h" // extern WebServer server

String web_auth_password()
{
    // Low 24 bits of the eFuse MAC -> 6 hex chars. Distinct per device,
    // immutable across reboots, short enough to type from the screen.
    uint64_t mac = ESP.getEfuseMac();
    char buf[7];
    snprintf(buf, sizeof(buf), "%06X", (uint32_t)(mac & 0xFFFFFFUL));
    return String(buf);
}

bool web_require_auth()
{
    if (server.authenticate(WEB_AUTH_USER, web_auth_password().c_str()))
    {
        return true;
    }
    // Sends a 401 with a WWW-Authenticate: Basic header so the browser
    // prompts for credentials (and resends them on every later request).
    server.requestAuthentication();
    return false;
}
