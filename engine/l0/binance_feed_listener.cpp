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
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <ctime>

#if defined(__linux__)
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <strings.h>
#include <cctype>
#endif

namespace egbos::l0 {

#if defined(__linux__)
static char* find_double_crlf(char* buf, std::size_t len) noexcept {
    if (len < 4) return nullptr;
    for (std::size_t i = 0; i + 3 < len; ++i) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' &&
            buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            return buf + i;
        }
    }
    return nullptr;
}

static void skip_ows(const char*& p) noexcept {
    while (*p == ' ' || *p == '\t') ++p;
}

static const char* trim_ows_end(const char* start, const char* end) noexcept {
    // Strip OWS (SP / HTAB per RFC 7230) and also stray CR that can appear
    // when the server sends CRLF line endings and we null-terminate at the CR.
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) --end;
    return end;
}

static bool ci_eq(const char* a, const char* b, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

static bool header_value_is(const char* value, const char* expected) noexcept {
    skip_ows(value);
    const char* end = value + std::strlen(value);
    end = trim_ows_end(value, end);
    const std::size_t n = static_cast<std::size_t>(end - value);
    const std::size_t e = std::strlen(expected);
    return n == e && ci_eq(value, expected, n);
}

static bool header_has_token(const char* value, const char* token) noexcept {
    const std::size_t tlen = std::strlen(token);
    const char* p = value;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') ++p;
        if (!*p) break;
        const char* start = p;
        while (*p && *p != ',') ++p;
        const char* end = trim_ows_end(start, p);
        const std::size_t n = static_cast<std::size_t>(end - start);
        if (n == tlen && ci_eq(start, token, tlen)) return true;
    }
    return false;
}
#endif

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
    // Force ALPN to http/1.1 only.  Without this restriction the server
    // negotiates HTTP/2 (h2), which is incompatible with the RFC 6455
    // WebSocket Upgrade handshake (requires HTTP/1.1 101 response).
    // Wire format: length-prefixed list of protocol names.
    static const unsigned char kAlpnHttpOne[] = "\x08http/1.1";
    SSL_CTX_set_alpn_protos(ssl_ctx_, kAlpnHttpOne, sizeof(kAlpnHttpOne) - 1);
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
    pending_rx_len_ = 0;
    pending_rx_offset_ = 0;
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
    pending_rx_len_ = 0;
    pending_rx_offset_ = 0;

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

    // A3: Set receive timeout so SSL_read unblocks if the peer stops sending
    // without closing the connection.  5 s is intentionally coarser than the
    // 500 ms stale threshold — avoids spurious staleness callbacks on normal
    // inter-frame gaps while still bounding the worst-case blocking time.
    // SO_RCVTIMEO does not affect connect() or SSL_connect() semantics.
    {
        struct timeval rcvtimeo{};
        rcvtimeo.tv_sec  = 5;
        rcvtimeo.tv_usec = 0;
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO,
                     &rcvtimeo, static_cast<socklen_t>(sizeof(rcvtimeo)));
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
    pending_rx_len_ = 0;
    pending_rx_offset_ = 0;

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

    // A4: accumulate HTTP response across reads until \r\n\r\n or bound.
    // Handshake-only test seam; recv_frame() never uses injected_ssl_read_.
    std::size_t accumulated = 0;
    char resp[kMaxHandshakeSize + 1]{};
    char* terminator = nullptr;
    auto read_fn = injected_ssl_read_ ? injected_ssl_read_ : ::SSL_read;

    while (accumulated < kMaxHandshakeSize) {
        const int want = static_cast<int>(kMaxHandshakeSize - accumulated);
        const int rlen = read_fn(ssl_, resp + accumulated, want);
        if (rlen <= 0) return false;

        accumulated += static_cast<std::size_t>(rlen);
        terminator = find_double_crlf(resp, accumulated);
        if (terminator) break;
    }

    if (!terminator) return false;

    const std::size_t header_len =
        static_cast<std::size_t>(terminator + 4 - resp);
    const std::size_t leftover = accumulated - header_len;

    // Never truncate leftover application bytes. Fail closed if they cannot fit.
    if (leftover > kPendingBufSize) return false;
    if (leftover > 0) {
        ::memcpy(pending_rx_buf_, terminator + 4, leftover);
        pending_rx_len_ = leftover;
        pending_rx_offset_ = 0;
    }

    // A5: explicit field validation — not strstr()-based protocol matching.
    if (header_len < 12 || ::memcmp(resp, "HTTP/1.1 101", 12) != 0) return false;

    bool has_upgrade = false;
    bool has_connection = false;
    bool has_accept = false;
    char accept_val[128]{};

    char* line = resp;
    while (line < terminator) {
        char* eol = line;
        while (eol <= terminator && !(eol[0] == '\r' && eol[1] == '\n')) ++eol;
        if (eol > terminator || !(eol[0] == '\r' && eol[1] == '\n')) break;
        *eol = '\0';

        if (::strncasecmp(line, "Upgrade:", 8) == 0) {
            if (header_value_is(line + 8, "websocket")) has_upgrade = true;
        } else if (::strncasecmp(line, "Connection:", 11) == 0) {
            if (header_has_token(line + 11, "Upgrade")) has_connection = true;
        } else if (::strncasecmp(line, "Sec-WebSocket-Accept:", 21) == 0) {
            const char* v = line + 21;
            skip_ows(v);
            const char* end = trim_ows_end(v, v + std::strlen(v));
            const std::size_t n = static_cast<std::size_t>(end - v);
            if (n >= sizeof(accept_val)) return false;
            ::memcpy(accept_val, v, n);
            accept_val[n] = '\0';
            has_accept = true;
        }

        line = eol + 2;
    }

    if (!has_upgrade || !has_connection || !has_accept) return false;

    // Expected Accept is SHA1(actual_request_key + RFC6455 GUID), Base64.
    char combined[64];
    const int combined_len = ::snprintf(
        combined, sizeof(combined),
        "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", ws_key);
    if (combined_len < 0 || static_cast<std::size_t>(combined_len) >= sizeof(combined)) {
        return false;
    }

    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined),
         static_cast<std::size_t>(combined_len), digest);

    char expected_accept[32]{};
    EVP_EncodeBlock(reinterpret_cast<unsigned char*>(expected_accept),
                    digest, SHA_DIGEST_LENGTH);

    if (::strcmp(accept_val, expected_accept) != 0) return false;

    return true;
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
    if (config_.stream_path) return true;

    // If using /ws base path, send a real RFC6455 JSON subscribe frame.
    const char* payload = "{\"method\": \"SUBSCRIBE\", \"params\": [\"btcusdt@depth@100ms\"], \"id\": 1}";
    std::size_t payload_len = __builtin_strlen(payload);
    
    uint8_t frame[128];
    frame[0] = 0x81; // FIN + Text
    frame[1] = 0x80 | static_cast<uint8_t>(payload_len); // Masked + Length (assume < 126)
    
    // Masking key (dummy 0s for simplicity since we don't care about secrecy here, 
    // but RFC 6455 says it should be random. 0x01020304 is fine)
    frame[2] = 0x01;
    frame[3] = 0x02;
    frame[4] = 0x03;
    frame[5] = 0x04;
    
    for (std::size_t i = 0; i < payload_len; ++i) {
        frame[6 + i] = payload[i] ^ frame[2 + (i % 4)];
    }
    
    int total_len = 6 + payload_len;
    return SSL_write(ssl_, frame, total_len) == total_len;
#else
    return true;
#endif
}
// recv_frame — receive one WebSocket text frame (RFC 6455)
// Returns: payload bytes written into frame_buf_, 0=close, <0=error
//
// A3: Returns kRecvTimeout when SO_RCVTIMEO fires on the initial header
// read (blocking socket — timeout surfaces as SSL_ERROR_WANT_READ + EAGAIN/EWOULDBLOCK).
// Mid-frame timeouts are treated as terminal errors: the stream is already
// desynchronised and recovery requires a full reconnect.
// ──────────────────────────────────────────────────────────────────────

// A3 sentinel — distinct from -1 (terminal error) and 0 (graceful close).
// Internal to recv_frame() / run_loop(); not part of the public API.
static constexpr int kRecvTimeout = -2;

// AD-003 idle service:
// Wake before attempting the first byte of a new WebSocket frame so the
// FeedIngestor can service its 5ms Tier-0 flush timer while the feed is idle.
// We deliberately do not impose this timeout on an already-started frame;
// fragmented/mid-frame reads retain the existing SO_RCVTIMEO behavior.
static int wait_for_frame_start(int fd, int timeout_ms) noexcept {
    if (fd < 0) return -1;

    struct pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;

    int rc;
    do {
        rc = ::poll(&pfd, 1, timeout_ms);
    } while (rc < 0 && errno == EINTR);

    if (rc == 0) return 0;
    if (rc < 0) return -1;

    if (pfd.revents & POLLNVAL) return -1;

    // POLLERR/POLLHUP are intentionally allowed through to SSL_read(),
    // which remains the authority for TLS/socket error classification.
    return 1;
}

int BinanceFeedListener::recv_frame() noexcept {
#if defined(__linux__)
    auto fill_buffer = [&](uint8_t* out, std::size_t N, bool is_initial_byte) -> int {
        std::size_t accumulated = 0;
        while (accumulated < N) {
            if (pending_rx_offset_ < pending_rx_len_) {
                // Consume leftovers first; never SSL_read() while pending remains.
                const std::size_t avail = pending_rx_len_ - pending_rx_offset_;
                const std::size_t consume = std::min(avail, N - accumulated);
                ::memcpy(out + accumulated, pending_rx_buf_ + pending_rx_offset_, consume);
                pending_rx_offset_ += consume;
                accumulated += consume;
                if (pending_rx_offset_ == pending_rx_len_) {
                    pending_rx_offset_ = 0;
                    pending_rx_len_ = 0;
                }
                continue;
            }

            // Before the first byte of a new frame, wait only 5ms.
            // This lets run_loop() call check_staleness(), which also services
            // the AD-003 Tier-0 5ms flush deadline during feed idleness.
            if (accumulated == 0 && is_initial_byte &&
                ::SSL_pending(ssl_) == 0) {
                const int ready = wait_for_frame_start(fd_, 5);
                if (ready == 0) {
                    return kRecvTimeout;
                }
                if (ready < 0) {
                    return -1;
                }
            }

            const int rd = ::SSL_read(ssl_, out + accumulated,
                                      static_cast<int>(N - accumulated));
            if (rd <= 0) {
                const int saved_errno = errno;
                if (accumulated == 0 && is_initial_byte) {
                    const int ssl_err = SSL_get_error(ssl_, rd);
                    const bool timeout_errno =
                        saved_errno == EAGAIN ||
                        saved_errno == EWOULDBLOCK ||
                        saved_errno == ETIMEDOUT;
                    if (timeout_errno &&
                        (ssl_err == SSL_ERROR_WANT_READ ||
                         ssl_err == SSL_ERROR_SYSCALL)) {
                        return kRecvTimeout;
                    }
                }
                return -1;
            }
            accumulated += static_cast<std::size_t>(rd);
        }
        return static_cast<int>(accumulated);
    };

    uint8_t hdr[2];
    int r = fill_buffer(hdr, 2, true);
    if (r < 0) return r;

    bool fin       = (hdr[0] & 0x80) != 0;
    uint8_t opcode =  hdr[0] & 0x0F;
    bool masked    = (hdr[1] & 0x80) != 0;
    uint64_t plen  =  hdr[1] & 0x7F;

    if (plen == 126) {
        uint8_t ext[2];
        if (fill_buffer(ext, 2, false) < 0) return -1;
        plen = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    } else if (plen == 127) {
        uint8_t ext[8];
        if (fill_buffer(ext, 8, false) < 0) return -1;
        if ((ext[0] & 0x80) != 0) return -1; // Protocol-invalid: MSB=1 fail closed
        plen = 0;
        for (int i = 0; i < 8; i++) plen = (plen << 8) | ext[i];
    }

    // Technical safeguard: refuse drain above bound to prevent resource exhaustion.
    if (plen > kMaxDrainSize) return -1;

    uint8_t mask[4]{};
    if (masked) {
        if (fill_buffer(mask, 4, false) < 0) return -1;
    }

    if (plen > kFrameBufSize) {
        constexpr std::size_t kDiscardChunk = 4096;
        uint8_t discard_buf[kDiscardChunk];
        uint64_t remaining = plen;
        while (remaining > 0) {
            std::size_t want = std::min(remaining, static_cast<uint64_t>(kDiscardChunk));
            if (fill_buffer(discard_buf, want, false) < 0) return -1;
            remaining -= want;
        }
        // Bounded drain of exactly plen bytes, then terminal error.
        // Not same-connection resynchronization: caller must reconnect.
        return -1;
    }

    if (plen > 0) {
        if (fill_buffer(frame_buf_, static_cast<std::size_t>(plen), false) < 0) return -1;
    }

    if (masked) {
        for (std::size_t i = 0; i < static_cast<std::size_t>(plen); i++)
            frame_buf_[i] ^= mask[i & 3];
    }

    frame_len_ = static_cast<std::size_t>(plen);

    if (opcode == 0x8) return 0;
    if (opcode == 0x9) {
        uint8_t pong[2] = {0x8A, 0x00};
        SSL_write(ssl_, pong, 2);
        return 0;
    }
    if (opcode == 0xA) return 0;

    (void)fin;
    return static_cast<int>(plen);
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

    // A2: Set running_ immediately so request_stop() on another thread is
    // always honoured regardless of where in the connect→receive cycle we are.
    running_.store(true, std::memory_order_relaxed);
    uint32_t backoff_s = 1; // A2: exponential backoff state (1→2→4→…→30 s)

    while (running_.load(std::memory_order_relaxed)) {
        // ── Connection phase (startup — allocates via OpenSSL/getaddrinfo) ──
        state_.store(ListenerState::INITIALIZING, std::memory_order_relaxed);

        connect_attempts_.fetch_add(1, std::memory_order_relaxed);
        bool connect_ok = perform_tls_connect();
        if (connect_ok && !perform_websocket_handshake()) {
            cleanup_connection();
            connect_ok = false;
        }

        if (!connect_ok) {
            state_.store(ListenerState::NON_OPERATIONAL_MULTICAST_FAILED,
                         std::memory_order_relaxed);
            if (!config_.auto_reconnect) return; // single-attempt callers exit here
            // auto_reconnect=true: fall through to backoff
        } else {
            send_subscribe_message();
            ws_connected_ = true;
            connect_count_.fetch_add(1, std::memory_order_relaxed);
            state_.store(ListenerState::OPERATIONAL, std::memory_order_relaxed);
            backoff_s = 1; // reset backoff on successful connection

            // ── Hot receive loop (zero allocation) ──
            bool terminal_error = false;
            uint64_t last_frame_rx_ns = 0;

            while (running_.load(std::memory_order_relaxed)) {
                const uint64_t recv_start_ns = monotonic_raw_ns();
                int n = recv_frame();
                const uint64_t recv_end_ns = monotonic_raw_ns();
                const uint64_t recv_wait_ns =
                    recv_end_ns >= recv_start_ns
                        ? (recv_end_ns - recv_start_ns)
                        : 0;



                // D4: timestamp acquired immediately after frame data is available.
                const uint64_t rx_timestamp_ns = recv_end_ns;

                if (n > 0) {
                    // BUG-FIX (2026-09-20): evaluate staleness against the
                    // PREVIOUS last_recv_time_ns_ BEFORE process_payload()
                    // overwrites it.  A receive gap > 500 ms was invisible
                    // when the frame arriving after the gap was the first
                    // thing to update last_recv_time_ns_.
                    //
                    // Ordering contract (must be preserved):
                    //   1. receive complete frame
                    //   2. capture rx_timestamp_ns  (done above)
                    //   3. check_staleness(rx_timestamp_ns)   ← HERE
                    //   4. update frame-gap diagnostics
                    //   5. update last_frame_rx_ns
                    //   6. process_payload(…)   ← overwrites last_recv_time_ns_
                    //   7. arrival callback
                    ingestor_.check_staleness(rx_timestamp_ns);

                    if (last_frame_rx_ns != 0 &&
                        rx_timestamp_ns >= last_frame_rx_ns) {
                        const uint64_t frame_gap_ns =
                            rx_timestamp_ns - last_frame_rx_ns;

                        if (frame_gap_ns >= 400'000'000ULL) {
                            std::fprintf(
                                stderr,
                                "[FRAME-GAP-DIAG] gap_ms=%llu prev_rx_ns=%llu rx_ns=%llu size=%d\n",
                                static_cast<unsigned long long>(
                                    frame_gap_ns / 1'000'000ULL),
                                static_cast<unsigned long long>(
                                    last_frame_rx_ns),
                                static_cast<unsigned long long>(
                                    rx_timestamp_ns),
                                n);
                        }
                    }

                    last_frame_rx_ns = rx_timestamp_ns;

                    // Forward raw JSON payload to FeedIngestor.
                    // FeedIngestor::process_payload is network-protocol-agnostic.
                    ingestor_.process_payload(frame_buf_,
                                              static_cast<std::size_t>(n),
                                              rx_timestamp_ns);
                    // Instrumentation callback (nullptr in production; set by harnesses).
                    // Invoked with the already-captured CLOCK_MONOTONIC_RAW timestamp.
                    if (frame_arrival_cb_) frame_arrival_cb_(rx_timestamp_ns);
                } else if (n == kRecvTimeout || n == 0) {
                    // A3: SO_RCVTIMEO fired, or graceful close / control frame.
                    // Both cases: check stale-feed threshold and re-enter.
                    ingestor_.check_staleness(rx_timestamp_ns);
                    if (!running_.load(std::memory_order_relaxed)) break;
                } else {
                    // n < 0 and n != kRecvTimeout: terminal connection error.
                    if (!running_.load(std::memory_order_relaxed)) break;
                    terminal_error = true;
                    break;
                }
            }

            cleanup_connection();

            if (terminal_error) {
                if (!config_.auto_reconnect) {
                    state_.store(ListenerState::FAULTED, std::memory_order_relaxed);
                    return;
                }
                // auto_reconnect=true: fall through to backoff and retry.
            } else {
                // Clean exit: request_stop() called or peer closed gracefully.
                return;
            }
        }

        // A2: Backoff before reconnect. Sleep 1 s at a time so request_stop()
        // remains responsive throughout the wait period.
        for (uint32_t i = 0;
             i < backoff_s && running_.load(std::memory_order_relaxed); ++i) {
            ::sleep(1);
        }
        if (backoff_s < 30u) backoff_s = std::min(backoff_s * 2u, 30u);
    }
}

} // namespace egbos::l0
