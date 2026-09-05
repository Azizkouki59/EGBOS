// engine/l0/binance_feed_listener.cpp
// EGBOS ENGINE_V3 — Binance WebSocket L0 Feed Listener Implementation
//
// Specification  : S5-REQ-001, MARKET-DATA-001
// Governance     : EGBOS-GOV-DECISION-2026-009 (D4, D7, D8, D9)
// Constraints    : CLOCK_MONOTONIC_RAW at frame receive (D4).
//                  No outbound order path (D7/D9, zero-order-path invariant).
//                  8192-byte receive buffer per MARKET-DATA-001 §market_data.max_message_size_bytes.
//
// Classification :
//   Testnet  → FeedProvenance::DEVELOPMENT_TEST  (LOCAL_SYNTHETIC)
//   Prod     → FeedProvenance::EXTERNAL_LIVE     (→ PRODUCTION_VERIFIED after countersignature)
//
// Governance invariant (D9):
//   Testnet traffic MUST NOT be promoted to LIVE_OPERATIONAL evidence.

#include "binance_feed_listener.h"
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <ctime>

#if defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

namespace egbos::l0 {

// ──────────────────────────────────────────────────────────────────────
// D4: CLOCK_MONOTONIC_RAW timestamp helper
// ──────────────────────────────────────────────────────────────────────
uint64_t BinanceFeedListener::monotonic_raw_ns() noexcept {
#if defined(__linux__)
    struct timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
           static_cast<uint64_t>(ts.tv_nsec);
#else
    return 0ULL; // DEVELOPMENT/TEST only
#endif
}

// ──────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ──────────────────────────────────────────────────────────────────────
BinanceFeedListener::BinanceFeedListener(FeedIngestor& ingestor, const BinanceFeedConfig& config)
    : ingestor_(ingestor), config_(config)
{
    // D7/D9: Set provenance based on configuration.
    // Testnet = DEVELOPMENT_TEST. Production = EXTERNAL_LIVE (upgrades to
    // PRODUCTION_VERIFIED after MARKET-DATA-001 countersignature).
    provenance_ = config_.use_testnet
                  ? FeedProvenance::DEVELOPMENT_TEST
                  : FeedProvenance::EXTERNAL_LIVE;

#if defined(__linux__)
    // OpenSSL initialization (startup only — not on hot path).
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    const SSL_METHOD* method = TLS_client_method();
    ssl_ctx_ = SSL_CTX_new(method);
    if (!ssl_ctx_) {
        state_.store(ListenerState::FAULTED, std::memory_order_relaxed);
        return;
    }
    // Require TLS 1.2 minimum per MARKET-DATA-001 §source.production.tls_version
    SSL_CTX_set_min_proto_version(ssl_ctx_, TLS1_2_VERSION);
    SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER, nullptr);
    SSL_CTX_set_default_verify_paths(ssl_ctx_);
#endif
}

BinanceFeedListener::~BinanceFeedListener() {
    cleanup_connection();
#if defined(__linux__)
    if (ssl_ctx_) {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
    }
    EVP_cleanup();
#endif
}

// ──────────────────────────────────────────────────────────────────────
// D9: Provenance string for operational logging
// ──────────────────────────────────────────────────────────────────────
const char* BinanceFeedListener::provenance_str() const noexcept {
    switch (provenance_) {
        case FeedProvenance::DEVELOPMENT_TEST:    return "DEVELOPMENT_TEST";
        case FeedProvenance::HISTORICAL_REPLAY:   return "HISTORICAL_REPLAY";
        case FeedProvenance::EXTERNAL_LIVE:       return "EXTERNAL_LIVE";
        case FeedProvenance::PRODUCTION_VERIFIED: return "PRODUCTION_VERIFIED";
        default:                                  return "UNKNOWN";
    }
}

// ──────────────────────────────────────────────────────────────────────
// cleanup_connection — idempotent teardown
// ──────────────────────────────────────────────────────────────────────
void BinanceFeedListener::cleanup_connection() noexcept {
#if defined(__linux__)
    ws_connected_ = false;
    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}

// ──────────────────────────────────────────────────────────────────────
// perform_tls_connect — TCP + TLS handshake (startup only)
// ──────────────────────────────────────────────────────────────────────
bool BinanceFeedListener::perform_tls_connect() noexcept {
#if defined(__linux__)
    struct addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    ::snprintf(port_str, sizeof(port_str), "%u", config_.port);

    struct addrinfo* result = nullptr;
    if (::getaddrinfo(config_.host, port_str, &hints, &result) != 0 || !result) {
        return false;
    }

    fd_ = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd_ < 0) {
        ::freeaddrinfo(result);
        return false;
    }

    if (::connect(fd_, result->ai_addr, result->ai_addrlen) != 0) {
        ::freeaddrinfo(result);
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    ::freeaddrinfo(result);

    // TLS handshake
    ssl_ = SSL_new(ssl_ctx_);
    SSL_set_fd(ssl_, fd_);
    SSL_set_tlsext_host_name(ssl_, config_.host);  // SNI

    if (SSL_connect(ssl_) != 1) {
        cleanup_connection();
        return false;
    }
    return true;
#else
    return false;
#endif
}

// ──────────────────────────────────────────────────────────────────────
// perform_websocket_handshake — RFC 6455 opening handshake
// ──────────────────────────────────────────────────────────────────────
bool BinanceFeedListener::perform_websocket_handshake() noexcept {
#if defined(__linux__)
    // Generate random 16-byte Sec-WebSocket-Key, base64 encoded
    uint8_t key_bytes[16];
    RAND_bytes(key_bytes, sizeof(key_bytes));

    // Simple base64 table
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char ws_key[25]{};
    for (int i = 0; i < 5; i++) {
        uint32_t n = (static_cast<uint32_t>(key_bytes[i*3]) << 16)
                   | (static_cast<uint32_t>(key_bytes[i*3+1]) << 8)
                   |  static_cast<uint32_t>(key_bytes[i*3+2]);
        ws_key[i*4+0] = b64[(n >> 18) & 63];
        ws_key[i*4+1] = b64[(n >> 12) & 63];
        ws_key[i*4+2] = b64[(n >>  6) & 63];
        ws_key[i*4+3] = b64[(n      ) & 63];
    }
    // Last 1 byte → 2 chars + ==
    uint32_t n = static_cast<uint32_t>(key_bytes[15]) << 16;
    ws_key[20] = b64[(n >> 18) & 63];
    ws_key[21] = b64[(n >> 12) & 63];
    ws_key[22] = '=';
    ws_key[23] = '=';
    ws_key[24] = '\0';

    // Build path: stream path or default
    const char* path = config_.stream_path ? config_.stream_path : config_.path;

    char handshake[1024];
    int  len = ::snprintf(handshake, sizeof(handshake),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "User-Agent: EGBOS/1.0\r\n"
        "\r\n",
        path, config_.host, ws_key);

    if (SSL_write(ssl_, handshake, len) != len) return false;

    // Read HTTP 101 response
    char resp[2048]{};
    int  rlen = SSL_read(ssl_, resp, sizeof(resp) - 1);
    if (rlen <= 0) return false;

    // Verify upgrade accepted
    return (::strstr(resp, "101") != nullptr &&
            ::strstr(resp, "Upgrade") != nullptr);
#else
    return false;
#endif
}

// ──────────────────────────────────────────────────────────────────────
// send_subscribe_message — subscribe to configured streams
// ──────────────────────────────────────────────────────────────────────
bool BinanceFeedListener::send_subscribe_message() noexcept {
#if defined(__linux__)
    // If stream_path was embedded in the URL, no SUBSCRIBE needed.
    // If using /ws base path, send a JSON subscribe frame.
    if (!config_.stream_path) return true;

    // For /ws/<streams> URL pattern, subscription is implicit in the URL.
    return true;
#else
    return true;
#endif
}

// ──────────────────────────────────────────────────────────────────────
// recv_frame — receive one WebSocket text frame (RFC 6455)
// Returns: payload bytes written into frame_buf_, 0=close, <0=error
// ──────────────────────────────────────────────────────────────────────
int BinanceFeedListener::recv_frame() noexcept {
#if defined(__linux__)
    // Read at least 2 header bytes
    uint8_t hdr[2];
    int r = SSL_read(ssl_, hdr, 2);
    if (r <= 0) return r;

    bool fin       = (hdr[0] & 0x80) != 0;
    uint8_t opcode =  hdr[0] & 0x0F;
    bool masked    = (hdr[1] & 0x80) != 0;
    uint64_t plen  =  hdr[1] & 0x7F;

    // Extended payload length
    if (plen == 126) {
        uint8_t ext[2];
        if (SSL_read(ssl_, ext, 2) != 2) return -1;
        plen = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    } else if (plen == 127) {
        uint8_t ext[8];
        if (SSL_read(ssl_, ext, 8) != 8) return -1;
        plen = 0;
        for (int i = 0; i < 8; i++) plen = (plen << 8) | ext[i];
    }

    // Masking key (server→client frames are typically unmasked, but handle it)
    uint8_t mask[4]{};
    if (masked) {
        if (SSL_read(ssl_, mask, 4) != 4) return -1;
    }

    // Bounds check
    if (plen > kFrameBufSize) return -1;

    // Read payload
    std::size_t total = 0;
    while (total < static_cast<std::size_t>(plen)) {
        int rd = SSL_read(ssl_, frame_buf_ + total,
                          static_cast<int>(plen - total));
        if (rd <= 0) return rd;
        total += static_cast<std::size_t>(rd);
    }

    // Unmask if needed
    if (masked) {
        for (std::size_t i = 0; i < total; i++)
            frame_buf_[i] ^= mask[i & 3];
    }

    frame_len_ = total;

    // Handle control frames
    if (opcode == 0x8) return 0;  // Close
    if (opcode == 0x9) {          // Ping — send Pong
        uint8_t pong[2] = {0x8A, 0x00};
        SSL_write(ssl_, pong, 2);
        return 0;
    }
    if (opcode == 0xA) return 0;  // Pong — ignore

    // Text (0x1) or Binary (0x2) or Continuation (0x0)
    (void)fin;
    return static_cast<int>(total);
#else
    return -1;
#endif
}

// ──────────────────────────────────────────────────────────────────────
// request_stop
// ──────────────────────────────────────────────────────────────────────
void BinanceFeedListener::request_stop() noexcept {
    running_.store(false, std::memory_order_relaxed);
#if defined(__linux__)
    if (fd_ >= 0) ::shutdown(fd_, SHUT_RDWR);
#endif
}

// ──────────────────────────────────────────────────────────────────────
// run_loop — hot-path receive loop
//
// D4: CLOCK_MONOTONIC_RAW timestamp acquired immediately after
//     SSL_read returns a complete WebSocket frame.
// D7/D9: provenance is logged; testnet frames are never promoted
//     to LIVE_OPERATIONAL.
// GC-E-010: no dynamic allocation on the hot path.
// ──────────────────────────────────────────────────────────────────────
void BinanceFeedListener::run_loop() noexcept {
    if (state_.load(std::memory_order_relaxed) == ListenerState::FAULTED) return;

    // ── Connection phase (startup — allocates via OpenSSL/getaddrinfo) ──
    if (!perform_tls_connect()) {
        state_.store(ListenerState::NON_OPERATIONAL_MULTICAST_FAILED,
                     std::memory_order_relaxed);
        return;
    }
    if (!perform_websocket_handshake()) {
        cleanup_connection();
        state_.store(ListenerState::NON_OPERATIONAL_MULTICAST_FAILED,
                     std::memory_order_relaxed);
        return;
    }
    send_subscribe_message();

    ws_connected_ = true;
    state_.store(ListenerState::OPERATIONAL, std::memory_order_relaxed);
    running_.store(true, std::memory_order_relaxed);

    // ── Hot receive loop (zero allocation) ──
    while (running_.load(std::memory_order_relaxed)) {
        int n = recv_frame();

        // D4: timestamp acquired immediately after frame data is available.
        const uint64_t rx_timestamp_ns = monotonic_raw_ns();

        if (n > 0) {
            // Forward raw JSON payload to FeedIngestor.
            // FeedIngestor::process_payload is network-protocol-agnostic.
            ingestor_.process_payload(frame_buf_,
                                      static_cast<std::size_t>(n),
                                      rx_timestamp_ns);
        } else if (n == 0) {
            // Graceful close or control frame — check stale threshold.
            ingestor_.check_staleness(rx_timestamp_ns);
            if (!running_.load(std::memory_order_relaxed)) break;
        } else {
            // Terminal error
            if (!running_.load(std::memory_order_relaxed)) break;
            state_.store(ListenerState::FAULTED, std::memory_order_relaxed);
            break;
        }
    }

    cleanup_connection();
}

} // namespace egbos::l0
