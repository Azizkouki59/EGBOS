// engine/l0/binance_feed_listener.h
// EGBOS ENGINE_V3 — Binance WebSocket L0 Feed Listener
//
// Specification  : S5-REQ-001 (Live Feed Integration), MARKET-DATA-001
// Governance     : EGBOS-GOV-DECISION-2026-009, D4, D7, D9
// Constraints    : Zero dynamic allocation on the hot receive path (GC-E-010).
//                  Strictly read-only network layer. No outbound order path.
//                  CLOCK_MONOTONIC_RAW timestamp acquired at WebSocket frame receive.
//
// Provenance:
//   FeedProvenance::DEVELOPMENT_TEST  — when connected to Binance Testnet
//   FeedProvenance::EXTERNAL_LIVE     — when connected to Binance Production (pre sign-off)
//   FeedProvenance::PRODUCTION_VERIFIED — after MARKET-DATA-001 countersignature
//
// Governance invariant (D9):
//   LOCAL_SYNTHETIC != LIVE_OPERATIONAL
//   Testnet traffic MUST NOT be promoted to LIVE_OPERATIONAL evidence.

#pragma once

#include "ingestor.h"
#include "feed_source.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <atomic>
#include <functional>

#if defined(__linux__)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

namespace egbos::l0 {

// ──────────────────────────────────────────────────────────────────────
// BinanceFeedConfig
// ──────────────────────────────────────────────────────────────────────
struct BinanceFeedConfig {
    // Whether to connect to Binance testnet (DEVELOPMENT_TEST)
    // or production (EXTERNAL_LIVE → PRODUCTION_VERIFIED after sign-off).
    bool use_testnet{true};

    // WebSocket host/port (filled from MARKET-DATA-001 endpoints)
    const char* host{"testnet.binance.vision"};
    const char* path{"/ws"};
    uint16_t    port{443};

    // Symbols to subscribe to (null-terminated, comma-separated combined stream path)
    // e.g. "btcusdt@depth/ethusdt@depth/btcusdt@bookTicker"
    const char* stream_path{nullptr};

    // Reconnect on disconnect
    bool auto_reconnect{true};

    // Stale feed threshold (nanoseconds). Default: 500ms per GC-E-012.
    uint64_t stale_threshold_ns{500'000'000ULL};
};

// ──────────────────────────────────────────────────────────────────────
// BinanceFeedListener
//
// Connects to the Binance WebSocket market data feed, receives frames,
// and passes raw payload bytes to FeedIngestor::process_payload with
// a CLOCK_MONOTONIC_RAW timestamp acquired immediately on frame receive.
//
// Hot-path receive loop is allocation-free. The TLS/WebSocket handshake
// allocates via OpenSSL during startup only.
// ──────────────────────────────────────────────────────────────────────
class BinanceFeedListener {
public:
    friend class BinanceWSProtocolTest;

    explicit BinanceFeedListener(FeedIngestor& ingestor, const BinanceFeedConfig& config);
    ~BinanceFeedListener();

    // Blocking hot-path receive loop.
    // Returns when request_stop() is called or a terminal error occurs.
    void run_loop() noexcept;

    // Signal-safe stop request.
    void request_stop() noexcept;

    // Optional frame-arrival callback.
    // When set, cb is invoked with the CLOCK_MONOTONIC_RAW timestamp
    // immediately after each data frame is dispatched to process_payload.
    // Invoked on the hot receive path — must be noexcept and allocation-free.
    // Classification: instrumentation only; does NOT affect production logic.
    void set_frame_arrival_callback(std::function<void(uint64_t)> cb) noexcept {
        frame_arrival_cb_ = std::move(cb);
    }

    ListenerState   state()      const noexcept { return state_.load(std::memory_order_relaxed); }
    FeedProvenance  provenance() const noexcept { return provenance_; }

    // Telemetry: track connection attempts and successful OPERATIONAL transitions.
    uint64_t connect_attempts() const noexcept { return connect_attempts_.load(std::memory_order_relaxed); }
    uint64_t connect_count()    const noexcept { return connect_count_.load(std::memory_order_relaxed); }

    // D9: classification accessor for operational logging.
    const char* provenance_str() const noexcept;

    // Read-only accessors for the current WebSocket frame payload.
    // Valid only from within a frame_arrival_cb invocation.
    // Classification: instrumentation/diagnostic only.
    const uint8_t* frame_buf() const noexcept { return frame_buf_; }
    std::size_t    frame_len() const noexcept { return frame_len_; }

private:
    // WebSocket frame receive — returns bytes read, 0 on graceful close, <0 on error.
    int  recv_frame() noexcept;
    bool send_subscribe_message() noexcept;
    bool perform_tls_connect() noexcept;
    bool perform_websocket_handshake() noexcept;
    void cleanup_connection() noexcept;

    // D4: CLOCK_MONOTONIC_RAW timestamp helper.
    static uint64_t monotonic_raw_ns() noexcept;

    FeedIngestor&              ingestor_;
    BinanceFeedConfig          config_;
    FeedProvenance             provenance_;
    std::atomic<ListenerState> state_{ListenerState::INITIALIZING};
    std::atomic<bool>          running_{false};
    std::atomic<uint64_t>      connect_attempts_{0};
    std::atomic<uint64_t>      connect_count_{0};

    // TCP socket fd
    int fd_{-1};

    // OpenSSL state (startup/connection only; not touched in hot receive path)
#if defined(__linux__)
    SSL_CTX* ssl_ctx_{nullptr};
    SSL*     ssl_{nullptr};

    // A4: Handshake-only test seam for deterministic application-byte chunks.
    // Used exclusively inside perform_websocket_handshake(). The hot-path
    // recv_frame() loop always calls ::SSL_read() directly.
    int (*injected_ssl_read_)(SSL*, void*, int) = nullptr;
#endif

    // Hot-path receive buffer (D8: 8192 per MARKET-DATA-001 specification)
    static constexpr std::size_t kRecvBufSize = 8192;
    uint8_t recv_buf_[kRecvBufSize]{};

    // WebSocket frame assembly buffer (handles fragmented frames)
    // 18 KB frames observed on production Binance @depth (1000-level snapshots).
    // 128 KB gives a 7x safety margin; still fits in a single L2 cache miss budget.
    static constexpr std::size_t kFrameBufSize = 131072;
    uint8_t frame_buf_[kFrameBufSize]{};
    std::size_t frame_len_{0};

    // A4 pending-buffer invariants (enforced on every connection transition):
    //   pending_rx_offset_ <= pending_rx_len_ <= kPendingBufSize
    // Reset to (0,0) at handshake start and in cleanup_connection().
    static constexpr std::size_t kPendingBufSize = 65536;
    uint8_t pending_rx_buf_[kPendingBufSize]{};
    std::size_t pending_rx_len_{0};
    std::size_t pending_rx_offset_{0};

    // A4: Maximum HTTP response bytes accumulated during the opening handshake.
    static constexpr std::size_t kMaxHandshakeSize = 8192;

    // R4 NEW POLICY DECISION — not existing repository governance.
    // No ADR/claim currently authorizes 10485760 (10 MiB). This is an
    // implementation resource/security bound, separate from RFC 6455.
    // Protocol-invalid (MSB=1 in the 64-bit length field) is independent.
    static constexpr uint64_t kMaxDrainSize = 10485760;

    // WebSocket masking key (client→server frames must be masked per RFC 6455)
    static constexpr std::size_t kMaskKeySize = 4;
    uint8_t mask_key_[kMaskKeySize]{};

    // WebSocket state
    bool ws_connected_{false};

    // Optional instrumentation callback (nullptr when not set).
    std::function<void(uint64_t)> frame_arrival_cb_;
};

} // namespace egbos::l0
