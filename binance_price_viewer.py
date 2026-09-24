#!/usr/bin/env python3
"""
DIAGNOSTIC -- LOCAL_SYNTHETIC / not for qualification use.

Streams live BTC/USDT (or any symbol) trade ticks from Binance via a raw
TLS WebSocket and prints each tick to the terminal in real time.

Uses only Python stdlib (socket + ssl + json) — no third-party packages.
Read-only. Does not touch governance/, does not invoke the qualification
pipeline, does not classify any output as LIVE_OPERATIONAL evidence.

Usage:
    python3 binance_price_viewer.py
    python3 binance_price_viewer.py --symbol ethusdt --duration 60
"""

import argparse
import base64
import json
import os
import socket
import ssl
import struct
import sys
import time


# ── WebSocket helpers (stdlib-only raw implementation) ──────────────────────

def _build_handshake(host: str, path: str) -> bytes:
    key = base64.b64encode(os.urandom(16)).decode()
    return (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: {host}\r\n"
        f"Upgrade: websocket\r\n"
        f"Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        f"Sec-WebSocket-Version: 13\r\n"
        f"\r\n"
    ).encode()


def _recv_frame(ssock: ssl.SSLSocket) -> bytes | None:
    """Read one WebSocket frame; handle ping/pong/close.  Returns payload bytes."""
    header = b""
    while len(header) < 2:
        chunk = ssock.recv(2 - len(header))
        if not chunk:
            return None
        header += chunk

    opcode = header[0] & 0x0F
    payload_len = header[1] & 0x7F

    if payload_len == 126:
        ext = ssock.recv(2)
        payload_len = struct.unpack("!H", ext)[0]
    elif payload_len == 127:
        ext = ssock.recv(8)
        payload_len = struct.unpack("!Q", ext)[0]

    payload = bytearray()
    while len(payload) < payload_len:
        chunk = ssock.recv(min(4096, payload_len - len(payload)))
        if not chunk:
            return None
        payload.extend(chunk)

    if opcode == 0x08:          # Close
        return None
    if opcode == 0x09:          # Ping → reply with Pong
        ssock.sendall(b"\x8A\x00")
        return _recv_frame(ssock)
    if opcode == 0x0A:          # Pong (unsolicited)
        return _recv_frame(ssock)

    return bytes(payload)


# ── Main streaming loop ─────────────────────────────────────────────────────

def stream_prices(symbol: str, duration: int) -> None:
    host = "stream.binance.com"
    port = 9443
    path = f"/ws/{symbol.lower()}@trade"

    print("[*] DIAGNOSTIC -- LOCAL_SYNTHETIC / not for qualification use.")
    print(f"[*] Connecting to wss://{host}:{port}{path}")

    sock = socket.create_connection((host, port), timeout=10)
    ctx = ssl.create_default_context()
    ssock = ctx.wrap_socket(sock, server_hostname=host)
    ssock.settimeout(10)

    ssock.sendall(_build_handshake(host, path))
    response = b""
    while b"\r\n\r\n" not in response:
        chunk = ssock.recv(4096)
        if not chunk:
            print("[-] Connection closed during handshake.", file=sys.stderr)
            return
        response += chunk

    if b"101 Switching Protocols" not in response:
        print(f"[-] Unexpected HTTP response:\n{response.decode(errors='replace')}", file=sys.stderr)
        return

    print(f"[*] Connected. Streaming {symbol.upper()} trades for {duration}s (Ctrl+C to stop)...")
    print(f"{'Time':>12}  {'Price (USDT)':>16}  {'Qty':>14}  {'Side':>4}")
    print("-" * 55)

    start = time.monotonic()
    trade_count = 0

    try:
        while time.monotonic() - start < duration:
            raw = _recv_frame(ssock)
            if raw is None:
                print("\n[-] Connection closed by server.")
                break
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue

            # Binance @trade stream fields:
            #   E = event time (ms), p = price, q = quantity, m = is_buyer_maker
            if msg.get("e") != "trade":
                continue

            price = float(msg["p"])
            qty   = float(msg["q"])
            side  = "SELL" if msg.get("m") else "BUY "
            ts    = time.strftime("%H:%M:%S", time.localtime(msg["E"] / 1000))

            # Print each tick immediately (flush=True for real-time display)
            print(f"{ts:>12}  {price:>16.2f}  {qty:>14.6f}  {side}", flush=True)
            trade_count += 1

    except KeyboardInterrupt:
        print("\n[*] Stopped by user (Ctrl+C).")
    finally:
        ssock.close()

    elapsed = time.monotonic() - start
    print(f"\n[*] {trade_count} trades received in {elapsed:.1f}s "
          f"({trade_count/elapsed:.1f} ticks/s)")
    print("[*] DIAGNOSTIC -- LOCAL_SYNTHETIC / not for qualification use.")


# ── Entry point ─────────────────────────────────────────────────────────────

def main() -> None:
    p = argparse.ArgumentParser(
        description="Print live Binance trade ticks to the terminal."
    )
    p.add_argument("--symbol",   default="btcusdt",
                   help="Trading pair symbol (default: btcusdt)")
    p.add_argument("--duration", type=int, default=30,
                   help="How many seconds to stream (default: 30). Use 0 for unlimited.")
    args = p.parse_args()

    duration = args.duration if args.duration > 0 else 10**9
    stream_prices(args.symbol, duration)


if __name__ == "__main__":
    main()
