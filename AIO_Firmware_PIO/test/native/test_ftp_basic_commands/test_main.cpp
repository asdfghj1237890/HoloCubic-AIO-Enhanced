// Smoke-level Unity tests for ESP32FtpServer (env:native_ftp).
//
// Purpose: give the upcoming PR-3.3 file split a safety net. The plan
// gates the split on this harness — the goal isn't exhaustive FTP-RFC
// coverage, it's exercising enough call paths that a mechanical
// split which breaks linkage / state-machine ordering / response
// formatting will fail at least one assertion.
//
// Coverage target: the four functional clusters the split will produce
//   - auth  (userIdentity, userPassword)         → tested via ftp_connect_and_auth
//   - commands (processCommand switch)           → PWD, FEAT, NOOP, QUIT
//   - util  (path/string helpers via PWD)        → 257 + "/" in response
//   - error path (unknown command)               → "XYZZY" → 500 Unknow
//
// Not covered (intentionally): data-transfer (RETR/STOR) — needs PASV
// data-port wiring + a second WiFiServer queue. If PR-3.3 ends up
// touching the transfer cluster meaningfully, add tests then.

#include <unity.h>
#include "Arduino.h"
#include "WiFiClient.h"
#include "SD.h"
#include "app/file_manager/ESP32FtpServer.h"
#include "ftp_test_helpers.h"

static FtpServer *srv = nullptr;

void setUp()
{
    g_fake_millis = 0;
    SD.clear();
    srv = new FtpServer();
    srv->begin("user", "pass");
}

void tearDown()
{
    delete srv;
    srv = nullptr;
    // Drain any leftover pending clients between tests.
    while (ftpServer.hasClient()) (void)ftpServer.available();
}

void test_auth_flow_succeeds()
{
    // Walk through cmdStatus 0 → 1 → 2 BEFORE pushing the client (see
    // ftp_test_helpers.cpp for the rationale — pushing at cmdStatus 0
    // gets the client immediately disconnected).
    srv->handleFTP();
    srv->handleFTP();

    WiFiClient c;
    c.mark_connected(true);
    ftpServer.push_pending_client(c);

    srv->handleFTP();  // picks up client + emits 220 welcome
    std::string welcome = c.take_tx_as_string();
    TEST_ASSERT_TRUE_MESSAGE(ftp_tx_contains(welcome, "220"),
                             "expected 220 welcome banner");

    c.inject_rx("USER user\r\n");
    ftp_pump(*srv, c, 50);
    std::string user_resp = c.take_tx_as_string();
    TEST_ASSERT_TRUE_MESSAGE(ftp_tx_contains(user_resp, "331"),
                             "expected 331 after USER");

    c.inject_rx("PASS pass\r\n");
    ftp_pump(*srv, c, 50);
    std::string pass_resp = c.take_tx_as_string();
    TEST_ASSERT_TRUE_MESSAGE(ftp_tx_contains(pass_resp, "230"),
                             "expected 230 after PASS");
}

void test_unknown_command_returns_500()
{
    WiFiClient c = ftp_connect_and_auth(*srv);
    std::string resp = ftp_send_command(*srv, c, "XYZZY foo\r\n");
    TEST_ASSERT_TRUE_MESSAGE(ftp_tx_contains(resp, "500"),
                             "expected 500 for unknown command");
}

void test_pwd_echoes_root_after_login()
{
    WiFiClient c = ftp_connect_and_auth(*srv);
    std::string resp = ftp_send_command(*srv, c, "PWD\r\n");
    TEST_ASSERT_TRUE_MESSAGE(ftp_tx_contains(resp, "257"),
                             "expected 257 from PWD");
    TEST_ASSERT_TRUE_MESSAGE(ftp_tx_contains(resp, "/"),
                             "expected current dir / in PWD response");
}

void test_feat_lists_extensions()
{
    // SYST isn't implemented in this FtpServer — falls through to the
    // unknown-command branch. FEAT is the next-best command in the
    // server-info cluster: it returns 211-/211 with " MLSD" between.
    WiFiClient c = ftp_connect_and_auth(*srv);
    std::string resp = ftp_send_command(*srv, c, "FEAT\r\n");
    TEST_ASSERT_TRUE_MESSAGE(ftp_tx_contains(resp, "211"),
                             "expected 211 from FEAT");
    TEST_ASSERT_TRUE_MESSAGE(ftp_tx_contains(resp, "MLSD"),
                             "expected MLSD listed in FEAT response");
}

void test_noop_returns_200()
{
    WiFiClient c = ftp_connect_and_auth(*srv);
    std::string resp = ftp_send_command(*srv, c, "NOOP\r\n");
    TEST_ASSERT_TRUE_MESSAGE(ftp_tx_contains(resp, "200"),
                             "expected 200 from NOOP");
}

void test_quit_disconnects_client()
{
    WiFiClient c = ftp_connect_and_auth(*srv);
    std::string resp = ftp_send_command(*srv, c, "QUIT\r\n");
    TEST_ASSERT_TRUE_MESSAGE(ftp_tx_contains(resp, "221"),
                             "expected 221 from QUIT");
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_auth_flow_succeeds);
    RUN_TEST(test_unknown_command_returns_500);
    RUN_TEST(test_pwd_echoes_root_after_login);
    RUN_TEST(test_feat_lists_extensions);
    RUN_TEST(test_noop_returns_200);
    RUN_TEST(test_quit_disconnects_client);
    return UNITY_END();
}
