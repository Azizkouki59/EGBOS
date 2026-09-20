// scripts/egbos_binance_cli.cpp
// EGBOS Binance Market Data CLI
// Classification: DEVELOPMENT_TEST / DIAGNOSTIC
// Constraints: Read-only CLI, zero-order-path, real Binance frames.

#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cctype>

#if defined(__linux__)
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#include <nlohmann/json.hpp>

#include "engine/l1/lob_engine.h"
#include "engine/l1/depth_parser.h"
#include "engine/core/fixed_point.h"

using json = nlohmann::json;
using namespace egbos::l1;
using namespace egbos::core;

enum class SyncState {
    DISCONNECTED,
    CONNECTING,
    BUFFERING,
    SNAPSHOT_FETCH,
    SYNCING,
    SYNCHRONIZED,
    DEGRADED
};

struct TickerInfo {
    int64_t bid_price = 0;
    int64_t bid_qty = 0;
    int64_t ask_price = 0;
    int64_t ask_qty = 0;
};

struct TradeInfo {
    int64_t price = 0;
    int64_t qty = 0;
    bool is_buyer_maker = false;
};

#if defined(__linux__)
class TlsConnection {
    int fd_ = -1;
    SSL_CTX* ctx_ = nullptr;
    SSL* ssl_ = nullptr;
public:
    TlsConnection() {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        ctx_ = SSL_CTX_new(TLS_client_method());
        SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
    }
    ~TlsConnection() {
        close();
        if (ctx_) SSL_CTX_free(ctx_);
    }
    bool connect(const std::string& host, int port) {
        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) return false;
        fd_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd_ < 0) { freeaddrinfo(res); return false; }
        
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        if (::connect(fd_, res->ai_addr, res->ai_addrlen) != 0) { freeaddrinfo(res); close(); return false; }
        freeaddrinfo(res);

        ssl_ = SSL_new(ctx_);
        SSL_set_fd(ssl_, fd_);
        SSL_set_tlsext_host_name(ssl_, host.c_str());
        if (SSL_connect(ssl_) != 1) { close(); return false; }
        return true;
    }
    void close() {
        if (ssl_) { SSL_shutdown(ssl_); SSL_free(ssl_); ssl_ = nullptr; }
        if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
    }
    int read(void* buf, int num) {
        return SSL_read(ssl_, buf, num);
    }
    int write(const void* buf, int num) {
        return SSL_write(ssl_, buf, num);
    }
};

class WsClient {
    TlsConnection conn_;
public:
    bool connect(const std::string& host, const std::string& path) {
        if (!conn_.connect(host, 443)) return false;
        std::string req = "GET " + path + " HTTP/1.1\r\n"
                          "Host: " + host + "\r\n"
                          "Upgrade: websocket\r\n"
                          "Connection: Upgrade\r\n"
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                          "Sec-WebSocket-Version: 13\r\n\r\n";
        conn_.write(req.data(), req.size());
        char buf[4096];
        int n = conn_.read(buf, sizeof(buf));
        if (n <= 0) return false;
        std::string resp(buf, n);
        return resp.find("101 Switching Protocols") != std::string::npos;
    }
    
    std::string read_frame() {
        std::string payload;
        while (true) {
            uint8_t head[2];
            int r = conn_.read(head, 2);
            if (r <= 0) return "";
            bool fin = (head[0] & 0x80) != 0;
            uint8_t op = head[0] & 0x0F;
            uint64_t len = head[1] & 0x7F;
            if (len == 126) {
                uint8_t ext[2];
                if (conn_.read(ext, 2) != 2) return "";
                len = (ext[0] << 8) | ext[1];
            } else if (len == 127) {
                uint8_t ext[8];
                if (conn_.read(ext, 8) != 8) return "";
                len = 0;
                for (int i=0; i<8; i++) len = (len << 8) | ext[i];
            }
            std::vector<uint8_t> buf(len);
            uint64_t got = 0;
            while (got < len) {
                r = conn_.read(buf.data() + got, len - got);
                if (r <= 0) return "";
                got += r;
            }
            if (op == 0x9) { // Ping
                uint8_t pong[2] = {0x8A, 0x00};
                conn_.write(pong, 2);
                continue;
            }
            if (op == 0x8) return ""; // Close
            payload.append(reinterpret_cast<char*>(buf.data()), len);
            if (fin) break;
        }
        return payload;
    }
};

int64_t parse_fixed(const std::string& s) {
    try {
        return static_cast<int64_t>(std::stod(s) * egbos::l1::kPriceScale + (std::stod(s) > 0 ? 0.5 : -0.5));
    } catch (...) {
        return 0;
    }
}

ParsedDepthUpdate fetch_snapshot(const std::string& symbol) {
    TlsConnection conn;
    ParsedDepthUpdate out{};
    out.first_update_id = 0;
    out.final_update_id = 0;
    if (!conn.connect("api.binance.com", 443)) return out;
    std::string req = "GET /api/v3/depth?symbol=" + symbol + "&limit=100 HTTP/1.1\r\n"
                      "Host: api.binance.com\r\n"
                      "Connection: close\r\n\r\n";
    conn.write(req.data(), req.size());
    std::string resp;
    char buf[8192];
    while (true) {
        int n = conn.read(buf, sizeof(buf));
        if (n <= 0) break;
        resp.append(buf, n);
    }
    size_t body_pos = resp.find("\r\n\r\n");
    if (body_pos != std::string::npos) {
        std::string body = resp.substr(body_pos + 4);
        
        std::string lower_resp = resp;
        for (auto& c : lower_resp) c = std::tolower(c);
        if (lower_resp.find("transfer-encoding: chunked") != std::string::npos) {
            std::string unchunked;
            size_t pos = 0;
            while (pos < body.size()) {
                size_t crlf = body.find("\r\n", pos);
                if (crlf == std::string::npos) break;
                std::string hex_len = body.substr(pos, crlf - pos);
                size_t len = 0;
                try { len = std::stoull(hex_len, nullptr, 16); } catch(...) { break; }
                if (len == 0) break;
                pos = crlf + 2;
                if (pos + len > body.size()) break;
                unchunked.append(body.substr(pos, len));
                pos += len + 2;
            }
            body = unchunked;
        }
        
        size_t start = body.find('{');
        size_t end = body.rfind('}');
        if (start != std::string::npos && end != std::string::npos && end >= start) {
            std::string json_str = body.substr(start, end - start + 1);
            try {
                json j = json::parse(json_str);
                out.final_update_id = j["lastUpdateId"].get<uint64_t>();
                
                out.bid_count = 0;
                for (auto& item : j["bids"]) {
                    if (out.bid_count >= egbos::l1::kMaxUpdatesPerPacket) break;
                    out.bids[out.bid_count].price = parse_fixed(item[0].get<std::string>());
                    out.bids[out.bid_count].qty = parse_fixed(item[1].get<std::string>());
                    out.bid_count++;
                }
                out.ask_count = 0;
                for (auto& item : j["asks"]) {
                    if (out.ask_count >= egbos::l1::kMaxUpdatesPerPacket) break;
                    out.asks[out.ask_count].price = parse_fixed(item[0].get<std::string>());
                    out.asks[out.ask_count].qty = parse_fixed(item[1].get<std::string>());
                    out.ask_count++;
                }
            } catch (...) {}
        }
    }
    return out;
}
#endif

// Globals
std::atomic<SyncState> g_state{SyncState::DISCONNECTED};
std::unique_ptr<LobEngine> g_lob;
TickerInfo g_ticker;
TradeInfo g_trade;
std::mutex g_mut;

std::deque<std::string> g_buffer;
ParsedDepthUpdate g_snapshot;
std::atomic<bool> g_snapshot_ready{false};
std::atomic<bool> g_snapshot_fetching{false};
std::atomic<bool> g_running{true};

uint64_t g_frames = 0;
uint64_t g_gaps = 0;

bool sync_buffer_replay(LobEngine* lob, uint64_t& last_id, const std::deque<std::string>& buffer) {
    bool synced = false;
    for (const auto& ev : buffer) {
        ParsedDepthUpdate p;
        if (parse_depth_update(reinterpret_cast<const uint8_t*>(ev.data()), ev.size(), &p) != ParseResult::OK) {
            return false;
        }
        if (p.final_update_id <= last_id) continue;
        
        if (!synced) {
            if (p.first_update_id <= last_id + 1 && p.final_update_id >= last_id + 1) {
                synced = true;
                lob->apply_depth_update(p);
                last_id = p.final_update_id;
            } else {
                return false; // Gap bridging failed
            }
        } else {
            if (p.first_update_id <= last_id + 1 && p.final_update_id >= last_id + 1) {
                lob->apply_depth_update(p);
                last_id = p.final_update_id;
            } else {
                return false; // Gap detected during buffer replay
            }
        }
    }
    return synced;
}

void run_ws(std::string symbol) {
#if defined(__linux__)
    std::string lower_sym = symbol;
    for (auto& c : lower_sym) c = std::tolower(c);
    std::string stream_path = "/stream?streams=" + lower_sym + "@depth@100ms/" + lower_sym + "@bookTicker/" + lower_sym + "@trade";

    while (g_running) {
        g_state = SyncState::CONNECTING;
        WsClient ws;
        if (!ws.connect("stream.binance.com", stream_path)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        g_state = SyncState::BUFFERING;
        g_buffer.clear();
        g_snapshot_ready = false;
        g_snapshot_fetching = false;
        
        if (!g_snapshot_fetching.exchange(true)) {
            std::thread([symbol]() {
                g_state = SyncState::SNAPSHOT_FETCH;
                ParsedDepthUpdate snap = fetch_snapshot(symbol);
                std::lock_guard<std::mutex> lock(g_mut);
                g_snapshot = snap;
                g_snapshot_ready = true;
                g_snapshot_fetching = false;
            }).detach();
        }

        while (g_running) {
            std::string msg = ws.read_frame();
            if (msg.empty()) break;
            
            try {
                json j = json::parse(msg);
                if (!j.contains("stream") || !j.contains("data")) continue;
                std::string stream = j["stream"].get<std::string>();
                json data = j["data"];
                
                std::lock_guard<std::mutex> lock(g_mut);
                
                if (stream.find("@bookTicker") != std::string::npos) {
                    g_ticker.bid_price = parse_fixed(data["b"].get<std::string>());
                    g_ticker.bid_qty = parse_fixed(data["B"].get<std::string>());
                    g_ticker.ask_price = parse_fixed(data["a"].get<std::string>());
                    g_ticker.ask_qty = parse_fixed(data["A"].get<std::string>());
                } else if (stream.find("@trade") != std::string::npos) {
                    g_trade.price = parse_fixed(data["p"].get<std::string>());
                    g_trade.qty = parse_fixed(data["q"].get<std::string>());
                    g_trade.is_buyer_maker = data["m"].get<bool>();
                } else if (stream.find("@depth") != std::string::npos) {
                    std::string inner_json = data.dump();
                    ParsedDepthUpdate upd;
                    if (parse_depth_update(reinterpret_cast<const uint8_t*>(inner_json.data()), inner_json.size(), &upd) != ParseResult::OK) {
                        continue;
                    }
                    g_frames++;
                    
                    if (g_state == SyncState::BUFFERING || g_state == SyncState::SNAPSHOT_FETCH) {
                        g_buffer.push_back(inner_json);
                        if (g_snapshot_ready) g_state = SyncState::SYNCING;
                    } 
                    
                    if (g_state == SyncState::SYNCING) {
                        g_buffer.push_back(inner_json);
                        uint64_t last_id = g_snapshot.final_update_id;
                        
                        if (g_buffer.empty()) continue;
                        ParsedDepthUpdate first_buf;
                        parse_depth_update(reinterpret_cast<const uint8_t*>(g_buffer.front().data()), g_buffer.front().size(), &first_buf);
                        
                        if (last_id < first_buf.first_update_id) {
                            g_state = SyncState::SNAPSHOT_FETCH;
                            g_snapshot_ready = false;
                            if (!g_snapshot_fetching.exchange(true)) {
                                std::thread([symbol]() {
                                    ParsedDepthUpdate snap = fetch_snapshot(symbol);
                                    std::lock_guard<std::mutex> lock(g_mut);
                                    g_snapshot = snap;
                                    g_snapshot_ready = true;
                                    g_snapshot_fetching = false;
                                }).detach();
                            }
                            continue;
                        }
                        
                        if (g_snapshot.bid_count > 0 || g_snapshot.ask_count > 0) {
                            int64_t anchor = g_snapshot.bid_count > 0 ? g_snapshot.bids[0].price : g_snapshot.asks[0].price;
                            g_lob = std::make_unique<LobEngine>(anchor, 100000LL);
                            g_lob->apply_depth_update(g_snapshot);
                            
                            bool synced = sync_buffer_replay(g_lob.get(), last_id, g_buffer);
                            for (const auto& ev : g_buffer) {
                                ParsedDepthUpdate p;
                                if (parse_depth_update(reinterpret_cast<const uint8_t*>(ev.data()), ev.size(), &p) != ParseResult::OK) {
                                    synced = false;
                                    break;
                                }
                                if (p.final_update_id <= last_id) continue;
                                
                                if (!synced) {
                                    if (p.first_update_id <= last_id + 1 && p.final_update_id >= last_id + 1) {
                                        synced = true;
                                        g_lob->apply_depth_update(p);
                                        last_id = p.final_update_id;
                                    } else {
                                        break; // Gap bridging failed
                                    }
                                } else {
                                    if (p.first_update_id <= last_id + 1 && p.final_update_id >= last_id + 1) {
                                        g_lob->apply_depth_update(p);
                                        last_id = p.final_update_id;
                                    } else {
                                        synced = false;
                                        break; // Gap detected during buffer replay
                                    }
                                }
                            }
                            
                            if (synced) {
                                g_buffer.clear();
                                g_state = SyncState::SYNCHRONIZED;
                            } else {
                                g_state = SyncState::SNAPSHOT_FETCH;
                                g_buffer.clear();
                                g_snapshot_ready = false;
                                if (!g_snapshot_fetching.exchange(true)) {
                                    std::thread([symbol]() {
                                        ParsedDepthUpdate snap = fetch_snapshot(symbol);
                                        std::lock_guard<std::mutex> lock(g_mut);
                                        g_snapshot = snap;
                                        g_snapshot_ready = true;
                                        g_snapshot_fetching = false;
                                    }).detach();
                                }
                            }
                        } else {
                            // Empty/failed snapshot. Retrigger fetch.
                            g_state = SyncState::SNAPSHOT_FETCH;
                            g_buffer.clear();
                            g_snapshot_ready = false;
                            if (!g_snapshot_fetching.exchange(true)) {
                                std::thread([symbol]() {
                                    ParsedDepthUpdate snap = fetch_snapshot(symbol);
                                    std::lock_guard<std::mutex> lock(g_mut);
                                    g_snapshot = snap;
                                    g_snapshot_ready = true;
                                    g_snapshot_fetching = false;
                                }).detach();
                            }
                        }
                    } else if (g_state == SyncState::SYNCHRONIZED) {
                        if (upd.first_update_id <= g_lob->last_update_id() + 1 && upd.final_update_id >= g_lob->last_update_id() + 1) {
                            g_lob->apply_depth_update(upd);
                        } else if (upd.first_update_id > g_lob->last_update_id() + 1) {
                            g_gaps++;
                            g_state = SyncState::DEGRADED;
                        }
                    } else if (g_state == SyncState::DEGRADED) {
                        g_state = SyncState::SNAPSHOT_FETCH;
                        g_buffer.clear();
                        g_snapshot_ready = false;
                        if (!g_snapshot_fetching.exchange(true)) {
                            std::thread([symbol]() {
                                ParsedDepthUpdate snap = fetch_snapshot(symbol);
                                std::lock_guard<std::mutex> lock(g_mut);
                                g_snapshot = snap;
                                g_snapshot_ready = true;
                                g_snapshot_fetching = false;
                            }).detach();
                        }
                    }
                }
            } catch (...) {}
        }
    }
#endif
}

void render_loop(int depth_limit, const std::string& format) {
    int64_t last_update_id = 0;
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::lock_guard<std::mutex> lock(g_mut);
        
        if (format == "ui") {
            std::cout << "\033[H\033[J";
            std::cout << "=== EGBOS BINANCE MARKET DATA CLI ===\n";
            std::cout << "State: ";
        switch (g_state.load()) {
            case SyncState::DISCONNECTED: std::cout << "DISCONNECTED"; break;
            case SyncState::CONNECTING: std::cout << "CONNECTING"; break;
            case SyncState::BUFFERING: std::cout << "BUFFERING"; break;
            case SyncState::SNAPSHOT_FETCH: std::cout << "SNAPSHOT_FETCH"; break;
            case SyncState::SYNCING: std::cout << "SYNCING"; break;
            case SyncState::SYNCHRONIZED: std::cout << "\033[32mSYNCHRONIZED\033[0m"; break;
            case SyncState::DEGRADED: std::cout << "\033[31mDEGRADED/RESYNC\033[0m"; break;
        }
        std::cout << "\nFrames: " << g_frames << " | Gaps: " << g_gaps << "\n";
        
        double s = egbos::l1::kPriceScale;
        
        std::cout << "\n[ LOB BBO Cross-Validation ]\n";
        if (g_lob && (g_state == SyncState::SYNCHRONIZED || g_state == SyncState::DEGRADED)) {
            std::cout << std::fixed << std::setprecision(2);
            double loc_bid = g_lob->best_bid_price() / s;
            double loc_ask = g_lob->best_ask_price() / s;
            double tic_bid = g_ticker.bid_price / s;
            double tic_ask = g_ticker.ask_price / s;
            
            std::cout << "Local LOB     : BID " << loc_bid << " | ASK " << loc_ask << "\n";
            std::cout << "Binance Ticker: BID " << tic_bid << " | ASK " << tic_ask << "\n";
            if (loc_bid == tic_bid && loc_ask == tic_ask) {
                std::cout << "Status        : \033[32mCONSISTENT\033[0m\n";
            } else {
                std::cout << "Status        : \033[33mINCONSISTENT (transient/delay)\033[0m\n";
            }
            std::cout << "Last Update u : " << g_lob->last_update_id() << "\n";
        } else {
            std::cout << "Awaiting synchronization...\n";
        }
        
        std::cout << "\n[ Last Trade ]\n";
        if (g_trade.price > 0) {
            std::cout << "Price: " << (g_trade.price / s) << " | Qty: " << (g_trade.qty / s) 
                      << " | Maker: " << (g_trade.is_buyer_maker ? "BUY" : "SELL") << "\n";
        }
        
        std::cout << "\n[ Order Book (Depth " << depth_limit << ") ]\n";
        if (g_lob && (g_state == SyncState::SYNCHRONIZED || g_state == SyncState::DEGRADED)) {
            std::vector<LobEngine::Level> bids, asks;
            g_lob->get_book_levels(bids, asks, depth_limit);
            
            std::cout << std::setw(15) << "ASK QTY" << " | " << std::setw(10) << "ASK PRICE" << "\n";
            std::cout << "--------------------------------\n";
            for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
                std::cout << "\033[31m" << std::setw(15) << (it->qty / s) << " | " << std::setw(10) << (it->price / s) << "\033[0m\n";
            }
            std::cout << "--------------------------------\n";
            for (const auto& b : bids) {
                std::cout << "\033[32m" << std::setw(15) << (b.qty / s) << " | " << std::setw(10) << (b.price / s) << "\033[0m\n";
            }
        }
        }
        
        if (format == "ui") {
            std::cout << std::flush;
        } else if (g_lob && (g_state == SyncState::SYNCHRONIZED || g_state == SyncState::DEGRADED)) {
            if (g_lob->last_update_id() != last_update_id) {
                last_update_id = g_lob->last_update_id();
                std::vector<LobEngine::Level> bids, asks;
                g_lob->get_book_levels(bids, asks, depth_limit);
                
                double s = egbos::l1::kPriceScale;
                
                if (format == "json") {
                    json j;
                    j["u"] = last_update_id;
                    j["bids"] = json::array();
                    j["asks"] = json::array();
                    for (const auto& b : bids) j["bids"].push_back(json::array({b.price / s, b.qty / s}));
                    for (const auto& a : asks) j["asks"].push_back(json::array({a.price / s, a.qty / s}));
                    std::cout << j.dump() << "\n";
                } else if (format == "csv") {
                    // output: u,bid1_p,bid1_q,ask1_p,ask1_q
                    double b1_p = bids.empty() ? 0 : bids[0].price / s;
                    double b1_q = bids.empty() ? 0 : bids[0].qty / s;
                    double a1_p = asks.empty() ? 0 : asks[0].price / s;
                    double a1_q = asks.empty() ? 0 : asks[0].qty / s;
                    std::cout << last_update_id << "," << std::fixed << std::setprecision(2) << b1_p << "," << b1_q << "," << a1_p << "," << a1_q << "\n";
                }
                std::cout << std::flush;
            }
        }
    }
}

int main(int argc, char** argv) {
    std::string symbol = "BTCUSDT";
    int depth = 10;
    std::string format = "ui";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--symbol" && i + 1 < argc) symbol = argv[++i];
        else if (arg == "--depth" && i + 1 < argc) depth = std::stoi(argv[++i]);
        else if (arg == "--format" && i + 1 < argc) format = argv[++i];
    }
    
    // If not ui, we don't need to print interactive headers at all
    if (format == "csv") {
        std::cout << "u,bid1_p,bid1_q,ask1_p,ask1_q\n";
    }
    
    std::thread ws_thread(run_ws, symbol);
    render_loop(depth, format);
    
    g_running = false;
    ws_thread.join();
    return 0;
}
