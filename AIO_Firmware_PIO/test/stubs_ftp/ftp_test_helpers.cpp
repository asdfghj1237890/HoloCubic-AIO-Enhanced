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
    // Walk the server through cmdStatus 0 → 1 → 2 (waiting-for-connect)
    // BEFORE pushing a client. Pushing earlier means the cmdStatus 0
    // branch (`if (client.connected()) disconnectClient();`) calls
    // client.stop() on our queues, wiping the test's injected data
    // before clientConnected() ever runs.
    srv.handleFTP();  // 0 → 1
    srv.handleFTP();  // 1 → 2

    WiFiClient c;
    c.mark_connected(true);
    ftpServer.push_pending_client(c);

    // Now we're at cmdStatus 2; this tick picks up the client AND
    // calls clientConnected() (welcome 220) AND sets cmdStatus = 3.
    srv.handleFTP();
    (void)c.take_tx_as_string();  // discard 220 banner

    // USER
    char line[64];
    snprintf(line, sizeof(line), "USER %s\r\n", user);
    c.inject_rx(line);
    ftp_pump(srv, c, 50);
    (void)c.take_tx_as_string();  // discard 331 response

    // PASS
    snprintf(line, sizeof(line), "PASS %s\r\n", pass);
    c.inject_rx(line);
    ftp_pump(srv, c, 50);
    (void)c.take_tx_as_string();  // discard 230 response

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
