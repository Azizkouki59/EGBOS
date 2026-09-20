# S6 Institutional Specification: Production Hardware Qualification

**Document ID:** S6-INST-SPEC-001  
**Authority:** EGBOS Governance Board  
**Governs:** Phase III Entry / S6  
**Status:** ACTIVE — software tasks IN PROGRESS; hardware gates PENDING  
**Predecessor:** S5-INST-SPEC-001 (FROZEN, `docs/S5_INSTITUTIONAL_SPEC.md`)  
**Successor:** S7 — Paper Trading  
**Related exit gate:** `governance/exit-gates/SLICE-S6-exit-v1.yaml`  
**HEAD at authoring:** `a2342c4`

---

## 1. Overview

Slice 6 (S6) is the Phase III entry gate. It has **two parallel workstreams**:

| Workstream | Owner | Blocking |
|---|---|---|
| **SW** — Software defect remediation from S5 Live Qualification | Engineering | Yes — must commit before hardware sign-off |
| **HW** — Physical production hardware procurement and benchmarking | Founder/Infra | Yes — must complete before S7 entry |

S7 (Paper Trading) cannot begin until **both** workstreams reach PASS. A clean `git status`, a green 213-test CTest suite, and all four PROC records signed are the minimum bar.

---

## 2. Software Workstream

### 2.1 Status Snapshot

| Req | Title | Status |
|---|---|---|
| S6-REQ-001 | LOB arithmetic overflow safety | **IMPLEMENTED, not committed** |
| S6-REQ-002 | Parser-level qty bounds rejection | **IMPLEMENTED, not committed** |
| S6-REQ-003 | Operational test parity (CTest green) | **PASS — 213/213** |
| S6-REQ-004 | Boundary-check script correctness | **IMPLEMENTED, not committed** |
| S6-REQ-005 | Worktree clean — zero stray files | **OPEN — ~87 untracked items** |

### 2.2 S6-REQ-001: LOB Arithmetic Overflow Safety

**Problem:** `price_to_index` performed `price - anchor_price_` with no overflow check — signed overflow is UB in C++. `index_to_price` had the same bug in `ticks * tick_size_`. Both reachable from untrusted exchange data.

**Fix applied** (`engine/l1/lob_engine.cpp`, worktree):
```cpp
// price_to_index — saturate to boundary index on overflow
int64_t diff;
if (__builtin_sub_overflow(price, anchor_price_, &diff)) {
    return (price < anchor_price_) ? 0 : static_cast<int32_t>(kMaxPriceLevels) - 1;
}

// index_to_price — saturate to INT64_MIN/MAX on overflow
int64_t offset;
if (__builtin_mul_overflow(ticks, tick_size_, &offset)) {
    return (ticks < 0) ? INT64_MIN : INT64_MAX;
}
int64_t price;
if (__builtin_add_overflow(anchor_price_, offset, &price)) {
    return (offset < 0) ? INT64_MIN : INT64_MAX;
}
```

**Verification:** `LobUpsertTest.*` (6), `LobDeterminismTest.*` (4) — all PASS.

### 2.3 S6-REQ-002: Parser-Level Qty Bounds Rejection

**Problem:** `apply_depth_update` silently saturated out-of-range quantities. No governance rule existed.

**Fix applied** (`engine/l1/depth_parser.cpp`, worktree):
```cpp
// S6-REQ-002: Deterministic parser-level bounds rejection for LOB quantities.
if (qty_val < 0 || qty_val > 0xFFFFFFFFLL) {
    return ParseResult::OVERFLOW;
}
```

The frame is rejected at the parser boundary. `apply_depth_update` receives only pre-validated values and casts without the guard.

### 2.4 S6-REQ-003: Operational Test Parity

**Requirement:** All tests wired in CMakeLists.txt, registered with CTest, passing.

**Current state:** 213/213 PASS, 2 credential-gated skips (expected). `test_lob_fuzz_run` gated behind `if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")` — previously broke GCC builds. `engine_l1` library target added to `engine/CMakeLists.txt`.

### 2.5 S6-REQ-004: Boundary Check Script Correctness

**Problem (HEAD `a2342c4`):** `check_s5_market_data_boundary.sh` had two defects:

1. `REST_PATTERN` included `testnet.binance.vision` — this is the **market-data WebSocket hostname** in `BinanceFeedListener`. Flagging it was a false positive.
2. `PATTERN` was method-level (brittle). Class-level `BinanceTradingClient::` is more robust.

**Fix applied** (worktree):
```diff
-PATTERN='BinanceTradingClient::(place_order|cancel_order|get_account|https_request)|FIXConsumer|ExecutionRouter'
-REST_PATTERN='api\.binance\.com|testnet\.binance\.vision'
+PATTERN='BinanceTradingClient::|FIXConsumer|ExecutionRouter'
+REST_PATTERN='api\.binance\.com'
```

`S5MarketDataBoundary` test — PASS.

### 2.6 S6-REQ-005: Worktree Cleanliness

The worktree has 30 modified tracked files and ~87 untracked items. Full disposal:

#### Must commit (S6 bundle):

| File | Reason |
|---|---|
| `engine/l1/lob_engine.cpp` | S6-REQ-001 |
| `engine/l1/depth_parser.cpp` | S6-REQ-002 |
| `engine/CMakeLists.txt` | `engine_l1` library target |
| `engine/tests/CMakeLists.txt` | Clang fuzz guard |
| `scripts/check_s5_market_data_boundary.sh` | S6-REQ-004 |
| `docs/S6_INSTITUTIONAL_SPEC.md` | This document |

> [!IMPORTANT]
> The remaining 24 tracked modifications must be reviewed individually before committing.
> Do NOT use `git add -A` without reading each diff first.

#### Untracked files — commit (governance artifacts):
- `governance/decisions/EGBOS-GOV-DECISION-*.md`
- `governance/decisions/S5_*.md`
- `governance/proof_artifacts/S5-AC-002-waiver.md`
- `governance/proof_artifacts/s5_market_data_boundary.json`
- `governance/proof_artifacts/s5_uptime_trial/`
- `governance/claims/CLAIM-RISK-STALE-001.yaml`
- `governance/ratification/`, `governance/registry/`, `governance/amendments/`
- `governance/exit-gates/SLICE-S0-exit-v1.yaml`
- `docs/S5_INSTITUTIONAL_SPEC.md`, `docs/S5_TIER0_PROVISIONING_HANDOFF.md`, `docs/S6_HANDOFF.md`
- `engine/tests/test_binance_frame_callback.cpp`
- `engine/tests/test_binance_frame_semantics.cpp`
- `engine/tests/test_cli_sync_replay.cpp`
- `engine/tests/test_tier0_threshold.cpp`
- `engine/tests/test_tier0_write_failure.cpp`
- `scripts/e2e_live_diagnostic.cpp`, `scripts/egbos_binance_cli.cpp`
- `scripts/live_pipeline_diagnostic.cpp`, `scripts/e2e_pipeline.h`, `scripts/CMakeLists.txt`

#### Untracked files — gitignore:
```
build_native/
build_fuzz/
build_fuzz_san/
runtime_tier0/
engine/tests/fuzz_corpus/
*.bak
*.bak_tier0_contract
*.bak_tier0_threshold
dummy_wal.bin
wal_primary.bin
wal_shadow.bin
scratch_test_client
test_depth_parser_bin
run_qualifier
tsc_after_write,
jitter_raw_timestamps_btcusdt.csv
wifi_rtt_raw.csv
governance/proof_artifacts/s5_ac001_binance_frames.csv
governance/proof_artifacts/l0_ast_check.log
governance/proof_artifacts/l0_state_machine_coverage.xml
scripts/Untitled-4
scripts/e2e_live_diagnostic.before_tier0.cpp
scripts/e2e_live_diagnostic.testnet.bak.cpp
```

#### Untracked files — delete (junk):
- `"= p.read_text()..."` — stray shell redirect
- `"RDER ENFORCER DIFF ==="` — stray shell redirect
- `"docs/<!DOCTYPE html>"`, `"docs/<!-- START doctoc..."` — stray HTML fragments
- `fix_*.py`, `patch_*.py`, `generate_tests.py`, `scratch_dummy_wal.py`
- `binance_*.py`, `egbos_wal_reader.py`, `test_ws_raw.py`
- `debug_det.cpp`, `S5_STEP28_OPERATIONAL_SOURCE_AUDIT.md` (root-level)
- `scripts/binance_candle_viewer.cpp`, `scripts/testnet_diagnostic_runner.cpp`
- `scripts/s5_live_jitter_diagnostic.cpp`

---

## 3. Hardware Workstream (PROC Gates)

All four PROC gates PENDING. Require physical action by the Founder.

### 3.1 PROC-001 — Colocation Facility (S6-AC-001)

| Criterion | Target |
|---|---|
| Facility class | Tier-3 or Tier-4 |
| One-way latency to Binance matching engine | ≤ 2 ms |
| Power redundancy | N+1 minimum |
| Security certification | SOC-2 Type II |

**Deliverable:** SLA document on file, `PROC-001` record countersigned.

### 3.2 PROC-002 — Execution CPU (S6-AC-002)

| Criterion | Target |
|---|---|
| Architecture | AMD EPYC 9004 (Genoa/Bergamo) or Intel Xeon Sapphire Rapids |
| Physical cores | ≥ 32 |
| NUMA topology | Documented in `config/cpu-affinity.yaml` |
| Core isolation | `isolcpus` configured for ENGINE_V3 ring threads |

**Deliverable:** `config/cpu-affinity.yaml` committed with real socket/core assignments.

### 3.3 PROC-003 — NVMe Storage (S6-AC-003)

| Criterion | Target |
|---|---|
| Interface | PCIe Gen 4.0 NVMe, dedicated controller |
| Capacity | ≥ 2 TB usable |
| `fsync` P99 latency | < 100 µs |

**Benchmark:**
```bash
fio --name=fsync_lat --ioengine=sync --rw=write --bs=4k --numjobs=1 \
    --size=1G --fsync=1 --group_reporting --output-format=json \
    > governance/proof_artifacts/s6_fsync_benchmark.json
```

**Deliverable:** `timing_constants.json::fsync_p99_us` populated from production device.

### 3.4 PROC-004 — Network Interface (S6-AC-004)

| Criterion | Target |
|---|---|
| NIC hardware | 10 GbE with DPDK or RDMA bypass |
| Kernel bypass ping-pong P99 | < 5 µs |
| Interrupt coalescing | DISABLED on latency-critical rings |

**Deliverable:** `timing_constants.json::loopback_rtt_p99_us` from DPDK measurement.

---

## 4. Governance Sign-offs

### 4.1 LIVE-CAPITAL-001.yaml (S6-AC-005)

Must be signed and committed before S7 paper-trading infrastructure is provisioned.

```yaml
# governance/sign-offs/LIVE-CAPITAL-001.yaml  (TEMPLATE)
document_id: "LIVE-CAPITAL-001"
slice: "S7"
capital_class: "STAGING"
max_notional_usd: FILL_IN
risk_limit_usd_per_day: FILL_IN
signer: FILL_IN
date: FILL_IN
git_commit_sha: FILL_IN
```

### 4.2 timing_constants.json (S6-AC-006)

```json
{
  "fsync_p99_us": null,
  "loopback_rtt_p99_us": null,
  "colocation_one_way_ms": null,
  "measurement_date": null,
  "measurement_host": null,
  "measurement_commit": null
}
```

Must be populated from **production hardware**, not developer laptop.

---

## 5. Exit Criteria — S6 → S7

All ten gates must be simultaneously true:

| # | Gate | Verified by |
|---|---|---|
| 1 | `git status --porcelain` empty | CI |
| 2 | `ctest` 0 failures (credential skips permitted) | CI |
| 3 | S6-REQ-001 merged to `main` | `git log` |
| 4 | S6-REQ-002 merged to `main` | `git log` |
| 5 | S6-REQ-004 boundary-script fix merged | `git log` |
| 6 | PROC-001 through PROC-004 countersigned | Physical inspection |
| 7 | `config/cpu-affinity.yaml` committed with real assignments | `git show` |
| 8 | `timing_constants.json` populated from production hardware | File contents |
| 9 | `LIVE-CAPITAL-001.yaml` signed and committed | `git show` |
| 10 | `SLICE-S6-exit-v1.yaml` `status: CLOSED` and signed | `git show` |

---

## 6. Recommended Commit Sequence

```
Commit 1: s6(sw): LOB arithmetic overflow safety + qty bounds rejection (S6-REQ-001/002)
  engine/l1/lob_engine.cpp
  engine/l1/depth_parser.cpp

Commit 2: s6(build): add engine_l1 library; gate LibFuzzer target on Clang (S6-REQ-003)
  engine/CMakeLists.txt
  engine/tests/CMakeLists.txt

Commit 3: s6(boundary): narrow PATTERN to class-level; remove WebSocket host from REST_PATTERN (S6-REQ-004)
  scripts/check_s5_market_data_boundary.sh

Commit 4: s6(docs): S6 institutional blueprint
  docs/S6_INSTITUTIONAL_SPEC.md

Commit 5: s6(governance): S5 decision packages, proof artifacts, exit-gate stubs
  governance/decisions/
  governance/proof_artifacts/
  governance/claims/
  governance/exit-gates/SLICE-S0-exit-v1.yaml

Commit 6: s6(tree): commit untracked deliverable test sources and diagnostic scripts
  engine/tests/test_binance_frame_callback.cpp
  engine/tests/test_binance_frame_semantics.cpp
  engine/tests/test_cli_sync_replay.cpp
  engine/tests/test_tier0_threshold.cpp
  engine/tests/test_tier0_write_failure.cpp
  docs/S5_INSTITUTIONAL_SPEC.md
  docs/S5_TIER0_PROVISIONING_HANDOFF.md
  docs/S6_HANDOFF.md
  scripts/e2e_live_diagnostic.cpp
  scripts/egbos_binance_cli.cpp
  scripts/live_pipeline_diagnostic.cpp
  scripts/CMakeLists.txt
  scripts/e2e_pipeline.h

Commit 7: chore: expand .gitignore for build artifacts and scratch files
  .gitignore
```

---

## 7. S7 Entry Scope (for reference only)

S7 (Paper Trading) requires: ENGINE_V3 order-entry path wired to a paper adapter; `LIVE-CAPITAL-001.yaml` signed; production hardware `timing_constants.json`; G1 governance gate (satisfied at S3 exit). S7 is out of S6 scope.
