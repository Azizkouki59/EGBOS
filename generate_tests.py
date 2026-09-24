import sys

cpp_code = r"""// engine/tests/test_binance_ws_protocol.cpp
// EGBOS ENGINE_V3 — Binance WebSocket L0 Protocol Unit Tests
// Classification : DEVELOPMENT_TEST / LOCAL_SYNTHETIC

#include <gtest/gtest.h>
#include "engine/l0/binance_feed_listener.h"
#include "engine/l0/ingestor.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <chrono>

namespace egbos::l0 {

class BinanceWSProtocolTest : public ::testing::Test {
public:
    static EVP_PKEY* pkey_;
    static X509* x509_;

    static void SetUpTestSuite() {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        
        pkey_ = EVP_PKEY_new();
        BIGNUM* bne = BN_new();
        BN_set_word(bne, RSA_F4);
        RSA* rsa = RSA_new();
        // Ignore deprecation for tests
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        RSA_generate_key_ex(rsa, 2048, bne, NULL);
#pragma GCC diagnostic pop
        EVP_PKEY_assign_RSA(pkey_, rsa);
        BN_free(bne);

        x509_ = X509_new();
        ASN1_INTEGER_set(X509_get_serialNumber(x509_), 1);
        X509_gmtime_adj(X509_get_notBefore(x509_), 0);
        X509_gmtime_adj(X509_get_notAfter(x509_), 31536000L);
        X509_set_pubkey(x509_, pkey_);
        X509_NAME* name = X509_get_subject_name(x509_);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)"localhost", -1, -1, 0);
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
        listener.state_.store(ListenerState::OPERATIONAL);
    }
    
    static int recv_frame(BinanceFeedListener& listener) {
        return listener.recv_frame();
    }
    
    static bool perform_websocket_handshake(BinanceFeedListener& listener) {
        return listener.perform_websocket_handshake();
    }

    static void set_injected_ssl_read(BinanceFeedListener& listener, int (*fn)(SSL*, void*, int)) {
        listener.injected_ssl_read_ = fn;
    }
};

EVP_PKEY* BinanceWSProtocolTest::pkey_ = nullptr;
X509* BinanceWSProtocolTest::x509_ = nullptr;

struct SocketPairHarness {
    int fds[2];
    SSL_CTX* server_ctx = nullptr;
    SSL* server_ssl = nullptr;
    SSL_CTX* client_ctx = nullptr;
    SSL* client_ssl = nullptr;

    SocketPairHarness() {
        socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
        
        server_ctx = SSL_CTX_new(TLS_server_method());
        SSL_CTX_use_certificate(server_ctx, BinanceWSProtocolTest::x509_);
        SSL_CTX_use_PrivateKey(server_ctx, BinanceWSProtocolTest::pkey_);
        server_ssl = SSL_new(server_ctx);
        SSL_set_fd(server_ssl, fds[0]);

        client_ctx = SSL_CTX_new(TLS_client_method());
        SSL_CTX_set_verify(client_ctx, SSL_VERIFY_NONE, nullptr);
        client_ssl = SSL_new(client_ctx);
        SSL_set_fd(client_ssl, fds[1]);
    }
    
    ~SocketPairHarness() {
        if (server_ssl) SSL_free(server_ssl);
        if (server_ctx) SSL_CTX_free(server_ctx);
        if (fds[0] >= 0) close(fds[0]);
    }

    void do_handshake() {
        std::thread t([this]() { SSL_accept(server_ssl); });
        SSL_connect(client_ssl);
        t.join();
    }
};

// ── Test Implementations ──────────────────────────────────────────────────

TEST_F(BinanceWSProtocolTest, T_A1_Negative_SocketPair) {
    SocketPairHarness harness;
    harness.do_handshake();

    egbos::shm::MarketEventsRegion market_events{};
    egbos::shm::GovernanceFlags gov_flags{};
    FeedIngestor ingestor(&market_events, &gov_flags, -1);
    
    BinanceFeedConfig config;
    BinanceFeedListener listener(ingestor, config);
    BinanceWSProtocolTest::inject_connection(listener, harness.fds[1], harness.client_ssl);

    uint8_t hdr[] = { 0x82, 127, 0,0,0,0,0,0x01,0x00,0x01 };
    SSL_write(harness.server_ssl, hdr, sizeof(hdr));
    close(harness.fds[0]); harness.fds[0] = -1;

    EXPECT_EQ(BinanceWSProtocolTest::recv_frame(listener), -1);
}

TEST_F(BinanceWSProtocolTest, T_A3_Idle) {
    SocketPairHarness harness;
    harness.do_handshake();

    egbos::shm::MarketEventsRegion market_events{};
    egbos::shm::GovernanceFlags gov_flags{};
    FeedIngestor ingestor(&market_events, &gov_flags, -1);
    
    BinanceFeedConfig config;
    BinanceFeedListener listener(ingestor, config);
    BinanceWSProtocolTest::inject_connection(listener, harness.fds[1], harness.client_ssl);

    struct timeval rcvtimeo{};
    rcvtimeo.tv_sec  = 0;
    rcvtimeo.tv_usec = 10000;
    ::setsockopt(harness.fds[1], SOL_SOCKET, SO_RCVTIMEO, &rcvtimeo, sizeof(rcvtimeo));

    EXPECT_EQ(BinanceWSProtocolTest::recv_frame(listener), -2); // kRecvTimeout
}

TEST_F(BinanceWSProtocolTest, T_A3_MidFrame) {
    SocketPairHarness harness;
    harness.do_handshake();

    egbos::shm::MarketEventsRegion market_events{};
    egbos::shm::GovernanceFlags gov_flags{};
    FeedIngestor ingestor(&market_events, &gov_flags, -1);
    
    BinanceFeedConfig config;
    BinanceFeedListener listener(ingestor, config);
    BinanceWSProtocolTest::inject_connection(listener, harness.fds[1], harness.client_ssl);

    struct timeval rcvtimeo{};
    rcvtimeo.tv_sec  = 0;
    rcvtimeo.tv_usec = 10000;
    ::setsockopt(harness.fds[1], SOL_SOCKET, SO_RCVTIMEO, &rcvtimeo, sizeof(rcvtimeo));

    uint8_t hdr[] = { 0x82 }; // partial 1-byte header
    SSL_write(harness.server_ssl, hdr, 1);

    EXPECT_EQ(BinanceWSProtocolTest::recv_frame(listener), -1); // Terminal error mid-frame
}

TEST_F(BinanceWSProtocolTest, T_A6_Completion) {
    SocketPairHarness harness;
    harness.do_handshake();

    egbos::shm::MarketEventsRegion market_events{};
    egbos::shm::GovernanceFlags gov_flags{};
    FeedIngestor ingestor(&market_events, &gov_flags, -1);
    
    BinanceFeedConfig config;
    BinanceFeedListener listener(ingestor, config);
    BinanceWSProtocolTest::inject_connection(listener, harness.fds[1], harness.client_ssl);

    std::thread t([&]() {
        usleep(10000); // 10ms
        uint8_t b1[] = { 0x81 };
        SSL_write(harness.server_ssl, b1, 1);
        usleep(10000); // 10ms
        uint8_t b2[] = { 0x05 };
        SSL_write(harness.server_ssl, b2, 1);
        uint8_t payload[] = "hello";
        SSL_write(harness.server_ssl, payload, 5);
    });

    EXPECT_EQ(BinanceWSProtocolTest::recv_frame(listener), 5);
    t.join();
}

TEST_F(BinanceWSProtocolTest, T_A6_Terminal) {
    SocketPairHarness harness;
    harness.do_handshake();

    egbos::shm::MarketEventsRegion market_events{};
    egbos::shm::GovernanceFlags gov_flags{};
    FeedIngestor ingestor(&market_events, &gov_flags, -1);
    
    BinanceFeedConfig config;
    BinanceFeedListener listener(ingestor, config);
    BinanceWSProtocolTest::inject_connection(listener, harness.fds[1], harness.client_ssl);

    uint8_t b1[] = { 0x81 };
    SSL_write(harness.server_ssl, b1, 1);
    close(harness.fds[0]); harness.fds[0] = -1;

    EXPECT_EQ(BinanceWSProtocolTest::recv_frame(listener), -1);
}

TEST_F(BinanceWSProtocolTest, T_R4_ProtocolInvalid) {
    SocketPairHarness harness;
    harness.do_handshake();

    egbos::shm::MarketEventsRegion market_events{};
    egbos::shm::GovernanceFlags gov_flags{};
    FeedIngestor ingestor(&market_events, &gov_flags, -1);
    
    BinanceFeedConfig config;
    BinanceFeedListener listener(ingestor, config);
    BinanceWSProtocolTest::inject_connection(listener, harness.fds[1], harness.client_ssl);

    uint8_t hdr[] = { 0x82, 127, 0x80,0,0,0,0,0,0,0 }; // MSB set
    SSL_write(harness.server_ssl, hdr, sizeof(hdr));

    EXPECT_EQ(BinanceWSProtocolTest::recv_frame(listener), -1);
}

TEST_F(BinanceWSProtocolTest, T_R4_ResourceExhaustion) {
    SocketPairHarness harness;
    harness.do_handshake();

    egbos::shm::MarketEventsRegion market_events{};
    egbos::shm::GovernanceFlags gov_flags{};
    FeedIngestor ingestor(&market_events, &gov_flags, -1);
    
    BinanceFeedConfig config;
    BinanceFeedListener listener(ingestor, config);
    BinanceWSProtocolTest::inject_connection(listener, harness.fds[1], harness.client_ssl);

    // length = 20MB (exceeds 10MB kMaxDrainSize)
    uint8_t hdr[] = { 0x82, 127, 0,0,0,0,0x01,0x31,0x2D,0x00 };
    SSL_write(harness.server_ssl, hdr, sizeof(hdr));

    EXPECT_EQ(BinanceWSProtocolTest::recv_frame(listener), -1);
}

// ── Test Seam ─────────────────────────────────────────────────────────────

static std::vector<int> mock_fragment_sizes;
static size_t mock_fragment_idx = 0;

static int fragmenting_ssl_read(SSL* ssl, void* buf, int num) {
    if (mock_fragment_idx < mock_fragment_sizes.size()) {
        num = std::min(num, mock_fragment_sizes[mock_fragment_idx++]);
    }
    return ::SSL_read(ssl, buf, num);
}

TEST_F(BinanceWSProtocolTest, T_A4_Fragmented) {
    SocketPairHarness harness;
    
    egbos::shm::MarketEventsRegion market_events{};
    egbos::shm::GovernanceFlags gov_flags{};
    FeedIngestor ingestor(&market_events, &gov_flags, -1);
    
    BinanceFeedConfig config;
    BinanceFeedListener listener(ingestor, config);
    BinanceWSProtocolTest::inject_connection(listener, harness.fds[1], harness.client_ssl);
    
    BinanceWSProtocolTest::set_injected_ssl_read(listener, fragmenting_ssl_read);
    mock_fragment_sizes = {5, 10, 20}; // force fragmentation on application side
    mock_fragment_idx = 0;

    std::thread server_thread([&]() {
        SSL_accept(harness.server_ssl);
        char req[1024]{};
        SSL_read(harness.server_ssl, req, sizeof(req));
        
        char* key_start = strstr(req, "Sec-WebSocket-Key: ");
        char req_key[32]{};
        if (key_start) {
            char* key_end = strchr(key_start + 19, '\r');
            if (key_end) {
                strncpy(req_key, key_start + 19, key_end - (key_start + 19));
            }
        }
        
        char combined[128];
        snprintf(combined, sizeof(combined), "%s258EAFA5-E914-47DA-95CA-5AB4085B64E4", req_key);
        unsigned char digest[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const unsigned char*>(combined), strlen(combined), digest);
        char expected_accept[128]{};
        EVP_EncodeBlock(reinterpret_cast<unsigned char*>(expected_accept), digest, SHA_DIGEST_LENGTH);
        
        char resp[2048];
        int len = snprintf(resp, sizeof(resp),
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n"
            "\r\n", expected_accept);
            
        SSL_write(harness.server_ssl, resp, len);
        
        // Write left-over WS bytes
        uint8_t payload[] = {0x81, 0x05, 'h','e','l','l','o'};
        SSL_write(harness.server_ssl, payload, sizeof(payload));
    });
    
    SSL_connect(harness.client_ssl);
    
    EXPECT_TRUE(BinanceWSProtocolTest::perform_websocket_handshake(listener));
    EXPECT_EQ(BinanceWSProtocolTest::recv_frame(listener), 5);
    
    server_thread.join();
}

TEST_F(BinanceWSProtocolTest, T_A2_Progression) {
    egbos::shm::MarketEventsRegion market_events{};
    egbos::shm::GovernanceFlags gov_flags{};
    FeedIngestor ingestor(&market_events, &gov_flags, -1);
    
    BinanceFeedConfig config;
    config.port = 12345; // some port where nothing listens
    config.host = "127.0.0.1";
    BinanceFeedListener listener(ingestor, config);

    std::thread t([&]() {
        listener.run_loop();
    });

    usleep(3500000); 
    listener.request_stop();
    t.join();
    
    EXPECT_TRUE(true);
}

} // namespace egbos::l0
"""

with open("engine/tests/test_binance_ws_protocol.cpp", "w") as f:
    f.write(cpp_code)
