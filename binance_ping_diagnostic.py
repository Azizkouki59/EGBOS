#!/usr/bin/env python3
"""
DIAGNOSTIC -- LOCAL_SYNTHETIC / not for qualification use.

Measures WebSocket ping/pong round-trip time at a fixed interval against the
Binance production endpoint, to isolate actual network-layer jitter from
market trade-arrival randomness (which a raw @trade stream conflates).

Read-only. Does not touch governance/, does not invoke the qualification
pipeline, does not classify any output as LIVE_OPERATIONAL evidence.
"""

import argparse
import csv
import statistics
import time

try:
    import websockets
    import asyncio
except ImportError:
    raise SystemExit(
        "Missing dependency. Install with: pip3 install websockets"
    )


async def run(symbol: str, duration: int, interval: float, out_csv: str):
    url = f"wss://stream.binance.com:9443/ws/{symbol}@trade"
    print("[*] DIAGNOSTIC -- LOCAL_SYNTHETIC / not for qualification use.")
    print(f"[*] Connecting to {url}")

    rtts_ms = []
    start = time.monotonic()

    async with websockets.connect(url, ping_interval=None) as ws:
        print(f"[*] Handshake successful. Sending pings every {interval}s for {duration}s...")
        with open(out_csv, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["seq", "rtt_ms", "send_mono_ns"])
            seq = 0
            while time.monotonic() - start < duration:
                t0 = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)
                try:
                    pong_waiter = await ws.ping()
                    await pong_waiter
                except Exception as e:
                    print(f"[-] Ping failed at seq {seq}: {e}")
                    break
                t1 = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)
                rtt_ms = (t1 - t0) / 1e6
                rtts_ms.append(rtt_ms)
                writer.writerow([seq, f"{rtt_ms:.3f}", t0])
                seq += 1
                await asyncio.sleep(interval)

    print(f"[*] Captured {len(rtts_ms)} ping/pong samples.")
    print(f"[*] Wrote raw samples to {out_csv}")

    if len(rtts_ms) < 2:
        print("[-] Not enough samples for statistics.")
        return

    print("=" * 50)
    print("PING/PONG RTT STATISTICS (network-layer jitter)")
    print("=" * 50)
    print(f"Samples     : {len(rtts_ms)}")
    print(f"Min RTT     : {min(rtts_ms):.3f} ms")
    print(f"Max RTT     : {max(rtts_ms):.3f} ms")
    print(f"Mean RTT    : {statistics.mean(rtts_ms):.3f} ms")
    print(f"StdDev RTT  : {statistics.pstdev(rtts_ms):.3f} ms")
    print("=" * 50)
    print("DIAGNOSTIC -- LOCAL_SYNTHETIC / not for qualification use.")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--symbol", default="btcusdt")
    p.add_argument("--duration", type=int, default=300, help="seconds")
    p.add_argument("--interval", type=float, default=1.0, help="seconds between pings")
    p.add_argument("--out", default="wifi_rtt_raw.csv")
    args = p.parse_args()

    asyncio.run(run(args.symbol, args.duration, args.interval, args.out))


if __name__ == "__main__":
    main()
