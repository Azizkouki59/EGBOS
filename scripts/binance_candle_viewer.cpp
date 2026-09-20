// scripts/binance_candle_viewer.cpp
// EGBOS — Standalone Binance Kline / Candle Terminal Viewer
//
// Purpose   : Live read-only diagnostic tool that streams OHLCV kline data
//             from Binance production WebSocket and displays it in the terminal.
//
// Endpoint  : wss://stream.binance.com:9443/ws/<symbol>@kline_<interval>
//
// Usage     : binance_candle_viewer [--symbol <sym>] [--interval <i>] [--duration <s>]
//   --symbol   <sym>  : Binance symbol, lowercase (default: btcusdt)
//   --interval <i>    : Kline interval: 1m 5m 15m 1h 4h 1d etc. (default: 1m)
//   --duration <s>    : Run for this many seconds (0 = until Ctrl+C, default: 0)
//
// Constraints:
//   - Read-only public market data. No auth, no order code.
//   - Fully standalone: does NOT link engine_l0/l1/core, LobEngine,
//     FeedIngestor, or L1Consumer. Zero dependency on EGBOS engine internals.
//   - Self-contained TLS+WebSocket client using OpenSSL + BSD sockets,
//     modelled on BinanceFeedListener's approach.
//   - Do NOT touch scripts/s5_live_binance_qualification.cpp.
//   - Do NOT write to governance/.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>

#if defined(__linux__) || defined(__APPLE__)
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

// ─── Global stop flag ────────────────────────────────────────────────────────

static std::atomic<bool> g_stop{false};

static void on_sigint(int) {
    g_stop.store(true, std::memory_order_relaxed);
}

// ─── Kline data ───────────────────────────────────────────────────────────────

struct Kline {
    int64_t open_time_ms{0};  // t
    char    open[32]{};       // o
    char    high[32]{};       // h
    char    low[32]{};        // l
    char    close[32]{};      // c
    char    volume[32]{};     // v
    bool    is_closed{false}; // x
};

// ─── Minimal hand-rolled JSON extractor ──────────────────────────────────────
//
// The kline message schema is narrow and stable; no external JSON library
// is needed.  We search for each field key and extract the raw value token.
//
// Kline sub-object keys of interest (inside "k": { ... }):
//   "t":<int64>, "o":"<str>", "h":"<str>", "l":"<str>",
//   "c":"<str>", "v":"<str>", "x":<bool>
//
// Returns true when the string value of key is copied into `out`.
// Returns false if the key is not found.

static bool extract_str_field(std::string_view json,
                              std::string_view key,
                              char* out, std::size_t out_size) {
    // Search for "key": in the JSON
    char search[64];
    int  slen = ::snprintf(search, sizeof(search), "\"%.*s\":",
                           static_cast<int>(key.size()), key.data());
    if (slen <= 0) return false;

    const char* p = ::strstr(json.data(), search);
    if (!p) return false;
    p += slen;

    // Skip optional whitespace
    while (*p == ' ' || *p == '\t') ++p;

    bool quoted = (*p == '"');
    if (quoted) ++p;

    const char* start = p;
    while (*p && (quoted ? (*p != '"') : (*p != ',' && *p != '}' && *p != ' ')))
        ++p;

    std::size_t len = std::min(static_cast<std::size_t>(p - start), out_size - 1);
    ::memcpy(out, start, len);
    out[len] = '\0';
    return true;
}

static bool extract_kline(std::string_view msg, Kline& k) {
    // The kline payload is nested under "k": { ... }
    const char* kp = ::strstr(msg.data(), "\"k\":{");
    if (!kp) return false;

    // Work with the sub-object slice for all field extractions
    std::string_view sub{kp, static_cast<std::size_t>(msg.end() - kp)};

    char buf[32];

    // t — open time (int64)
    if (extract_str_field(sub, "t", buf, sizeof(buf)))
        k.open_time_ms = static_cast<int64_t>(::strtoll(buf, nullptr, 10));

    extract_str_field(sub, "o", k.open,   sizeof(k.open));
    extract_str_field(sub, "h", k.high,   sizeof(k.high));
    extract_str_field(sub, "l", k.low,    sizeof(k.low));
    extract_str_field(sub, "c", k.close,  sizeof(k.close));
    extract_str_field(sub, "v", k.volume, sizeof(k.volume));

    // x — is_closed (true/false)
    if (extract_str_field(sub, "x", buf, sizeof(buf)))
        k.is_closed = (::strcmp(buf, "true") == 0);

    return true;
}

// ─── Terminal rendering ───────────────────────────────────────────────────────

// ANSI colour helpers — degrade gracefully if terminal doesn't support them.
static constexpr const char* RESET  = "\033[0m";
static constexpr const char* BOLD   = "\033[1m";
static constexpr const char* GREEN  = "\033[32m";
static constexpr const char* YELLOW = "\033[33m";
static constexpr const char* CYAN   = "\033[36m";
static constexpr const char* WHITE  = "\033[97m";
static constexpr const char* DIM    = "\033[2m";
static constexpr const char* RED    = "\033[31m";

// Pretty-print open_time_ms as HH:MM:SS UTC
static void format_time(int64_t ms, char* buf, std::size_t n) {
    time_t secs = static_cast<time_t>(ms / 1000);
    struct tm t{};
#if defined(__linux__) || defined(__APPLE__)
    ::gmtime_r(&secs, &t);
#else
    t = *::gmtime(&secs);
#endif
    ::snprintf(buf, n, "%02d:%02d:%02d UTC", t.tm_hour, t.tm_min, t.tm_sec);
}

// Overwriting live-update line (carriage return, no newline)
static void print_live(const char* symbol, const char* interval,
                       const Kline& k, int updates) {
    char timebuf[32];
    format_time(k.open_time_ms, timebuf, sizeof(timebuf));

    // Determine direction colour (close vs open)
    const char* dir_col = GREEN;
    if (k.close[0] && k.open[0]) {
        double c = ::atof(k.close);
        double o = ::atof(k.open);
        if (c < o) dir_col = RED;
    }

    ::printf("\r%s%s%s [%s|%s]  "
             "O:%s%s%s  H:%s%s%s  L:%s%s%s  C:%s%s%s%s  "
             "V:%s%.4s%s  %s#%d%s   ",
             BOLD, CYAN, symbol,
             interval, timebuf,
             WHITE, k.open,  RESET,
             GREEN, k.high,  RESET,
             RED,   k.low,   RESET,
             dir_col, BOLD, k.close, RESET,
             DIM, k.volume, RESET,
             DIM, updates, RESET);
    ::fflush(stdout);
}

// Closed-candle block — printed on a fresh line with extra decoration
static void print_closed(const char* symbol, const char* interval,
                         const Kline& k, int candles) {
    char timebuf[32];
    format_time(k.open_time_ms, timebuf, sizeof(timebuf));

    const char* dir_col = GREEN;
    const char* dir_str = "▲";
    if (k.close[0] && k.open[0]) {
        double c = ::atof(k.close);
        double o = ::atof(k.open);
        if (c < o) { dir_col = RED; dir_str = "▼"; }
    }

    ::printf("\n");
    ::printf("%s\xe2\x95\x94%s%s\n", CYAN,
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x97", RESET);
    {
        char row[256];
        ::snprintf(row, sizeof(row),
                   "  CLOSED CANDLE #%-3d  %s%-6s %-5s  %s%-16s",
                   candles, BOLD, symbol, interval, DIM, timebuf);
        ::printf("%s\xe2\x95\x91%s%s\xe2\x95\x91%s\n", CYAN, row, CYAN, RESET);
    }
    ::printf("%s\xe2\x95\xa0%s%s\n", CYAN,
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\xa3", RESET);
    {
        char row[256];
        ::snprintf(row, sizeof(row), "  Open   %s%-14s%s  High  %s%-14s",
                   WHITE, k.open, RESET, GREEN, k.high);
        ::printf("%s\xe2\x95\x91%s%s\xe2\x95\x91%s\n", CYAN, row, CYAN, RESET);
    }
    {
        char row[256];
        ::snprintf(row, sizeof(row), "  Low    %s%-14s%s  Close %s%s%-14s",
                   RED, k.low, RESET, dir_col, BOLD, k.close);
        ::printf("%s\xe2\x95\x91%s%s\xe2\x95\x91%s\n", CYAN, row, CYAN, RESET);
    }
    {
        char row[64];
        ::snprintf(row, sizeof(row), "  Volume %-22s", k.volume);
        ::printf("%s\xe2\x95\x91%s%s\xe2\x95\x91%s\n", CYAN, row, CYAN, RESET);
    }
    ::printf("%s\xe2\x95\x9a%s%s\n", CYAN,
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
             "\xe2\x95\x9d", RESET);
    (void)dir_str;
    ::fflush(stdout);
}

// ─── Self-contained TLS + WebSocket client ────────────────────────────────────

static constexpr const char* kHost     = "stream.binance.com";
static constexpr uint16_t    kPort     = 9443;
static constexpr std::size_t kRecvBuf  = 8192;
static constexpr std::size_t kFrameBuf = 65536;

struct WsClient {
    SSL_CTX* ctx{nullptr};
    SSL*     ssl{nullptr};
    int      fd{-1};

    uint8_t recv_buf[kRecvBuf]{};
    uint8_t frame_buf[kFrameBuf]{};

    bool init() {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        const SSL_METHOD* m = TLS_client_method();
        ctx = SSL_CTX_new(m);
        if (!ctx) return false;
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_default_verify_paths(ctx);
        return true;
    }

    bool connect(const char* host, uint16_t port) {
        struct addrinfo hints{};
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        char port_str[8];
        ::snprintf(port_str, sizeof(port_str), "%u", port);

        struct addrinfo* res = nullptr;
        if (::getaddrinfo(host, port_str, &hints, &res) != 0 || !res)
            return false;

        fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0) { ::freeaddrinfo(res); return false; }

        if (::connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
            ::freeaddrinfo(res); ::close(fd); fd = -1; return false;
        }
        ::freeaddrinfo(res);

        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, fd);
        SSL_set_tlsext_host_name(ssl, host);
        if (SSL_connect(ssl) != 1) { cleanup(); return false; }
        return true;
    }

    bool handshake(const char* host, const char* path) {
        // Random 16-byte key → base64
        static const char b64[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        uint8_t kb[16];
        RAND_bytes(kb, 16);
        char ws_key[25]{};
        for (int i = 0; i < 5; i++) {
            uint32_t n = (uint32_t(kb[i*3])<<16)|(uint32_t(kb[i*3+1])<<8)|kb[i*3+2];
            ws_key[i*4+0] = b64[(n>>18)&63];
            ws_key[i*4+1] = b64[(n>>12)&63];
            ws_key[i*4+2] = b64[(n>>6) &63];
            ws_key[i*4+3] = b64[(n)    &63];
        }
        uint32_t n = uint32_t(kb[15]) << 16;
        ws_key[20] = b64[(n>>18)&63];
        ws_key[21] = b64[(n>>12)&63];
        ws_key[22] = '='; ws_key[23] = '='; ws_key[24] = '\0';

        char req[1024];
        int  rlen = ::snprintf(req, sizeof(req),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: %s\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "User-Agent: EGBOS-CandleViewer/1.0\r\n"
            "\r\n",
            path, host, ws_key);
        if (SSL_write(ssl, req, rlen) != rlen) return false;

        char resp[2048]{};
        int  rd = SSL_read(ssl, resp, sizeof(resp)-1);
        if (rd <= 0) return false;
        return (::strstr(resp, "101") && ::strstr(resp, "Upgrade"));
    }

    // Returns payload bytes in frame_buf, 0 = closed/control, <0 = error.
    int recv_frame() {
        uint8_t hdr[2];
        if (SSL_read(ssl, hdr, 2) <= 0) return -1;

        uint8_t  opcode = hdr[0] & 0x0F;
        bool     masked = (hdr[1] & 0x80) != 0;
        uint64_t plen   =  hdr[1] & 0x7F;

        if (plen == 126) {
            uint8_t ext[2];
            if (SSL_read(ssl, ext, 2) != 2) return -1;
            plen = (uint64_t(ext[0])<<8) | ext[1];
        } else if (plen == 127) {
            uint8_t ext[8];
            if (SSL_read(ssl, ext, 8) != 8) return -1;
            plen = 0;
            for (int i = 0; i < 8; i++) plen = (plen<<8)|ext[i];
        }

        uint8_t mask[4]{};
        if (masked && SSL_read(ssl, mask, 4) != 4) return -1;

        // Oversized-frame guard: drain the payload off the socket before returning
        // so the TLS stream stays byte-aligned for the next frame.  Returning -1
        // early (without reading the payload) would leave `plen` bytes on the
        // socket; the next SSL_read would then misinterpret them as a new frame
        // header — the same class of bug as Finding 2 in binance_feed_listener.cpp.
        // Kline frames are always small in practice, so this path is purely
        // defensive; it does not affect normal operation.
        if (plen > kFrameBuf) {
            // Discard loop: read and throw away up to plen bytes.
            constexpr std::size_t kDiscardChunk = 4096;
            uint8_t discard[kDiscardChunk];
            uint64_t remaining = plen;
            while (remaining > 0) {
                int want = static_cast<int>(std::min(remaining,
                                           static_cast<uint64_t>(kDiscardChunk)));
                int rd = SSL_read(ssl, discard, want);
                if (rd <= 0) return -1;  // connection lost mid-drain; nothing we can do
                remaining -= static_cast<uint64_t>(rd);
            }
            return -1;  // frame was too large — skip it; connection stays aligned
        }

        std::size_t total = 0;
        while (total < static_cast<std::size_t>(plen)) {
            int rd = SSL_read(ssl, frame_buf+total,
                              static_cast<int>(plen-total));
            if (rd <= 0) return rd;
            total += static_cast<std::size_t>(rd);
        }
        if (masked)
            for (std::size_t i = 0; i < total; i++) frame_buf[i] ^= mask[i&3];

        frame_buf[total] = '\0'; // null-terminate for strstr safety

        if (opcode == 0x8) return 0;  // Close
        if (opcode == 0x9) {          // Ping → Pong
            // Send masked pong (client→server must be masked per RFC 6455)
            uint8_t pong[6] = {0x8A, 0x80, 0x00, 0x00, 0x00, 0x00};
            SSL_write(ssl, pong, sizeof(pong));
            return 0;
        }
        if (opcode == 0xA) return 0;  // Pong — ignore
        return static_cast<int>(total);
    }

    void cleanup() {
        if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); ssl = nullptr; }
        if (fd >= 0) { ::close(fd); fd = -1; }
    }

    void teardown() {
        cleanup();
        if (ctx) { SSL_CTX_free(ctx); ctx = nullptr; }
        EVP_cleanup();
    }
};

// ─── Entry point ──────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
#if defined(__linux__) || defined(__APPLE__)
    ::signal(SIGPIPE, SIG_IGN);
    ::signal(SIGINT,  on_sigint);
    ::signal(SIGTERM, on_sigint);
#endif

    // ── Parse CLI arguments ──────────────────────────────────────────────
    std::string symbol   = "btcusdt";
    std::string interval = "1m";
    int         duration = 0;   // seconds, 0 = infinite

    for (int i = 1; i < argc; ++i) {
        if (::strcmp(argv[i], "--symbol") == 0 && i+1 < argc)
            symbol = argv[++i];
        else if (::strcmp(argv[i], "--interval") == 0 && i+1 < argc)
            interval = argv[++i];
        else if (::strcmp(argv[i], "--duration") == 0 && i+1 < argc)
            duration = ::atoi(argv[++i]);
        else if (::strcmp(argv[i], "--help") == 0 || ::strcmp(argv[i], "-h") == 0) {
            ::printf(
                "Usage: binance_candle_viewer [OPTIONS]\n\n"
                "  --symbol   <sym>  Binance symbol (default: btcusdt)\n"
                "  --interval <i>    Kline interval: 1m 5m 15m 1h 4h 1d (default: 1m)\n"
                "  --duration <s>    Run seconds (0=until Ctrl+C, default: 0)\n\n"
                "Examples:\n"
                "  binance_candle_viewer\n"
                "  binance_candle_viewer --symbol ethusdt --interval 5m\n"
                "  binance_candle_viewer --symbol btcusdt --interval 1h --duration 120\n"
            );
            return 0;
        }
    }

    // Normalise symbol to lowercase
    for (char& c : symbol) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

    // Build WebSocket path: /ws/<symbol>@kline_<interval>
    char ws_path[128];
    ::snprintf(ws_path, sizeof(ws_path), "/ws/%s@kline_%s",
               symbol.c_str(), interval.c_str());

    // ── Banner ───────────────────────────────────────────────────────────
    std::string dur_str = (duration > 0) ? std::to_string(duration) + "s" : "until Ctrl+C";
    ::printf("%s╔══════════════════════════════════════════════════════╗%s\n", CYAN, RESET);
    ::printf("%s║       EGBOS Binance Candle Viewer                    ║%s\n", CYAN, RESET);
    ::printf("%s╠══════════════════════════════════════════════════════╣%s\n", CYAN, RESET);
    ::printf("%s║  Host    : %-41s║%s\n", CYAN, kHost,             RESET);
    ::printf("%s║  Path    : %-41s║%s\n", CYAN, ws_path,           RESET);
    ::printf("%s║  Symbol  : %-41s║%s\n", CYAN, symbol.c_str(),    RESET);
    ::printf("%s║  Interval: %-41s║%s\n", CYAN, interval.c_str(),  RESET);
    ::printf("%s║  Duration: %-41s║%s\n", CYAN, dur_str.c_str(),   RESET);
    ::printf("%s╚══════════════════════════════════════════════════════╝%s\n\n", CYAN, RESET);

    // ── Connect ──────────────────────────────────────────────────────────
    WsClient ws;
    if (!ws.init()) {
        ::fprintf(stderr, "ERROR: OpenSSL init failed\n");
        return 1;
    }

    ::printf("%sConnecting to %s:%u ...%s\n", DIM, kHost, kPort, RESET);
    if (!ws.connect(kHost, kPort)) {
        ::fprintf(stderr, "ERROR: TCP/TLS connect failed — check network.\n");
        ws.teardown();
        return 1;
    }
    ::printf("%sTLS connected.%s\n", DIM, RESET);

    if (!ws.handshake(kHost, ws_path)) {
        ::fprintf(stderr, "ERROR: WebSocket handshake failed.\n");
        ws.teardown();
        return 1;
    }
    ::printf("%sWebSocket open. Streaming %s@kline_%s ...%s\n\n",
             GREEN, symbol.c_str(), interval.c_str(), RESET);

    // ── Receive loop ─────────────────────────────────────────────────────
    auto start_time = std::chrono::steady_clock::now();

    int updates       = 0;  // updates within current candle
    int closed_candles = 0; // total closed candles seen

    while (!g_stop.load(std::memory_order_relaxed)) {
        // Duration check
        if (duration > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::steady_clock::now() - start_time).count();
            if (elapsed >= static_cast<long long>(duration)) break;
        }

        int n = ws.recv_frame();
        if (n < 0) {
            ::printf("\n%sConnection error — exiting.%s\n", RED, RESET);
            break;
        }
        if (n == 0) continue; // control frame

        std::string_view msg{reinterpret_cast<const char*>(ws.frame_buf),
                             static_cast<std::size_t>(n)};

        Kline k{};
        if (!extract_kline(msg, k)) continue;

        ++updates;

        if (k.is_closed) {
            ++closed_candles;
            print_closed(symbol.c_str(), interval.c_str(), k, closed_candles);
            updates = 0;
        } else {
            print_live(symbol.c_str(), interval.c_str(), k, updates);
        }
    }

    ::printf("\n%sStopped. Closed candles seen: %d%s\n",
             CYAN, closed_candles, RESET);

    ws.teardown();
    return 0;
}
