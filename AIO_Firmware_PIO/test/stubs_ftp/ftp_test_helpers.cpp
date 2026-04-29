#include "ftp_test_helpers.h"
#include <string.h>

void ftp_pump(FtpServer &srv, WiFiClient &client, int max_iters)
{
    for (int i = 0; i < max_iters; i++) {
        size_t before = client.available();
        srv.handleFTP();
        // Stop once we've fully drained the input AND given the server
        // an extra tick to emit its response.
        if (client.available() == 0 && before == 0) {
            // One more tick to capture any deferred response, then bail.
            srv.handleFTP();
            return;
        }
    }
}

WiFiClient ftp_connect_and_auth(FtpServer &srv, const char *user, const char *pass)
{
    WiFiClient c;
    c.mark_connected(true);
    ftpServer.push_pending_client(c);

    // Two pumps to clear cmdStatus 0→1→2→3 (idle wait → register client → wait for USER).
    srv.handleFTP();  // accepts pending client
    srv.handleFTP();  // moves into idle / waiting-for-id

    // Welcome banner (220) is in tx; clear it.
    (void)c.take_tx_as_string();

    // USER
    char line[64];
    snprintf(line, sizeof(line), "USER %s\r\n", user);
    c.inject_rx(line);
    ftp_pump(srv, c, 50);
    (void)c.take_tx_as_string();  // clear "331 OK" response

    // PASS
    snprintf(line, sizeof(line), "PASS %s\r\n", pass);
    c.inject_rx(line);
    ftp_pump(srv, c, 50);
    (void)c.take_tx_as_string();  // clear "230 logged in" response

    return c;
}

std::string ftp_send_command(FtpServer &srv, WiFiClient &client, const char *line)
{
    client.inject_rx(line);
    ftp_pump(srv, client, 50);
    return client.take_tx_as_string();
}

bool ftp_tx_contains(const std::string &tx, const char *needle)
{
    return needle && tx.find(needle) != std::string::npos;
}
