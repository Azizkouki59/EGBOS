// engine/l0/binance_trading_client.h
// EGBOS ENGINE_V3 — Binance Trading API Client
//
// Specification  : MARKET-DATA-001 §trading_api
// Governance     : EGBOS-GOV-DECISION-2026-009 D7 (zero-order-path on L0 feed)
//
// IMPORTANT ARCHITECTURAL CONSTRAINT:
//   This client is SEPARATE from the L0 read-only feed path.
//   BinanceFeedListener = RECEIVE ONLY (zero-order-path compliant).
//   BinanceTradingClient = SEND/ORDER (wired to execution layer, NOT L0).
//
// Credentials:
//   All API keys are read from environment variables ONLY.
//   NEVER store keys in source code or committed configuration.
//   Testnet:    BINANCE_TESTNET_API_KEY / BINANCE_TESTNET_API_SECRET
//   Production: BINANCE_PROD_API_KEY   / BINANCE_PROD_API_SECRET
//
// Classification:
//   Testnet orders: DEVELOPMENT_TEST — not production evidence
//   Production orders: LIVE_OPERATIONAL (after MARKET-DATA-001 countersignature)

#pragma once

#include "feed_source.h"
#include <cstdint>
#include <cstddef>
#include <string>

#if defined(__linux__)
#include <openssl/ssl.h>
#include <openssl/hmac.h>
#endif

namespace egbos::l0 {

// ──────────────────────────────────────────────────────────────────────
// Order side
// ──────────────────────────────────────────────────────────────────────
enum class OrderSide : uint8_t { BUY = 0, SELL = 1 };

// ──────────────────────────────────────────────────────────────────────
// OrderRequest
// ──────────────────────────────────────────────────────────────────────
struct OrderRequest {
    const char* symbol;       // e.g. "BTCUSDT"
    OrderSide   side;         // BUY / SELL
    const char* type;         // "LIMIT" / "MARKET"
    const char* quantity;     // e.g. "0.001"
    const char* price;        // e.g. "50000.00" (LIMIT only; nullptr for MARKET)
    const char* time_in_force;// "GTC" / "IOC" / "FOK" (LIMIT only)
    uint64_t    recv_window_ms{5000};
};

// ──────────────────────────────────────────────────────────────────────
// OrderResult
// ──────────────────────────────────────────────────────────────────────
struct OrderResult {
    bool        accepted{false};
    int64_t     order_id{-1};
    char        client_order_id[64]{};
    char        status[32]{};
    char        error_msg[256]{};
};

// ──────────────────────────────────────────────────────────────────────
// BinanceTradingClient
//
// HMAC-SHA256 signed REST/WebSocket order placement.
// Separate from the L0 read-only feed listener (D7 zero-order-path invariant).
// ──────────────────────────────────────────────────────────────────────
class BinanceTradingClient {
public:
    explicit BinanceTradingClient(bool use_testnet = true);
    ~BinanceTradingClient();

    // Returns false if API credentials are not available in environment.
    bool init() noexcept;

    // Place a new order. Blocking REST call.
    // Returns false if the environment is testnet and use_testnet=false, or vice versa.
    bool place_order(const OrderRequest& req, OrderResult& result_out) noexcept;

    // Cancel an order by symbol + order ID.
    bool cancel_order(const char* symbol, int64_t order_id, OrderResult& result_out) noexcept;

    // Query account balances (REST GET /api/v3/account).
    // Writes JSON response into buf (null-terminated), returns false on error.
    bool get_account(char* buf, std::size_t buf_len) noexcept;

    FeedProvenance provenance() const noexcept { return provenance_; }
    bool           is_ready()   const noexcept { return ready_; }

private:
    // HMAC-SHA256 signing per Binance API spec.
    // Writes hex-encoded signature into sig_buf (must be >= 65 bytes).
    bool sign(const char* payload, std::size_t len,
              char* sig_buf, std::size_t sig_buf_len) const noexcept;

    // HTTPS REST helper — sends request, reads response into buf.
    bool https_request(const char* method,
                       const char* host,
                       const char* path,
                       const char* body,
                       char* resp_buf,
                       std::size_t resp_buf_len) noexcept;

    // Return current Unix timestamp in milliseconds.
    static int64_t unix_ms() noexcept;

    bool           use_testnet_;
    bool           ready_{false};
    FeedProvenance provenance_;

    // API credentials (loaded from environment at init())
    char api_key_[256]{};
    char api_secret_[256]{};

    // Resolved endpoints
    const char* rest_host_{nullptr};
    const char* rest_path_order_{nullptr};

#if defined(__linux__)
    SSL_CTX* ssl_ctx_{nullptr};
#endif

    // Response buffer (used internally)
    static constexpr std::size_t kRespBufSize = 16384;
    char resp_buf_[kRespBufSize]{};
};

} // namespace egbos::l0
