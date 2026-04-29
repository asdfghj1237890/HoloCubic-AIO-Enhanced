#ifndef AIO_FTP_STUB_WIFICLIENT_H
#define AIO_FTP_STUB_WIFICLIENT_H

#include "Arduino.h"
#include "WiFi.h"
#include <deque>
#include <memory>

// Bidirectional buffered WiFiClient stub for FTP harness tests.
//
// Two queues per client:
//   rx  — bytes the FTP server reads (test injects via inject_rx())
//   tx  — bytes the FTP server writes (test inspects via take_tx())
//
// The client is value-typed in FtpServer (`WiFiClient client;` is a
// member, and `client = ftpServer.available();` reassigns it). To keep
// state alive across the assignment, the actual buffers live in a
// shared_ptr-managed Backing struct so the assignment-target client
// keeps pointing at the same queues the test injected into.
//
// Usage (test side):
//   WiFiClient c = wifi_make_scripted_client();
//   c.inject_rx("USER alice\r\n");
//   ftp_test_server.handleFTP();
//   std::string out = c.take_tx_as_string();
//   ASSERT_CONTAINS(out, "331");

class WiFiClient : public Print {
public:
    struct Backing {
        std::deque<uint8_t> rx;
        std::deque<uint8_t> tx;
        bool connected = false;
    };

    WiFiClient() : b_(std::make_shared<Backing>()) {}
    explicit WiFiClient(std::shared_ptr<Backing> b) : b_(b) {}

    // --- FtpServer-facing API ---
    int connected() { return b_->connected ? 1 : 0; }
    operator bool() { return b_->connected; }
    void stop() {
        b_->connected = false;
        b_->rx.clear();
        b_->tx.clear();
    }
    int available() { return (int)b_->rx.size(); }
    int read() {
        if (b_->rx.empty()) return -1;
        uint8_t c = b_->rx.front();
        b_->rx.pop_front();
        return c;
    }
    int read(uint8_t *dst, size_t n) {
        size_t take = n < b_->rx.size() ? n : b_->rx.size();
        for (size_t i = 0; i < take; i++) {
            dst[i] = b_->rx.front();
            b_->rx.pop_front();
        }
        return (int)take;
    }
    size_t write(uint8_t c) override {
        b_->tx.push_back(c);
        return 1;
    }
    size_t write(const uint8_t *buf, size_t n) override {
        for (size_t i = 0; i < n; i++) b_->tx.push_back(buf[i]);
        return n;
    }
    void flush() {}
    void setNoDelay(bool) {}
    int connect(const char *, uint16_t) { return 0; }
    int connect(IPAddress, uint16_t) { return 0; }

    // --- Test-facing API ---
    void mark_connected(bool v = true) { b_->connected = v; }
    void inject_rx(const char *s) {
        if (!s) return;
        for (; *s; s++) b_->rx.push_back((uint8_t)*s);
    }
    std::string take_tx_as_string() {
        std::string out(b_->tx.begin(), b_->tx.end());
        b_->tx.clear();
        return out;
    }
    size_t tx_size() const { return b_->tx.size(); }

private:
    std::shared_ptr<Backing> b_;
};

// WiFiServer scripted by ftp_test_helpers — push_pending_client() to
// queue a client returned by the next available()/hasClient() pair.
class WiFiServer {
public:
    WiFiServer(uint16_t = 21) {}
    void begin() {}
    void begin(uint16_t) {}
    void end() {}
    void close() {}
    void stop() {}
    void setNoDelay(bool) {}

    // FtpServer pattern: if (hasClient()) { client = available(); }
    bool hasClient() { return !pending_.empty(); }
    WiFiClient available() {
        if (pending_.empty()) return WiFiClient();
        WiFiClient c = pending_.front();
        pending_.pop_front();
        return c;
    }

    // Test-side: queue a client to be returned by the next available().
    void push_pending_client(const WiFiClient &c) { pending_.push_back(c); }

private:
    std::deque<WiFiClient> pending_;
};

#endif
