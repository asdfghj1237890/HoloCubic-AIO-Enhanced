#ifndef AIO_FTP_TEST_HELPERS_H
#define AIO_FTP_TEST_HELPERS_H

#include "Arduino.h"
#include "WiFiClient.h"
#include "SD.h"
#include "app/file_manager/ESP32FtpServer.h"

// FtpServer constructs two TU-level WiFiServer globals; the test reaches
// them by `extern` so it can push a pending client before each handleFTP().
extern WiFiServer ftpServer;
extern WiFiServer dataServer;

// Drives the FTP server through enough handleFTP() iterations to absorb
// all currently queued bytes from `client`. The server's state machine
// processes one byte per readChar() invocation and one stage transition
// per handleFTP() call (auth → command → command → ...), so we loop
// until either the rx queue drains or we hit a sane upper bound.
void ftp_pump(FtpServer &srv, WiFiClient &client, int max_iters = 200);

// Connect a fresh client and run through the welcome → USER → PASS
// → 230 OK handshake. Returns the connected, authenticated client so
// tests can layer further commands on top.
WiFiClient ftp_connect_and_auth(FtpServer &srv,
                                const char *user = "user",
                                const char *pass = "pass");

// Convenience: send a single command line and pump.
std::string ftp_send_command(FtpServer &srv, WiFiClient &client, const char *line);

// Convenience: substring search on collected tx bytes.
bool ftp_tx_contains(const std::string &tx, const char *needle);

#endif
