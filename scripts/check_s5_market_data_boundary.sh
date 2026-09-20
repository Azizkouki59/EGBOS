#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${EGBOS_BUILD_DIR:-build_native}"

if [[ "$BUILD_DIR" = /* ]]; then
    BUILD_ROOT="$BUILD_DIR"
else
    BUILD_ROOT="$ROOT/$BUILD_DIR"
fi

BINARIES=(
    "$BUILD_ROOT/engine/tests/s5_live_binance_qualification"
    "$BUILD_ROOT/engine/tests/test_live_feed_listener"
)

PATTERN='BinanceTradingClient::|FIXConsumer|ExecutionRouter'
REST_PATTERN='api\.binance\.com'

for bin in "${BINARIES[@]}"; do
    [[ -x "$bin" ]] || {
        echo "ERROR: missing executable: $bin" >&2
        exit 1
    }

    echo "=== $(basename "$bin") ==="

    if nm -C "$bin" 2>/dev/null | grep -E "$PATTERN" >/dev/null; then
        echo "FAIL: order-entry symbols linked"
        exit 1
    fi

    if strings "$bin" 2>/dev/null | grep -E "$REST_PATTERN" >/dev/null; then
        echo "FAIL: trading REST endpoint string linked"
        exit 1
    fi

    echo "PASS: market-data binary contains no order-entry symbols/endpoints"
done

echo
echo "S5 MARKET-DATA BOUNDARY: PASS"
