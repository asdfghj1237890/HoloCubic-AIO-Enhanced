// FtpServer auth (PR-3.3 split).
// Bodies copied byte-for-byte from ESP32FtpServer.cpp. State machine
// stays in handleFTP (in ESP32FtpServer.cpp); this TU only owns the
// USER and PASS reply paths.

#include "ESP32FtpServer_internal.h"

boolean FtpServer::userIdentity()
{
    if (strcmp(command, "USER"))
        client.println("500 Syntax error");
    if (strcmp(parameters, _FTP_USER.c_str()))
        client.println("530 user not found");
    else
    {
        client.println("331 OK. Password required");
        snprintf(cwdName, sizeof(cwdName), "/");
        return true;
    }
    millisDelay = millis() + 100; // delay of 100 ms
    return false;
}

boolean FtpServer::userPassword()
{
    if (strcmp(command, "PASS"))
        client.println("500 Syntax error");
    else if (strcmp(parameters, _FTP_PASS.c_str()))
        client.println("530 ");
    else
    {
#ifdef FTP_DEBUG
        Serial.println("OK. Waiting for commands.");
#endif
        client.println("230 User logged in, proceed.");
        return true;
    }
    millisDelay = millis() + 100; // delay of 100 ms
    return false;
}
