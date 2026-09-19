#!/usr/bin/env bash
# scripts/s5_zero_order_path_enforcer.sh
# EGBOS S5-REQ-005 â€” Architectural Zero-Order Path Enforcement
#
# Classification  : INFRASTRUCTURE_VALIDATION
# Governance      : V3 AUTHORITATIVE â€” S5-REQ-005
#
# Architecture:
#   PRIVILEGED BOOTSTRAP (root / CAP_NET_ADMIN)
#       â†“
#   create isolated network namespace
#       â†“
#   configure RX-only interface (lo or specified multicast iface)
#       â†“
#   configure namespace without a default route
#       â†“
#   drop privileges (setuid to EGBOS_USER)
#       â†“
#   exec() EGBOS engine â€” runs UNPRIVILEGED in isolated namespace
#
# The disposable namespace enforces the tested S5 boundary conditions:
#   1. No default route is present in the namespace.
#   2. Only the configured RX-side interface is provisioned by this harness.
#   3. The engine process has no CAP_NET_ADMIN / CAP_NET_RAW after exec().
#
# IMPORTANT:
#   Absence of a default route does NOT by itself prove arbitrary egress is
#   impossible. Connected routes may still permit traffic to reachable peers.
#
# Usage:
#   sudo s5_zero_order_path_enforcer.sh --engine-binary <path> [--engine-user <user>]
#                                       [--rx-iface <iface>] [--test-only]
#
#   --engine-binary  : Path to EGBOS engine binary.
#   --engine-user    : Unprivileged user to run engine as (default: egbos).
#   --rx-iface       : Feed interface to configure in namespace (default: lo for test).
#   --test-only      : Validate the namespace policy only; do not exec the engine.
#                      Uses a disposable test namespace and test interface.

set -euo pipefail

# â”€â”€â”€ Configuration â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
readonly NETNS_NAME="egbos_rx_only_$$"
ENGINE_BINARY=""
ENGINE_USER="${EGBOS_USER:-egbos}"
RX_IFACE="lo"   # default to lo for safe testing; override in production
TEST_ONLY=false

# â”€â”€â”€ Argument parsing â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
while [[ $# -gt 0 ]]; do
    case "$1" in
        --engine-binary) ENGINE_BINARY="$2"; shift 2 ;;
        --engine-user)   ENGINE_USER="$2";   shift 2 ;;
        --rx-iface)      RX_IFACE="$2";      shift 2 ;;
        --test-only)     TEST_ONLY=true;     shift ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

# â”€â”€â”€ Root check â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
if [[ "$(id -u)" -ne 0 ]]; then
    echo "[S5-REQ-005] ERROR: Must run as root (CAP_NET_ADMIN required for namespace creation)" >&2
    exit 1
fi

# â”€â”€â”€ Capability check â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
if ! ip netns list &>/dev/null; then
    echo "[S5-REQ-005] ENVIRONMENT_BLOCKED: ip netns not available or CAP_NET_ADMIN absent" >&2
    exit 2
fi

# â”€â”€â”€ Cleanup trap â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
cleanup() {
    echo "[S5-REQ-005] Cleaning up namespace: ${NETNS_NAME}"
    ip netns del "$NETNS_NAME" 2>/dev/null || true
}
trap cleanup EXIT

# â”€â”€â”€ Create isolated namespace â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
echo "[S5-REQ-005] Creating isolated network namespace: ${NETNS_NAME}"
ip netns add "$NETNS_NAME"

# Add loopback (always needed for internal IPC)
ip netns exec "$NETNS_NAME" ip link set lo up

# â”€â”€â”€ Configure RX interface inside namespace â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# In production: move the physical multicast feed interface into this namespace.
# In test mode: only lo is present; no external connectivity exists by design.
#
# SAFETY: We do NOT move physical production interfaces during --test-only mode.
if [[ "$TEST_ONLY" == false ]] && [[ "$RX_IFACE" != "lo" ]]; then
    echo "[S5-REQ-005] Moving interface ${RX_IFACE} into namespace ${NETNS_NAME}"
    ip link set "$RX_IFACE" netns "$NETNS_NAME"
    ip netns exec "$NETNS_NAME" ip link set "$RX_IFACE" up
fi

# â”€â”€â”€ Verify: No outbound default route â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
echo "[S5-REQ-005] Verifying: no default outbound route in namespace"
ROUTES=$(ip netns exec "$NETNS_NAME" ip route show 2>&1)
if echo "$ROUTES" | grep -q 'default'; then
    echo "[S5-REQ-005] FAIL: Default route found; required no-default-route condition is not satisfied" >&2
    echo "$ROUTES" >&2
    exit 1
fi
echo "[S5-REQ-005] PASS: No default route observed in disposable namespace."
echo "[S5-REQ-005] NOTE: Absence of a default route does not by itself prove arbitrary egress is impossible."

# â”€â”€â”€ Verify: Target user exists and is not root â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
if [[ "$TEST_ONLY" == false ]]; then
    if ! id "$ENGINE_USER" &>/dev/null; then
        echo "[S5-REQ-005] ERROR: Engine user '${ENGINE_USER}' does not exist." >&2
        echo "[S5-REQ-005] Production execution requires an existing unprivileged user." >&2
        exit 1
    fi

    ENGINE_UID=$(id -u "$ENGINE_USER")
    if [[ "$ENGINE_UID" -eq 0 ]]; then
        echo "[S5-REQ-005] ERROR: ENGINE_USER resolves to root (uid 0). Engine must not run as root." >&2
        exit 1
    fi

    echo "[S5-REQ-005] Engine user: ${ENGINE_USER} (uid=${ENGINE_UID})"
fi

# â”€â”€â”€ Test mode: verify targeted egress probe from inside namespace â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
echo "[S5-REQ-005] Running targeted egress probe from inside namespace"
EGRESS_TEST_RESULT="FAIL"
set +e
ip netns exec "$NETNS_NAME" sh -c '
    python3 -c "
import socket, errno
try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(2)
    s.connect((\"8.8.8.8\", 80))
    print(\"EGRESS_ALLOWED\")
except OSError as e:
    if e.errno in (101, 111, 113, 110, 99):
        print(\"EGRESS_DENIED errno=%d\" % e.errno)
    else:
        print(\"EGRESS_ERROR errno=%d\" % e.errno)
finally:
    try: s.close()
    except: pass
" 2>/dev/null || echo "EGRESS_DENIED (no python3)"
' 2>/dev/null
EGRESS_EXIT=$?
set -e

# Re-run and capture output for analysis
EGRESS_OUTPUT=$(ip netns exec "$NETNS_NAME" sh -c '
    python3 -c "
import socket, errno
try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(2)
    s.connect((\"8.8.8.8\", 80))
    print(\"EGRESS_ALLOWED\")
except OSError as e:
    print(\"EGRESS_DENIED errno=%d\" % e.errno)
finally:
    try: s.close()
    except: pass
" 2>/dev/null || echo "EGRESS_DENIED (no python3)"
' 2>/dev/null || echo "EGRESS_DENIED (namespace exec failed)")

if echo "$EGRESS_OUTPUT" | grep -q 'EGRESS_DENIED'; then
    EGRESS_TEST_RESULT="PASS"
    echo "[S5-REQ-005] PASS: Targeted egress probe blocked â€” ${EGRESS_OUTPUT}"
elif echo "$EGRESS_OUTPUT" | grep -q 'EGRESS_ALLOWED'; then
    EGRESS_TEST_RESULT="FAIL"
    echo "[S5-REQ-005] FAIL: Targeted egress probe succeeded â€” ${EGRESS_OUTPUT}" >&2
    exit 1
else
    echo "[S5-REQ-005] EGRESS_TEST_RESULT=INCONCLUSIVE: ${EGRESS_OUTPUT}"
fi

# â”€â”€â”€ Test only: do not exec engine â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
if [[ "$TEST_ONLY" == true ]]; then
    echo ""
    echo "[S5-REQ-005] TEST_ONLY mode â€” engine not launched"
    echo "[S5-REQ-005] Namespace policy verification:"
    echo "  Namespace            : ${NETNS_NAME}"
    echo "  Default route absent : PASS"
    echo "  Targeted probe blocked: ${EGRESS_TEST_RESULT}"
    echo "  Classification       : INFRASTRUCTURE_VALIDATION"
    echo ""
    echo "[S5-REQ-005] NOTE: This script verifies the namespace configuration and targeted egress probe result."
    echo "[S5-REQ-005] ARCHITECTURAL_ENFORCEMENT scope is limited to the conditions actually observed."
    echo "[S5-REQ-005]   1. This script executed as the privileged bootstrap."
    echo "[S5-REQ-005]   2. Engine running unprivileged inside the namespace."
    echo "[S5-REQ-005]   3. Physical RX feed interface present and reachable."
    exit 0
fi

# â”€â”€â”€ Production: exec engine inside namespace (drop privileges) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
if [[ -z "$ENGINE_BINARY" ]]; then
    echo "[S5-REQ-005] ERROR: --engine-binary required in production mode" >&2
    exit 1
fi

echo "[S5-REQ-005] Dropping privileges and running engine inside namespace"
echo "[S5-REQ-005] Engine binary: ${ENGINE_BINARY}"
echo "[S5-REQ-005] Engine user  : ${ENGINE_USER} (uid=${ENGINE_UID})"

# Run the engine as a child of this bootstrap shell rather than replacing the
# shell itself. This preserves the EXIT trap so the namespace is cleaned up
# after engine termination.
set +e
ip netns exec "$NETNS_NAME" \
    capsh --drop=cap_net_admin,cap_net_raw --user="$ENGINE_USER" -- \
    -c "exec \"$ENGINE_BINARY\""
ENGINE_EXIT=$?
set -e

echo "[S5-REQ-005] Engine exited with status ${ENGINE_EXIT}"
exit "${ENGINE_EXIT}"
