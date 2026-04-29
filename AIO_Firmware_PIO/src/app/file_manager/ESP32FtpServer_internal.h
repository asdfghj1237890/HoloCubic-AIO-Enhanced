// Internal shared header for the ESP32FtpServer split (PR-3.3).
// Holds the TU-level globals (ftpServer / dataServer WiFiServers) and
// the formerly-static path helpers that the auth / commands / transfer
// / util .cpp files all need to reach.
//
// Pre-split, get_file_basename + get_file_cwd were file-static helpers
// in ESP32FtpServer.cpp. processCommand uses them; once processCommand
// moves into _commands.cpp, the helpers must lose `static` to be
// callable across TUs. They're not part of the public class surface
// so they live here, not in ESP32FtpServer.h.

#ifndef AIO_ESP32_FTPSERVER_INTERNAL_H
#define AIO_ESP32_FTPSERVER_INTERNAL_H

#include <Arduino.h>
#include <WiFiClient.h>
#include "ESP32FtpServer.h"

// Globals constructed in ESP32FtpServer.cpp; reached by handleFTP and
// (post-split) by dataConnect in ESP32FtpServer_transfer.cpp.
extern WiFiServer ftpServer;
extern WiFiServer dataServer;

// Path helpers — moved from file-static into util.cpp, exposed here.
const char *get_file_basename(const char *path);
String get_file_cwd(const char *path);

#endif
