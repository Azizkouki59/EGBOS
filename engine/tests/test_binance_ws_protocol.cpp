// engine/tests/test_binance_ws_protocol.cpp
// EGBOS ENGINE_V3 — Binance WebSocket L0 Protocol Unit Tests (A1–A6, R4)
//
// Classification : DEVELOPMENT_TEST / LOCAL_SYNTHETIC
//                  These tests do NOT constitute LIVE_OPERATIONAL evidence.
//                  They MUST NOT contact Binance or any external exchange.
//                  They do NOT qualify S5-AC-001.

#include <gtest/gtest.h>
#include <signal.h>
#include "engine/l0/binance_feed_listener.h"
#include "engine/l0/ingestor.h"
#include "engine/l0/feed_source.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bn.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <strings.h>
#include <string>
#include <thread>
#include <vector>

namespace egbos::l0 {
namespace {

using clock = std::chrono::steady_clock;

constexpr uint8_t kHelloFrame[] = {0x81, 0x05, 'h', 'e', 'l', 'l', 'o'};
constexpr uint8_t kWorldFrame[] = {0x81, 0x05, 'w', 'o', 'r', 'l', 'd'};

std::string extract_ws_key(const char* req) {
    const char* p = req;
    while (*p) {
        if (::strncasecmp(p, "Sec-WebSocket-Key:", 18) == 0) {
            p += 18;
            while (*p == ' ' || *p == '\t') ++p;
            const char* e = p;
            while (*e && *e != '\r') ++e;
            return std::string(p, static_cast<std::size_t>(e - p));
        }
        const char* nl = std::strstr(p, "\r\n");
        if (!nl) break;
        p = nl + 2;
    }
    return {};
}

std::string ws_accept_from_key(const std::string& key) {
    char combined[64];
    const int n = std::snprintf(combined, sizeof(combined),
                                "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
                                key.c_str());
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined),
         static_cast<std::size_t>(n), digest);
    char out[32]{};
    EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out), digest, SHA_DIGEST_LENGTH);
    return std::string(out);
}

std::string http_switching(const std::string& accept,
                           const char* status = "HTTP/1.1 101 Switching Protocols",
                           bool upgrade = true,
                           bool connection = true) {
    std::string r;
    r += status;
    r += "\r\n";
    if (upgrade) r += "Upgrade: websocket\r\n";
    if (connection) r += "Connection: Upgrade\r\n";
    r += "Sec-WebSocket-Accept: ";
    r += accept;
    r += "\r\n\r\n";
    return r;
}

void put_be64(uint8_t* p, uint64_t v) {
    for (int i = 7; i >= 0; --i) {
        p[i] = static_cast<uint8_t>(v & 0xFFu);
        v >>= 8;
    }
}

bool ssl_write_all(SSL* s, const void* data, int n) {
    const auto* p = static_cast<const uint8_t*>(data);
    int off = 0;
    while (off < n) {
        const int w = SSL_write(s, p + off, n - off);
        if (w <= 0) return false;
        off += w;
    }
    return true;
}

} // namespace

class BinanceWSProtocolTest : public ::testing::Test {
public:
    static EVP_PKEY* pkey_;
    static X509* x509_;

    static void SetUpTestSuite() {
        signal(SIGPIPE, SIG_IGN);
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();

        pkey_ = EVP_PKEY_new();
        BIGNUM* bne = BN_new();
        BN_set_word(bne, RSA_F4);
        RSA* rsa = RSA_new();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        RSA_generate_key_ex(rsa, 2048, bne, nullptr);
#pragma GCC diagnostic pop
        EVP_PKEY_assign_RSA(pkey_, rsa);
        BN_free(bne);

        x509_ = X509_new();
        ASN1_INTEGER_set(X509_get_serialNumber(x509_), 1);
        X509_gmtime_adj(X509_get_notBefore(x509_), 0);
        X509_gmtime_adj(X509_get_notAfter(x509_), 31536000L);
        X509_set_pubkey(x509_, pkey_);
        X509_NAME* name = X509_get_subject_name(x509_);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   reinterpret_cast<unsigned char*>(const_cast<char*>("localhost")),
                                   -1, -1, 0);
        X509_set_issuer_name(x509_, name);
        X509_sign(x509_, pkey_, EVP_sha256());
    }

    static void TearDownTestSuite() {
        X509_free(x509_);
        EVP_PKEY_free(pkey_);
        EVP_cleanup();
    }

    static void disable_verify(BinanceFeedListener& listener) {
        SSL_CTX_set_verify(listener.ssl_ctx_, SSL_VERIFY_NONE, nullptr);
    }

    static void inject_connection(BinanceFeedListener& listener, int fd, SSL* ssl) {
        listener.fd_ = fd;
        listener.ssl_ = ssl;
        listener.state_.store(ListenerState::OPERATIONAL, std::memory_order_relaxed);
    }

    static int recv_frame(BinanceFeedListener& listener) {
        return listener.recv_frame();
    }

    static bool perform_websocket_handshake(BinanceFeedListener& listener) {
        return listener.perform_websocket_handshake();
    }

    static bool perform_tls_connect(BinanceFeedListener& listener) {
        return listener.perform_tls_connect();
    }

    static void set_injected_ssl_read(BinanceFeedListener& listener,
                                      int (*fn)(SSL*, void*, int)) {
        listener.injected_ssl_read_ = fn;
    }

    static SSL* ssl(BinanceFeedListener& listener) { return listener.ssl_; }
    static int fd(const BinanceFeedListener& listener) { return listener.fd_; }

    static std::size_t pending_available(const BinanceFeedListener& listener) {
        if (listener.pending_rx_len_ < listener.pending_rx_offset_) return 0;
        return listener.pending_rx_len_ - listener.pending_rx_offset_;
    }

    static constexpr uint64_t max_drain_size() {
        return BinanceFeedListener::kMaxDrainSize;
    }
    static constexpr std::size_t frame_buf_size() {
        return BinanceFeedListener::kFrameBufSize;
    }
};

EVP_PKEY* BinanceWSProtocolTest::pkey_ = nullptr;
X509* BinanceWSProtocolTest::x509_ = nullptr;

struct SocketPairHarness {
    int fds[2]{-1, -1};
    SSL_CTX* server_ctx = nullptr;
    SSL* server_ssl = nullptr;
    SSL_CTX* client_ctx = nullptr;
    SSL* client_ssl = nullptr;

    SocketPairHarness() {
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) return;

        server_ctx = SSL_CTX_new(TLS_server_method());
        SSL_CTX_set_min_proto_version(server_ctx, TLS1_2_VERSION);
        SSL_CTX_use_certificate(server_ctx, BinanceWSProtocolTest::x509_);
        SSL_CTX_use_PrivateKey(server_ctx, BinanceWSProtocolTest::pkey_);
        server_ssl = SSL_new(server_ctx);
        SSL_set_fd(server_ssl, fds[0]);

        client_ctx = SSL_CTX_new(TLS_client_method());
        SSL_CTX_set_min_proto_version(client_ctx, TLS1_2_VERSION);
        SSL_CTX_set_verify(client_ctx, SSL_VERIFY_NONE, nullptr);
        client_ssl = SSL_new(client_ctx);
        SSL_set_fd(client_ssl, fds[1]);
    }

    ~SocketPairHarness() {
        if (server_ssl) SSL_free(server_ssl);
        if (server_ctx) SSL_CTX_free(server_ctx);
        if (client_ssl) SSL_free(client_ssl);
        if (client_ctx) SSL_CTX_free(client_ctx);
        if (fds[0] >= 0) ::close(fds[0]);
        if (fds[1] >= 0) ::close(fds[1]);
    }

    void steal_client(BinanceFeedListener& listener) {
        BinanceWSProtocolTest::inject_connection(listener, fds[1], client_ssl);
        fds[1] = -1;
        client_ssl = nullptr;
    }

    void do_handshake() {
        std::thread t([this]() { SSL_accept(server_ssl); });
        const int rc = SSL_connect(client_ssl);
        t.join();
        ASSERT_EQ(rc, 1);
    }
};

struct LocalTlsServer {
    int listen_fd{-1};
    uint16_t port{0};
    SSL_CTX* ctx{nullptr};

    bool start() {
        listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) return false;
        int opt = 1;
        ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(0);
        if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            return false;
        }
        if (::listen(listen_fd, 8) != 0) return false;
        socklen_t alen = sizeof(addr);
        if (::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &alen) != 0) {
            return false;
        }
        port = ntohs(addr.sin_port);

        ctx = SSL_CTX_new(TLS_server_method());
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
        SSL_CTX_use_certificate(ctx, BinanceWSProtocolTest::x509_);
        SSL_CTX_use_PrivateKey(ctx, BinanceWSProtocolTest::pkey_);
        return true;
    }

    ~LocalTlsServer() {
        if (ctx) SSL_CTX_free(ctx);
        if (listen_fd >= 0) ::close(listen_fd);
    }

    int accept_fd() {
        sockaddr_in peer{};
        socklen_t plen = sizeof(peer);
        return ::accept(listen_fd, reinterpret_cast<sockaddr*>(&peer), &plen);
    }

    SSL* ssl_accept(int fd) {
        SSL* s = SSL_new(ctx);
        SSL_set_fd(s, fd);
        if (SSL_accept(s) != 1) {
            SSL_free(s);
            return nullptr;
        }
        return s;
    }

    static void close_ssl(SSL* s, int fd) {
        if (s) {
            SSL_shutdown(s);
            SSL_free(s);
        }
        if (fd >= 0) ::close(fd);
    }

    static std::string read_http_request(SSL* s) {
        std::string req;
        char buf[1024];
        while (req.find("\r\n\r\n") == std::string::npos) {
            const int n = SSL_read(s, buf, sizeof(buf));
            if (n <= 0) break;
            req.append(buf, static_cast<std::size_t>(n));
            if (req.size() > 8192) break;
        }
        return req;
    }

    static bool write_switching(SSL* s, const std::string& req,
                                const std::string& extra,
                                const char* status = "HTTP/1.1 101 Switching Protocols",
                                bool upgrade = true,
                                bool connection = true,
                                const char* accept_override = nullptr) {
        const std::string key = extract_ws_key(req.c_str());
        if (key.empty()) return false;
        const std::string accept =
            accept_override ? std::string(accept_override) : ws_accept_from_key(key);
        const std::string body = http_switching(accept, status, upgrade, connection) + extra;
        return ssl_write_all(s, body.data(), static_cast<int>(body.size()));
    }
};

struct TestIngestor {
    std::unique_ptr<egbos::shm::MarketEventsRegion> market_events =
        std::make_unique<egbos::shm::MarketEventsRegion>();
    egbos::shm::GovernanceFlags gov_flags{};
    FeedIngestor ingestor{market_events.get(), &gov_flags, -1};
};

static bool set_short_rcvtimeo(int fd, int usec) {
    struct timeval rcvtimeo{};
    rcvtimeo.tv_sec = 0;
    rcvtimeo.tv_usec = usec;
    return ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rcvtimeo, sizeof(rcvtimeo)) == 0;
}

// ── A1 ────────────────────────────────────────────────────────────────────

TEST_F(BinanceWSProtocolTest, T_A1_Happy_DrainAndReconnect) {
    LocalTlsServer server;
    ASSERT_TRUE(server.start());

    std::atomic<int> accepts{0};
    std::thread server_thread([&]() {
        int fd = server.accept_fd();
        if (fd < 0) return;
        SSL* s = server.ssl_accept(fd);
        if (!s) {
            ::close(fd);
            return;
        }
        ++accepts;
        const std::string req = LocalTlsServer::read_http_request(s);
        LocalTlsServer::write_switching(s, req, {});

        const uint64_t plen = BinanceWSProtocolTest::frame_buf_size() + 1;
        uint8_t hdr[10] = {0x82, 127};
        put_be64(hdr + 2, plen);
        ssl_write_all(s, hdr, sizeof(hdr));
        std::vector<uint8_t> payload(static_cast<std::size_t>(plen), 0xAB);
        ssl_write_all(s, payload.data(), static_cast<int>(payload.size()));
        LocalTlsServer::close_ssl(s, fd);

        fd = server.accept_fd();
        if (fd < 0) return;
        s = server.ssl_accept(fd);
        if (!s) {
            ::close(fd);
            return;
        }
        ++accepts;
        const std::string req2 = LocalTlsServer::read_http_request(s);
        std::string extra(reinterpret_cast<const char*>(kHelloFrame), sizeof(kHelloFrame));
        LocalTlsServer::write_switching(s, req2, extra);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        LocalTlsServer::close_ssl(s, fd);
    });

    TestIngestor env;
    BinanceFeedConfig config;
    config.use_testnet = true;
    config.host = "127.0.0.1";
    config.port = server.port;
    config.path = "/ws";
    config.auto_reconnect = true;
    BinanceFeedListener listener(env.ingestor, config);
    disable_verify(listener);

    std::atomic<int> frames{0};
    listener.set_frame_arrival_callback([&](uint64_t) noexcept { frames.fetch_add(1); });

    std::thread loop([&]() { listener.run_loop(); });
    const auto deadline = clock::now() + std::chrono::seconds(8);
    while (clock::now() < deadline && frames.load() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    listener.request_stop();
    loop.join();
    server_thread.join();

    EXPECT_GE(accepts.load(), 2);
    EXPECT_GE(frames.load(), 1);
}

TEST_F(BinanceWSProtocolTest, T_A1_Negative_MidDrainTerminal) {
    SocketPairHarness harness;
    harness.do_handshake();

    TestIngestor env;
    BinanceFeedConfig config;
    BinanceFeedListener listener(env.ingestor, config);
    harness.steal_client(listener);

    const uint64_t plen = BinanceWSProtocolTest::frame_buf_size() + 1;
    uint8_t hdr[10] = {0x82, 127};
    put_be64(hdr + 2, plen);
    SSL_write(harness.server_ssl, hdr, sizeof(hdr));
    uint8_t few[8]{};
    SSL_write(harness.server_ssl, few, sizeof(few));
    SSL_shutdown(harness.server_ssl);
    ::close(harness.fds[0]);
    harness.fds[0] = -1;

    EXPECT_EQ(recv_frame(listener), -1);
}

// ── A2 ────────────────────────────────────────────────────────────────────

TEST_F(BinanceWSProtocolTest, T_A2_Progression) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);
    socklen_t len = sizeof(addr);
    getsockname(server_fd, (struct sockaddr*)&addr, &len);
    int port = ntohs(addr.sin_port);

    TestIngestor env;
    BinanceFeedConfig config;
    config.host = "127.0.0.1";
    config.port = port;
    config.auto_reconnect = true;
    BinanceFeedListener listener(env.ingestor, config);
    disable_verify(listener);

    std::mutex mu;
    std::vector<clock::time_point> fails;
    std::atomic<bool> watch{true};
    std::thread watcher([&]() {
        while (watch.load()) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(server_fd, &set);
            struct timeval tv{0, 50000}; // 50ms timeout
            if (select(server_fd + 1, &set, NULL, NULL, &tv) > 0) {
                int client = accept(server_fd, NULL, NULL);
                if (client >= 0) {
                    close(client);
                }
            }
        }
        close(server_fd);
    });

    std::thread loop([&]() { listener.run_loop(); });
    
    // Wait for 4 connection attempts. The exact timing of the source event
    // is recorded when the listener increments its connect_attempts counter.
    auto start = std::chrono::steady_clock::now();
    uint64_t last_attempts = 0;
    while (fails.size() < 4) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(15)) break;
        
        uint64_t current_attempts = listener.connect_attempts();
        if (current_attempts > last_attempts) {
            fails.push_back(std::chrono::steady_clock::now());
            last_attempts = current_attempts;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    listener.request_stop();
    loop.join();
    watch = false;
    watcher.join();

    ASSERT_GE(fails.size(), 4u);
    auto gap_ms = [&](std::size_t i) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(fails[i] - fails[i - 1]).count();
    };
    EXPECT_NEAR(gap_ms(1), 1000, 350);
    EXPECT_NEAR(gap_ms(2), 2000, 350);
    EXPECT_NEAR(gap_ms(3), 4000, 400);
}

TEST_F(BinanceWSProtocolTest, T_A2_Responsiveness) {
    TestIngestor env;
    BinanceFeedConfig config;
    config.host = "127.0.0.1";
    config.port = 1;
    config.auto_reconnect = true;
    BinanceFeedListener listener(env.ingestor, config);
    disable_verify(listener);

    std::thread loop([&]() { listener.run_loop(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const auto t0 = clock::now();
    listener.request_stop();
    loop.join();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count();
    EXPECT_LT(ms, 1500);
}

TEST_F(BinanceWSProtocolTest, T_A2_Reset) {
    LocalTlsServer server;
    ASSERT_TRUE(server.start());

    std::atomic<int> accepts{0};
    std::atomic<bool> stop_server{false};
    std::thread server_thread([&]() {
        while (!stop_server.load()) {
            int fd = server.accept_fd();
            if (fd < 0) return;
            SSL* s = server.ssl_accept(fd);
            if (!s) {
                ::close(fd);
                continue;
            }
            ++accepts;
            const std::string req = LocalTlsServer::read_http_request(s);
            LocalTlsServer::write_switching(s, req, {});
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            LocalTlsServer::close_ssl(s, fd);
        }
    });

    TestIngestor env;
    BinanceFeedConfig config;
    config.host = "127.0.0.1";
    config.port = server.port;
    config.path = "/ws";
    config.auto_reconnect = true;
    BinanceFeedListener listener(env.ingestor, config);
    disable_verify(listener);

    std::thread loop([&]() { listener.run_loop(); });
    const auto deadline = clock::now() + std::chrono::seconds(6);
    while (clock::now() < deadline && accepts.load() < 2) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    listener.request_stop();
    loop.join();
    stop_server = true;
    int poke = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(server.port);
    ::connect(poke, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::close(poke);
    server_thread.join();

    EXPECT_GE(accepts.load(), 2);
}

// ── A3 (real kernel SO_RCVTIMEO on the socket fd; not BIO_mem/BIO_pair) ──

TEST_F(BinanceWSProtocolTest, T_A3_Idle) {
    SocketPairHarness harness;
    harness.do_handshake();

    TestIngestor env;
    BinanceFeedConfig config;
    BinanceFeedListener listener(env.ingestor, config);
    harness.steal_client(listener);
    ASSERT_TRUE(set_short_rcvtimeo(fd(listener), 50000));

    EXPECT_EQ(recv_frame(listener), -2);
}

TEST_F(BinanceWSProtocolTest, T_A3_MidFrame) {
    SocketPairHarness harness;
    harness.do_handshake();

    TestIngestor env;
    BinanceFeedConfig config;
    BinanceFeedListener listener(env.ingestor, config);
    harness.steal_client(listener);
    ASSERT_TRUE(set_short_rcvtimeo(fd(listener), 50000));

    uint8_t hdr[] = {0x82};
    SSL_write(harness.server_ssl, hdr, 1);

    EXPECT_EQ(recv_frame(listener), -1);
}

// ── A4 ────────────────────────────────────────────────────────────────────

TEST_F(BinanceWSProtocolTest, T_A4_Coalesced) {
    LocalTlsServer server;
    ASSERT_TRUE(server.start());

    std::thread server_thread([&]() {
        int fd = server.accept_fd();
        ASSERT_GE(fd, 0);
        SSL* s = server.ssl_accept(fd);
        ASSERT_NE(s, nullptr);
        const std::string req = LocalTlsServer::read_http_request(s);
        std::string extra(reinterpret_cast<const char*>(kHelloFrame), sizeof(kHelloFrame));
        ASSERT_TRUE(LocalTlsServer::write_switching(s, req, extra));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        LocalTlsServer::close_ssl(s, fd);
    });

    TestIngestor env;
    BinanceFeedConfig config;
    config.host = "127.0.0.1";
    config.port = server.port;
    config.path = "/ws";
    BinanceFeedListener listener(env.ingestor, config);
    disable_verify(listener);
    ASSERT_TRUE(perform_tls_connect(listener));
    ASSERT_TRUE(perform_websocket_handshake(listener));
    EXPECT_EQ(pending_available(listener), sizeof(kHelloFrame));
    EXPECT_EQ(recv_frame(listener), 5);
    server_thread.join();
}

static std::vector<int> g_frag_sizes;
static std::size_t g_frag_idx = 0;

static int fragmenting_ssl_read(SSL* ssl, void* buf, int num) {
    if (g_frag_idx < g_frag_sizes.size()) {
        num = std::min(num, g_frag_sizes[g_frag_idx++]);
    }
    return ::SSL_read(ssl, buf, num);
}

TEST_F(BinanceWSProtocolTest, T_A4_Fragmented) {
    LocalTlsServer server;
    ASSERT_TRUE(server.start());

    std::thread server_thread([&]() {
        int fd = server.accept_fd();
        ASSERT_GE(fd, 0);
        SSL* s = server.ssl_accept(fd);
        ASSERT_NE(s, nullptr);
        const std::string req = LocalTlsServer::read_http_request(s);
        std::string extra(reinterpret_cast<const char*>(kHelloFrame), sizeof(kHelloFrame));
        ASSERT_TRUE(LocalTlsServer::write_switching(s, req, extra));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        LocalTlsServer::close_ssl(s, fd);
    });

    TestIngestor env;
    BinanceFeedConfig config;
    config.host = "127.0.0.1";
    config.port = server.port;
    config.path = "/ws";
    BinanceFeedListener listener(env.ingestor, config);
    disable_verify(listener);
    ASSERT_TRUE(perform_tls_connect(listener));

    g_frag_sizes = {5, 7, 9, 11, 13};
    g_frag_idx = 0;
    set_injected_ssl_read(listener, fragmenting_ssl_read);

    ASSERT_TRUE(perform_websocket_handshake(listener));
    EXPECT_EQ(recv_frame(listener), 5);
    server_thread.join();
}

TEST_F(BinanceWSProtocolTest, T_A4_PartialFrameLeftover) {
    LocalTlsServer server;
    ASSERT_TRUE(server.start());

    std::thread server_thread([&]() {
        int fd = server.accept_fd();
        ASSERT_GE(fd, 0);
        SSL* s = server.ssl_accept(fd);
        ASSERT_NE(s, nullptr);
        const std::string req = LocalTlsServer::read_http_request(s);
        std::string extra(reinterpret_cast<const char*>(kHelloFrame), 3);
        ASSERT_TRUE(LocalTlsServer::write_switching(s, req, extra));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        SSL_write(s, kHelloFrame + 3, static_cast<int>(sizeof(kHelloFrame) - 3));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        LocalTlsServer::close_ssl(s, fd);
    });

    TestIngestor env;
    BinanceFeedConfig config;
    config.host = "127.0.0.1";
    config.port = server.port;
    config.path = "/ws";
    BinanceFeedListener listener(env.ingestor, config);
    disable_verify(listener);
    ASSERT_TRUE(perform_tls_connect(listener));
    ASSERT_TRUE(perform_websocket_handshake(listener));
    EXPECT_EQ(pending_available(listener), 3u);
    EXPECT_EQ(recv_frame(listener), 5);
    server_thread.join();
}

TEST_F(BinanceWSProtocolTest, T_A4_MultipleLeftover) {
    LocalTlsServer server;
    ASSERT_TRUE(server.start());

    std::thread server_thread([&]() {
        int fd = server.accept_fd();
        ASSERT_GE(fd, 0);
        SSL* s = server.ssl_accept(fd);
        ASSERT_NE(s, nullptr);
        const std::string req = LocalTlsServer::read_http_request(s);
        std::string extra;
        extra.append(reinterpret_cast<const char*>(kHelloFrame), sizeof(kHelloFrame));
        extra.append(reinterpret_cast<const char*>(kWorldFrame), sizeof(kWorldFrame));
        ASSERT_TRUE(LocalTlsServer::write_switching(s, req, extra));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        LocalTlsServer::close_ssl(s, fd);
    });

    TestIngestor env;
    BinanceFeedConfig config;
    config.host = "127.0.0.1";
    config.port = server.port;
    config.path = "/ws";
    BinanceFeedListener listener(env.ingestor, config);
    disable_verify(listener);
    ASSERT_TRUE(perform_tls_connect(listener));
    ASSERT_TRUE(perform_websocket_handshake(listener));
    EXPECT_EQ(pending_available(listener), sizeof(kHelloFrame) + sizeof(kWorldFrame));
    EXPECT_EQ(recv_frame(listener), 5);
    EXPECT_EQ(recv_frame(listener), 5);
    server_thread.join();
}

// ── A5 ────────────────────────────────────────────────────────────────────

TEST_F(BinanceWSProtocolTest, T_A5_Valid) {
    LocalTlsServer server;
    ASSERT_TRUE(server.start());

    std::thread server_thread([&]() {
        int fd = server.accept_fd();
        ASSERT_GE(fd, 0);
        SSL* s = server.ssl_accept(fd);
        ASSERT_NE(s, nullptr);
        const std::string req = LocalTlsServer::read_http_request(s);
        ASSERT_TRUE(LocalTlsServer::write_switching(s, req, {}));
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        LocalTlsServer::close_ssl(s, fd);
    });

    TestIngestor env;
    BinanceFeedConfig config;
    config.host = "127.0.0.1";
    config.port = server.port;
    config.path = "/ws";
    BinanceFeedListener listener(env.ingestor, config);
    disable_verify(listener);
    ASSERT_TRUE(perform_tls_connect(listener));
    EXPECT_TRUE(perform_websocket_handshake(listener));
    server_thread.join();
}

TEST_F(BinanceWSProtocolTest, T_A5_InvalidAccept) {
    LocalTlsServer server;
    ASSERT_TRUE(server.start());

    std::thread server_thread([&]() {
        int fd = server.accept_fd();
        ASSERT_GE(fd, 0);
        SSL* s = server.ssl_accept(fd);
        ASSERT_NE(s, nullptr);
        const std::string req = LocalTlsServer::read_http_request(s);
        ASSERT_TRUE(LocalTlsServer::write_switching(
            s, req, {}, "HTTP/1.1 101 Switching Protocols", true, true,
            "xxxxxxxxxxxxxxxxxxxxxxxx"));
        LocalTlsServer::close_ssl(s, fd);
    });

    TestIngestor env;
    BinanceFeedConfig config;
    config.host = "127.0.0.1";
    config.port = server.port;
    config.path = "/ws";
    BinanceFeedListener listener(env.ingestor, config);
    disable_verify(listener);
    ASSERT_TRUE(perform_tls_connect(listener));
    EXPECT_FALSE(perform_websocket_handshake(listener));
    server_thread.join();
}

TEST_F(BinanceWSProtocolTest, T_A5_WrongStatus) {
    LocalTlsServer server;
    ASSERT_TRUE(server.start());

    std::thread server_thread([&]() {
        int fd = server.accept_fd();
        ASSERT_GE(fd, 0);
        SSL* s = server.ssl_accept(fd);
        ASSERT_NE(s, nullptr);
        const std::string req = LocalTlsServer::read_http_request(s);
        ASSERT_TRUE(LocalTlsServer::write_switching(s, req, {}, "HTTP/1.1 200 OK"));
        LocalTlsServer::close_ssl(s, fd);
    });

    TestIngestor env;
    BinanceFeedConfig config;
    config.host = "127.0.0.1";
    config.port = server.port;
    config.path = "/ws";
    BinanceFeedListener listener(env.ingestor, config);
    disable_verify(listener);
    ASSERT_TRUE(perform_tls_connect(listener));
    EXPECT_FALSE(perform_websocket_handshake(listener));
    server_thread.join();
}

TEST_F(BinanceWSProtocolTest, T_A5_InvalidRequiredHeader) {
    LocalTlsServer server;
    ASSERT_TRUE(server.start());

    std::thread server_thread([&]() {
        int fd = server.accept_fd();
        ASSERT_GE(fd, 0);
        SSL* s = server.ssl_accept(fd);
        ASSERT_NE(s, nullptr);
        const std::string req = LocalTlsServer::read_http_request(s);
        ASSERT_TRUE(LocalTlsServer::write_switching(
            s, req, {}, "HTTP/1.1 101 Switching Protocols", false, true, nullptr));
        LocalTlsServer::close_ssl(s, fd);
    });

    TestIngestor env;
    BinanceFeedConfig config;
    config.host = "127.0.0.1";
    config.port = server.port;
    config.path = "/ws";
    BinanceFeedListener listener(env.ingestor, config);
    disable_verify(listener);
    ASSERT_TRUE(perform_tls_connect(listener));
    EXPECT_FALSE(perform_websocket_handshake(listener));
    server_thread.join();
}

// ── A6 ────────────────────────────────────────────────────────────────────

TEST_F(BinanceWSProtocolTest, T_A6_Completion) {
    SocketPairHarness harness;
    harness.do_handshake();

    TestIngestor env;
    BinanceFeedConfig config;
    BinanceFeedListener listener(env.ingestor, config);
    harness.steal_client(listener);

    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        uint8_t b1[] = {0x81};
        SSL_write(harness.server_ssl, b1, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        uint8_t b2[] = {0x05};
        SSL_write(harness.server_ssl, b2, 1);
        SSL_write(harness.server_ssl, "hello", 5);
    });

    EXPECT_EQ(recv_frame(listener), 5);
    t.join();
}

TEST_F(BinanceWSProtocolTest, T_A6_Terminal) {
    SocketPairHarness harness;
    harness.do_handshake();

    TestIngestor env;
    BinanceFeedConfig config;
    BinanceFeedListener listener(env.ingestor, config);
    harness.steal_client(listener);

    uint8_t b1[] = {0x81};
    SSL_write(harness.server_ssl, b1, 1);
    SSL_shutdown(harness.server_ssl);
    ::close(harness.fds[0]);
    harness.fds[0] = -1;

    EXPECT_EQ(recv_frame(listener), -1);
}

// ── R4 ────────────────────────────────────────────────────────────────────

TEST_F(BinanceWSProtocolTest, T_R4_ProtocolInvalid) {
    SocketPairHarness harness;
    harness.do_handshake();

    TestIngestor env;
    BinanceFeedConfig config;
    BinanceFeedListener listener(env.ingestor, config);
    harness.steal_client(listener);

    uint8_t hdr[] = {0x82, 127, 0x80, 0, 0, 0, 0, 0, 0, 0};
    SSL_write(harness.server_ssl, hdr, sizeof(hdr));

    EXPECT_EQ(recv_frame(listener), -1);
}

TEST_F(BinanceWSProtocolTest, T_R4_ResourceExhaustion) {
    // kMaxDrainSize is a NEW POLICY DECISION, not existing governance.
    // Implemented as fail-closed resource bound, separate from MSB=1.
    SocketPairHarness harness;
    harness.do_handshake();

    TestIngestor env;
    BinanceFeedConfig config;
    BinanceFeedListener listener(env.ingestor, config);
    harness.steal_client(listener);

    uint8_t hdr[10] = {0x82, 127};
    put_be64(hdr + 2, BinanceWSProtocolTest::max_drain_size() + 1);
    SSL_write(harness.server_ssl, hdr, sizeof(hdr));

    EXPECT_EQ(recv_frame(listener), -1);
}

} // namespace egbos::l0
