#!/usr/bin/env python3
import socket
import ssl
import base64
import os
import struct
import time
import argparse
import sys
import math

def build_handshake(host, path):
    key = base64.b64encode(os.urandom(16)).decode('utf-8')
    req = (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: {host}\r\n"
        f"Upgrade: websocket\r\n"
        f"Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        f"Sec-WebSocket-Version: 13\r\n"
        f"\r\n"
    )
    return req.encode('utf-8')

def parse_frame(ssock):
    header = ssock.recv(2)
    if not header or len(header) < 2:
        return None
    
    # We record the exact monotonic time as soon as the first bytes of the frame arrive
    rx_time_ns = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)

    opcode = header[0] & 0x0F
    
    if opcode == 0x08:  # Close
        return None
    if opcode == 0x09:  # Ping
        ssock.sendall(b'\x8A\x00')
        return parse_frame(ssock)
    if opcode == 0x0A:  # Pong
        return parse_frame(ssock)

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
        
    return rx_time_ns, payload

def compute_stats(intervals_ns):
    if not intervals_ns:
        return 0, 0, 0, 0
    min_ns = min(intervals_ns)
    max_ns = max(intervals_ns)
    mean_ns = sum(intervals_ns) / len(intervals_ns)
    variance = sum((x - mean_ns) ** 2 for x in intervals_ns) / len(intervals_ns)
    stddev_ns = math.sqrt(variance)
    return min_ns, max_ns, mean_ns, stddev_ns

def measure_jitter(symbol, duration_sec):
    host = "stream.binance.com"
    port = 9443
    stream_name = f"{symbol.lower()}@trade"
    path = f"/ws/{stream_name}"
    
    print(f"[*] DIAGNOSTIC — LOCAL_SYNTHETIC / not for qualification use.")
    print(f"[*] Connecting to wss://{host}:{port}{path}")
    
    sock = socket.create_connection((host, port))
    context = ssl.create_default_context()
    ssock = context.wrap_socket(sock, server_hostname=host)
    
    ssock.sendall(build_handshake(host, path))
    resp = ssock.recv(4096)
    if b"101 Switching Protocols" not in resp:
        print("[-] Handshake failed!")
        return

    print(f"[*] Handshake successful. Recording frame jitter for {duration_sec}s (or Ctrl+C to stop)...")

    start_time = time.time()
    timestamps_ns = []
    
    try:
        while time.time() - start_time < duration_sec:
            res = parse_frame(ssock)
            if res is None:
                print("[-] Connection closed by server.")
                break
            
            rx_time_ns, _ = res
            timestamps_ns.append(rx_time_ns)
                
    except KeyboardInterrupt:
        print("\n[*] Stopped by user (Ctrl+C).")
    finally:
        ssock.close()
        
    print(f"[*] Connection closed. Captured {len(timestamps_ns)} frames.")
    
    if len(timestamps_ns) < 2:
        print("[-] Not enough frames to compute jitter.")
        return
        
    intervals_ns = [timestamps_ns[i] - timestamps_ns[i-1] for i in range(1, len(timestamps_ns))]
    min_ns, max_ns, mean_ns, stddev_ns = compute_stats(intervals_ns)
    
    # Write raw timestamps to file
    out_file = f"jitter_raw_timestamps_{symbol}.csv"
    with open(out_file, "w") as f:
        f.write("# DIAGNOSTIC — LOCAL_SYNTHETIC / not for qualification use.\n")
        f.write("Frame,Timestamp_MONOTONIC_RAW_ns,IntervalToPrev_ns\n")
        f.write(f"1,{timestamps_ns[0]},0\n")
        for i, (ts, interval) in enumerate(zip(timestamps_ns[1:], intervals_ns)):
            f.write(f"{i+2},{ts},{interval}\n")
            
    print(f"[*] Wrote raw timestamps to {out_file}")
    print("\n" + "="*50)
    print("JITTER STATISTICS (inter-frame arrival time)")
    print("="*50)
    print(f"Frames analyzed : {len(timestamps_ns)}")
    print(f"Min Jitter      : {min_ns / 1_000_000:.3f} ms")
    print(f"Max Jitter      : {max_ns / 1_000_000:.3f} ms")
    print(f"Mean Jitter     : {mean_ns / 1_000_000:.3f} ms")
    print(f"StdDev Jitter   : {stddev_ns / 1_000_000:.3f} ms")
    print("="*50)
    print("DIAGNOSTIC — LOCAL_SYNTHETIC / not for qualification use.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Binance Standalone Jitter Diagnostic")
    parser.add_argument("--symbol", type=str, default="btcusdt", help="Symbol to stream (e.g. btcusdt)")
    parser.add_argument("--duration", type=int, default=300, help="Duration to run in seconds (default: 300 = 5 min)")
    args = parser.parse_args()
    
    measure_jitter(args.symbol, args.duration)
