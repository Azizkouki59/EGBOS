#!/usr/bin/env python3
"""
DIAGNOSTIC -- LOCAL_SYNTHETIC / not for qualification use.

Streams live Binance candlestick (kline) data via raw TLS WebSocket.
Prints in-progress candles and explicitly flags when a candle closes.

Uses only Python stdlib (socket + ssl + json) — no third-party packages.
Read-only. Does not touch governance/, does not invoke qualification.
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
from datetime import datetime

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


def _recv_exact(ssock: ssl.SSLSocket, n: int) -> bytes | None:
    buf = bytearray()
    while len(buf) < n:
        chunk = ssock.recv(n - len(buf))
        if not chunk: return None
        buf.extend(chunk)
    return bytes(buf)

def _recv_frame(ssock: ssl.SSLSocket) -> bytes | None:
    """Read one WebSocket frame; handle ping/pong/close. Returns payload bytes."""
    header = _recv_exact(ssock, 2)
    if not header: return None

    opcode = header[0] & 0x0F
    payload_len = header[1] & 0x7F

    if opcode == 0x08:          # Close
        return None
    if opcode == 0x09:          # Ping → reply with Pong
        ssock.sendall(b"\x8A\x00")
        return _recv_frame(ssock)
    if opcode == 0x0A:          # Pong (unsolicited)
        return _recv_frame(ssock)

    if payload_len == 126:
        ext = _recv_exact(ssock, 2)
        if not ext: return None
        payload_len = struct.unpack("!H", ext)[0]
    elif payload_len == 127:
        ext = _recv_exact(ssock, 8)
        if not ext: return None
        payload_len = struct.unpack("!Q", ext)[0]

    payload = _recv_exact(ssock, payload_len)
    if not payload: return None

    return bytes(payload)

# ── Main streaming loop ─────────────────────────────────────────────────────

def stream_klines(symbol: str, interval: str, duration: int) -> None:
    host = "stream.binance.com"
    port = 9443
    path = f"/ws/{symbol.lower()}@kline_{interval}"

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

    print(f"[*] Connected. Streaming {symbol.upper()} ({interval}) klines for {duration}s (Ctrl+C to stop)...")
    print("-" * 110)

    start_time = time.monotonic()
    update_count = 0

    try:
        while time.monotonic() - start_time < duration:
            raw = _recv_frame(ssock)
            if raw is None:
                print("\n[-] Connection closed by server.")
                break
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue

            if msg.get("e") != "kline":
                continue

            k = msg.get("k", {})
            if not k:
                continue

            o_time = datetime.fromtimestamp(k["t"] / 1000).strftime("%Y-%m-%d %H:%M:%S")
            o = float(k["o"])
            h = float(k["h"])
            l = float(k["l"])
            c = float(k["c"])
            v = float(k["v"])
            is_closed = k["x"]

            sym_fmt = f"{symbol.upper():<8} ({interval})"
            prices = f"O: {o:>9.2f}  H: {h:>9.2f}  L: {l:>9.2f}  C: {c:>9.2f}"
            vol = f"Vol: {v:>10.4f}"

            if is_closed:
                print(f"\n{'='*20} CANDLE CLOSED {'='*20}")
                print(f"{sym_fmt} [{o_time}]  {prices}  {vol}")
                print(f"{'='*55}\n", flush=True)
            else:
                print(f"\r{sym_fmt} [{o_time}]  {prices}  {vol}  | IN_PROGRESS                             ", end='', flush=True)
            
            update_count += 1

    except KeyboardInterrupt:
        print("\n\n[*] Stopped by user (Ctrl+C).")
    finally:
        ssock.close()

    elapsed = time.monotonic() - start_time
    print(f"\n[*] {update_count} updates received in {elapsed:.1f}s")
    print("[*] DIAGNOSTIC -- LOCAL_SYNTHETIC / not for qualification use.")

def main() -> None:
    p = argparse.ArgumentParser(description="Live Binance kline/candlestick viewer.")
    p.add_argument("--symbol", default="btcusdt", help="Trading pair symbol (default: btcusdt)")
    p.add_argument("--interval", default="1m", help="Kline interval (default: 1m, e.g., 5m, 1h)")
    p.add_argument("--duration", type=int, default=0, help="Seconds to run (default: unlimited)")
    args = p.parse_args()

    duration = args.duration if args.duration > 0 else 10**9
    stream_klines(args.symbol, args.interval, duration)

if __name__ == "__main__":
    main()
