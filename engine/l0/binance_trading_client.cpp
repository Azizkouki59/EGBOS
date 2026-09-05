// engine/l0/binance_trading_client.cpp
// EGBOS ENGINE_V3 — Binance Trading API Client Implementation
//
// Specification  : MARKET-DATA-001 §trading_api
// Governance     : D7 (zero-order-path separation from L0 feed)
//
// Credentials:
//   Read from environment only.
//   Testnet:    BINANCE_TESTNET_API_KEY / BINANCE_TESTNET_API_SECRET
//   Production: BINANCE_PROD_API_KEY   / BINANCE_PROD_API_SECRET

#include "binance_trading_client.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

#if defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/hmac.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#endif

namespace egbos::l0 {

// ──────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ──────────────────────────────────────────────────────────────────────
BinanceTradingClient::BinanceTradingClient(bool use_testnet)
    : use_testnet_(use_testnet)
{
    provenance_ = use_testnet_
                  ? FeedProvenance::DEVELOPMENT_TEST
                  : FeedProvenance::EXTERNAL_LIVE;

    rest_host_         = use_testnet_ ? "testnet.binance.vision" : "api.binance.com";
    rest_path_order_   = "/api/v3/order";

#if defined(__linux__)
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    const SSL_METHOD* method = TLS_client_method();
    ssl_ctx_ = SSL_CTX_new(method);
    if (ssl_ctx_) {
        SSL_CTX_set_min_proto_version(ssl_ctx_, TLS1_2_VERSION);
        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_default_verify_paths(ssl_ctx_);
    }
#endif
}

BinanceTradingClient::~BinanceTradingClient() {
#if defined(__linux__)
    if (ssl_ctx_) { SSL_CTX_free(ssl_ctx_); ssl_ctx_ = nullptr; }
    EVP_cleanup();
#endif
}

// ──────────────────────────────────────────────────────────────────────
// init — load credentials from environment
// ──────────────────────────────────────────────────────────────────────
bool BinanceTradingClient::init() noexcept {
    const char* key_env    = use_testnet_ ? "BINANCE_TESTNET_API_KEY"    : "BINANCE_PROD_API_KEY";
    const char* secret_env = use_testnet_ ? "BINANCE_TESTNET_API_SECRET" : "BINANCE_PROD_API_SECRET";

    const char* key    = ::getenv(key_env);
    const char* secret = ::getenv(secret_env);

    if (!key || !secret || key[0] == '\0' || secret[0] == '\0') {
        ::fprintf(stderr,
            "[BinanceTradingClient] ERROR: %s / %s not set in environment.\n"
            "  Credentials MUST be environment variables. Never commit API keys.\n",
            key_env, secret_env);
        return false;
    }

    ::strncpy(api_key_,    key,    sizeof(api_key_)    - 1);
    ::strncpy(api_secret_, secret, sizeof(api_secret_) - 1);
    api_key_[sizeof(api_key_)-1]       = '\0';
    api_secret_[sizeof(api_secret_)-1] = '\0';

    ready_ = true;
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// unix_ms — current Unix timestamp in milliseconds (for Binance timestamp param)
// ──────────────────────────────────────────────────────────────────────
int64_t BinanceTradingClient::unix_ms() noexcept {
    struct timespec ts{};
#if defined(__linux__)
    ::clock_gettime(CLOCK_REALTIME, &ts);   // Binance requires wall-clock (API timestamp)
#endif
    return static_cast<int64_t>(ts.tv_sec) * 1000LL
         + static_cast<int64_t>(ts.tv_nsec) / 1'000'000LL;
}

// ──────────────────────────────────────────────────────────────────────
// sign — HMAC-SHA256 hex signature per Binance API spec
// ──────────────────────────────────────────────────────────────────────
bool BinanceTradingClient::sign(const char* payload, std::size_t len,
                                char* sig_buf, std::size_t sig_buf_len) const noexcept {
    if (sig_buf_len < 65) return false;
#if defined(__linux__)
    uint8_t digest[32];
    unsigned int dlen = sizeof(digest);
    HMAC(EVP_sha256(),
         api_secret_, static_cast<int>(::strlen(api_secret_)),
         reinterpret_cast<const uint8_t*>(payload), len,
         digest, &dlen);
    for (unsigned int i = 0; i < dlen; i++) {
        ::snprintf(sig_buf + i*2, 3, "%02x", digest[i]);
    }
    sig_buf[64] = '\0';
    return true;
#else
    (void)payload; (void)len;
    sig_buf[0] = '\0';
    return false;
#endif
}

// ──────────────────────────────────────────────────────────────────────
// https_request — minimal HTTPS REST helper
// ──────────────────────────────────────────────────────────────────────
bool BinanceTradingClient::https_request(const char* method,
                                          const char* host,
                                          const char* path,
                                          const char* body,
                                          char* resp_buf,
                                          std::size_t resp_buf_len) noexcept {
#if defined(__linux__)
    if (!ssl_ctx_) return false;

    // TCP connect
    struct addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    if (::getaddrinfo(host, "443", &hints, &res) != 0 || !res) return false;

    int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { ::freeaddrinfo(res); return false; }
    if (::connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
        ::close(fd); ::freeaddrinfo(res); return false;
    }
    ::freeaddrinfo(res);

    // TLS
    SSL* ssl = SSL_new(ssl_ctx_);
    SSL_set_fd(ssl, fd);
    SSL_set_tlsext_host_name(ssl, host);
    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl); ::close(fd); return false;
    }

    // Build HTTP request
    std::size_t body_len = body ? ::strlen(body) : 0;
    char req[4096];
    int  rlen;
    if (body_len > 0) {
        rlen = ::snprintf(req, sizeof(req),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "X-MBX-APIKEY: %s\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            method, path, host, api_key_, body_len, body);
    } else {
        rlen = ::snprintf(req, sizeof(req),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "X-MBX-APIKEY: %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path, host, api_key_);
    }

    SSL_write(ssl, req, rlen);

    // Read response
    std::size_t total = 0;
    int rd;
    while ((rd = SSL_read(ssl, resp_buf + total,
                          static_cast<int>(resp_buf_len - total - 1))) > 0) {
        total += static_cast<std::size_t>(rd);
        if (total >= resp_buf_len - 1) break;
    }
    resp_buf[total] = '\0';

    SSL_shutdown(ssl); SSL_free(ssl); ::close(fd);
    return total > 0;
#else
    (void)method; (void)host; (void)path; (void)body;
    (void)resp_buf; (void)resp_buf_len;
    return false;
#endif
}

// ──────────────────────────────────────────────────────────────────────
// place_order
// ──────────────────────────────────────────────────────────────────────
bool BinanceTradingClient::place_order(const OrderRequest& req,
                                        OrderResult& result_out) noexcept {
    if (!ready_) { ::strncpy(result_out.error_msg, "NOT_INITIALIZED", 255); return false; }

    int64_t ts = unix_ms();

    // Build query string
    char query[1024];
    int qlen;
    if (req.price && req.time_in_force) {
        qlen = ::snprintf(query, sizeof(query),
            "symbol=%s&side=%s&type=%s&timeInForce=%s&quantity=%s&price=%s&timestamp=%lld&recvWindow=%llu",
            req.symbol,
            req.side == OrderSide::BUY ? "BUY" : "SELL",
            req.type,
            req.time_in_force,
            req.quantity,
            req.price,
            static_cast<long long>(ts),
            static_cast<unsigned long long>(req.recv_window_ms));
    } else {
        qlen = ::snprintf(query, sizeof(query),
            "symbol=%s&side=%s&type=%s&quantity=%s&timestamp=%lld&recvWindow=%llu",
            req.symbol,
            req.side == OrderSide::BUY ? "BUY" : "SELL",
            req.type,
            req.quantity,
            static_cast<long long>(ts),
            static_cast<unsigned long long>(req.recv_window_ms));
    }

    // Sign
    char sig[65];
    if (!sign(query, static_cast<std::size_t>(qlen), sig, sizeof(sig))) {
        ::strncpy(result_out.error_msg, "SIGNING_FAILED", 255);
        return false;
    }

    // Append signature
    char body[1280];
    ::snprintf(body, sizeof(body), "%s&signature=%s", query, sig);

    // POST
    char path[256];
    ::snprintf(path, sizeof(path), "%s", rest_path_order_);

    if (!https_request("POST", rest_host_, path, body, resp_buf_, kRespBufSize)) {
        ::strncpy(result_out.error_msg, "HTTP_REQUEST_FAILED", 255);
        return false;
    }

    // Minimal JSON parse for orderId / status
    const char* id_pos = ::strstr(resp_buf_, "\"orderId\":");
    if (id_pos) {
        result_out.order_id = ::atoll(id_pos + 10);
        result_out.accepted = true;
    }
    const char* status_pos = ::strstr(resp_buf_, "\"status\":\"");
    if (status_pos) {
        const char* s = status_pos + 10;
        const char* e = ::strchr(s, '"');
        if (e) {
            std::size_t slen = static_cast<std::size_t>(e - s);
            if (slen >= sizeof(result_out.status)) slen = sizeof(result_out.status) - 1;
            ::memcpy(result_out.status, s, slen);
            result_out.status[slen] = '\0';
        }
    }
    if (!result_out.accepted) {
        ::strncpy(result_out.error_msg, resp_buf_, 255);
    }
    return result_out.accepted;
}

// ──────────────────────────────────────────────────────────────────────
// cancel_order
// ──────────────────────────────────────────────────────────────────────
bool BinanceTradingClient::cancel_order(const char* symbol, int64_t order_id,
                                         OrderResult& result_out) noexcept {
    if (!ready_) { ::strncpy(result_out.error_msg, "NOT_INITIALIZED", 255); return false; }

    int64_t ts = unix_ms();
    char query[512];
    int  qlen = ::snprintf(query, sizeof(query),
        "symbol=%s&orderId=%lld&timestamp=%lld",
        symbol, static_cast<long long>(order_id), static_cast<long long>(ts));

    char sig[65];
    if (!sign(query, static_cast<std::size_t>(qlen), sig, sizeof(sig))) {
        ::strncpy(result_out.error_msg, "SIGNING_FAILED", 255);
        return false;
    }

    char path[512];
    ::snprintf(path, sizeof(path), "%s?%s&signature=%s",
               rest_path_order_, query, sig);

    if (!https_request("DELETE", rest_host_, path, nullptr, resp_buf_, kRespBufSize)) {
        ::strncpy(result_out.error_msg, "HTTP_REQUEST_FAILED", 255);
        return false;
    }

    result_out.accepted = (::strstr(resp_buf_, "\"orderId\"") != nullptr);
    if (!result_out.accepted)
        ::strncpy(result_out.error_msg, resp_buf_, 255);
    return result_out.accepted;
}

// ──────────────────────────────────────────────────────────────────────
// get_account
// ──────────────────────────────────────────────────────────────────────
bool BinanceTradingClient::get_account(char* buf, std::size_t buf_len) noexcept {
    if (!ready_) return false;

    int64_t ts = unix_ms();
    char query[128];
    int  qlen = ::snprintf(query, sizeof(query), "timestamp=%lld",
                           static_cast<long long>(ts));

    char sig[65];
    if (!sign(query, static_cast<std::size_t>(qlen), sig, sizeof(sig))) return false;

    char path[256];
    ::snprintf(path, sizeof(path), "/api/v3/account?%s&signature=%s", query, sig);

    return https_request("GET", rest_host_, path, nullptr, buf, buf_len);
}

} // namespace egbos::l0
