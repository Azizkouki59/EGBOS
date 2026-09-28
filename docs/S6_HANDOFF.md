# S6 Handoff Document

## 1. STATE OF THE TREE

- **Current HEAD**: `a2342c4 Add depth_parser, L1Consumer, LobEngine upsert; harden BinanceFeedListener (A1-A6); restore dropped test suites`
- **Last 6 Commits on `engine/`**:
  - `a2342c4 Add depth_parser, L1Consumer, LobEngine upsert; harden BinanceFeedListener (A1-A6); restore dropped test suites`
  - `c623499 s2: replace binary ingest stub with Binance JSON normalization`
  - `8d3cffd fix(l1): Restore reproducible determinism baseline`
  - `e5a7255 fix(l1): Physically split bid and ask price level arrays to prevent cross-book corruption during aggregate feed ingest`
  - `f2400f1 fix(wal): enforce payload_length bound at runtime, add payload CRC32C`
  - `3e6a264 test(s5): add S5 validation and regression tests`

### Tracked Modified Files
- `CMakeLists.txt`: Fetches dependencies via static URLs instead of git.
- `docs/EGBOS_MASTER.md`: Documents Tier-0 `MarketEventSlot` layout.
- `docs/S2_INSTITUTIONAL_SPEC.md`: Updates Tier-0 physical record documentation.
- `engine/CMakeLists.txt`: Adds `engine_l1` static library target.
- `engine/core/shm_layout.h`: Removes `/dev/shm` prefix from shared memory paths.
- `engine/l0/binance_feed_listener.cpp`: Adds connection timeout, jitter stats, and WebSocket RFC6455 framing.
- `engine/l0/binance_json_normalizer.cpp`: Implements zero-allocation JSON parsing and field normalization.
- `engine/l0/ingestor.cpp`: Adds Tier-0 durable fsync thresholding and sticky-fault propagation.
- `engine/l0/ingestor.h`: Adds Tier-0 flush state tracking structures.
- `engine/l1/l1_consumer.cpp`: Handles synthetic/control events before L1 book progression.
- `engine/l1/lob_engine.cpp`: Adds `UINT32_MAX` saturation clamping for quantity in `apply_depth_update`.
- `engine/tests/CMakeLists.txt`: Wires new S2 and S5 tests into the build.
- `engine/tests/test_gap_detection.cpp`: Modifies tests to allow unordered JSON fields.
- `engine/tests/test_gap_framework.cpp`, `test_l1_consumer.cpp`, `test_lob_fuzz.cpp`, `test_replay_timestamp_determinism.cpp`, `test_snapshot_contract.cpp`, `test_timestamp_injection.cpp`, `test_zero_order_path.cpp`: General regression/contract test updates.
- `govcheck/CMakeLists.txt`: Minor test wiring updates.
- `governance/proof_artifacts/s5_ac001_binance_evidence.json` / `.sha256`: Updates evidence metrics.
- `governance/proof_artifacts/s5_market_data_results.json`: Artifact updates.
- `scripts/s5_live_binance_qualification.cpp`: Adds CSV logging of jitter intervals and SHA256 sidecar.

### Untracked Files Classification
- **(a) Real Deliverables (Commit Required)**: 
  - `docs/S5_INSTITUTIONAL_SPEC.md`, `docs/S5_TIER0_PROVISIONING_HANDOFF.md`
  - `governance/` (all markdown/yaml/json files inside).
  - `engine/tests/test_*.cpp` (the new test files).
  - `scripts/e2e_live_diagnostic.cpp`, `scripts/egbos_binance_cli.cpp`, `scripts/live_pipeline_diagnostic.cpp`, `scripts/s5_live_jitter_diagnostic.cpp`, `scripts/testnet_diagnostic_runner.cpp`, `scripts/e2e_pipeline.h`.
- **(b) Scratch/Scripts (Keep but `.gitignore`)**:
  - Python scratch scripts: `binance_candle_viewer.py`, `binance_jitter_diagnostic.py`, `binance_ping_diagnostic.py`, `binance_price_viewer.py`, `egbos_wal_reader.py`, `fix_gap_framework.py`, `fix_lob.py`, `fix_viewer.py`, `fix_zero_order_path.py`, `generate_tests.py`, `patch_cmake.py`, `patch_diag.py`, `patch_s5.py`, `patch_subscribe.py`, `patch_test.py`, `scratch_dummy_wal.py`, `test_ws_raw.py`.
  - Data / binary files: `dummy_wal.bin`, `wal_primary.bin`, `wal_shadow.bin`, `jitter_raw_timestamps_btcusdt.csv`, `wifi_rtt_raw.csv`, `test_depth_parser_bin`.
  - Scratch C++ files: `scratch_test_client.cpp`, `debug_det.cpp`.
- **(c) Accidental Junk (Delete)**:
  - Zero-byte stray shell redirect files: `addr`, `fd`, `opcode`, `order_id`, `price`, `sequence`, `sequence,`, `buf_index`, `len`, `off`, `tsc_after_write`, `tsc_before_write,`, `header_crc32c`, `instrument_id`, `payload_length`, `quantity_delta`, `record_type`, `user_data`. (Confirmed 0 bytes via `ls -l`, safe to remove).

**Tree State**: **NOT COMMITTABLE**. The tree mixes uncommitted core engine code with governance artifacts, untracked tests, stray shell redirects, and temporary Python/C++ scripts.

## 2. WHAT S2-S5 ACTUALLY BUILT

- **L0 Feed Ingest**:
  - `binance_feed_listener`, `ingestor`, `binance_json_normalizer`.
  - Claims to satisfy: S2-REQ-001 (FlatBuffers Normalization), S2-REQ-002 (Tier-0 Ordering), S2-REQ-003 (Gap Detection), S2-REQ-004 (Stale Timeout).
  - Covered by: `test_gap_detection`, `test_tier0_write_failure`, `test_tier0_threshold`, `test_live_feed_listener`, `test_binance_ws_protocol`, `test_binance_feed_listener`.
- **L1 LOB Engine**:
  - `lob_engine`, `l1_consumer`, `depth_parser`.
  - Claims to satisfy: S2-REQ-005 (No LOB Allocation), S2-REQ-006 (Fixed-Point Pricing), S2-REQ-007 (Fuzz Resistance), S5-REQ-002 (Gap Detection on Real Feeds), S5-REQ-005 (Zero Order Path).
  - Covered by: `test_lob_determinism`, `test_lob_fuzz`, `test_depth_parser`, `test_l1_consumer`, `test_lob_upsert`, `test_zero_order_path`, `test_exact_market_pipeline`.
- **Governance / Proof Artifacts**:
  - `S5_AC001_WIFI_QUALIFICATION_DECISION_PACKAGE.md`, `S5-AC-002-waiver.md`, `s5_ac001_binance_evidence.json`.
  - Claims to satisfy: S5-REQ-001 (Live Jitter Buffer Sizing), S5-REQ-003 (14-Day Uptime Trial), S5-REQ-004 (Nightly Replay Validation).
  - Covered by: `test_snapshot_contract`, `test_replay_timestamp_determinism`, `test_uptime_telemetry`.

## 3. TEST INVENTORY

All tests reside in `engine/tests/CMakeLists.txt`.
- **Wired and Registered with CTest (`gtest_discover_tests`)**:
  - `spsc_ring_test`, `pool_allocator_test`, `wal_writer_test` (Linux only), `test_gap_detection`, `test_tier0_write_failure`, `test_tier0_threshold`, `test_lob_determinism`, `position_engine_test`, `risk_enforcer_test`, `test_gap_framework`, `test_uptime_telemetry`, `test_replay_comparison`, `test_zero_order_path`, `test_timestamp_injection`, `test_exact_market_pipeline`, `test_cli_production_parity`, `test_replay_timestamp_determinism`, `test_snapshot_contract`, `test_live_feed_listener`, `test_netns_egress` (runs via bwrap), `test_binance_ws_protocol`, `test_binance_feed_listener`, `test_depth_parser`, `test_l1_consumer`, `test_lob_upsert`, `test_binance_frame_semantics`, `test_cli_sync_replay`.
- **Wired but NOT Registered with CTest**:
  - `test_lob_fuzz` (Libfuzzer target, no CTest registration).
- **Unwired Tests (Untracked, not in `CMakeLists.txt`)**:
  - `engine/tests/test_binance_frame_callback.cpp`

*(Note: Without running a build, we assume wired tests build successfully based on their presence in CMakeLists.txt and active development).*

## 4. KNOWN DEFECTS / OPEN RISKS

- **`price_to_index` / `index_to_price` Signed Overflow UB**:
  - **Source**: `engine/l1/lob_engine.cpp:20` computes `int64_t diff = price - anchor_price_;` and `int64_t ticks = diff / tick_size_;` before the boundary clamp.
  - **Risk**: If `price` is near `INT64_MAX` and `anchor_price_` is negative, signed overflow occurs (Undefined Behavior). 
  - **Fuzzing**: `test_lob_fuzz.cpp:20-21` reads `price` via `std::memcpy(&price, Data + 8, 8);`. This means unconstrained `int64_t` values *do* reach `price_to_index` in the fuzz harness, hitting this UB.
  - **Test Coverage**: No standard unit test explicitly exercises `INT64_MIN` / `INT64_MAX` inputs.
- **Uncommitted Quantity Clamp to `UINT32_MAX`**:
  - **Source**: `engine/l1/lob_engine.cpp:211` enforces `qty >= 0xFFFFFFFFLL ? 0xFFFFFFFFu : static_cast<uint32_t>(qty)` in `apply_depth_update`.
  - **Risk**: Saturation is an ad-hoc fix. If the exchange broadcasts an order >429 BTC, saturating it silently corrupts the local order book state relative to the exchange. It should likely be rejected as a parser error or fatal fault. 
  - **Governance**: **UNVERIFIED** - No governance document (neither `EGBOS_MASTER.md` nor `S1/S2/S3/S5_INSTITUTIONAL_SPEC.md`) explicitly specifies this saturation behavior. 
- **TODO/FIXME/HACK in `engine/`**:
  - **None found**. (0 results from `grep -rE "TODO|FIXME|HACK" engine/`).

## 5. S6 ENTRY CRITERIA

Before starting S6 work, the following must be executed to sanitize the tree and lock in S5:
1. **Clean Accidental Junk**:
   ```bash
   rm addr fd opcode order_id price sequence sequence, buf_index len off tsc_after_write tsc_before_write, header_crc32c instrument_id payload_length quantity_delta record_type user_data
   ```
2. **Update `.gitignore`**:
   ```bash
   echo "*.py\n*.csv\n*.bin\nscratch_*" >> .gitignore
   ```
3. **Commit S5 Deliverables**:
   ```bash
   git add CMakeLists.txt docs/ engine/ govcheck/ governance/ scripts/
   git commit -m "chore(s5): commit final S5 deliverables and diagnostics"
   ```
4. **Re-run Quality Gates**:
   ```bash
   mkdir -p build && cd build
   cmake ..
   make -j$(nproc)
   ctest --output-on-failure
   ```

## 6. PROPOSED S6 SCOPE

Ranked by risk to system correctness:

1. **Fix `price_to_index` Undefined Behavior**
   - **Problem**: Signed integer overflow when calculating `diff` or `ticks` before clamping, reachable via fuzzing.
   - **Affected Files**: `engine/l1/lob_engine.cpp`
   - **Acceptance Criteria**: Arithmetic uses checked/saturating operations (`__builtin_add_overflow` / `__builtin_sub_overflow`) and defaults to `0` or `kMaxPriceLevels - 1` upon overflow.
   - **Risk**: High (Crash/DoS via maliciously crafted/extreme feed prices).
2. **Standardize LOB Quantity Bounds (S6-REQ-001 (PROPOSED))**
   - **Problem**: `apply_depth_update` silently saturates extreme quantities to `UINT32_MAX`, causing structural misalignment with exchange LOB.
   - **Affected Files**: `engine/l1/lob_engine.cpp`, `docs/EGBOS_MASTER.md`
   - **Acceptance Criteria**: Introduce an explicit governance decision on structural bounds. Either throw a fatal parsing error if `qty > UINT32_MAX` or promote internal LOB structures to `int64_t`.
   - **Risk**: High (Silent data corruption / trading on phantom liquidity).
3. **Wire Orphaned Tests**
   - **Problem**: `test_binance_frame_callback.cpp` exists but is untracked and unwired.
   - **Affected Files**: `engine/tests/CMakeLists.txt`, `engine/tests/test_binance_frame_callback.cpp`
   - **Acceptance Criteria**: Test is added to `CMakeLists.txt` via `add_executable` and `gtest_discover_tests`, and passes cleanly.
   - **Risk**: Low (Missing coverage).

## Deferred integration notes (found during S6 code review, 2026-09-28)
- L2 FeatureEngine writes a valid, mask=0 FeatureVector; no L3 consumer exists
  yet. When L3 is built, confirm it checks validity_mask_lo64 per-bit before
  reading any feature payload byte — do not assume zero payload means zero
  feature value.
- core::GovernanceFlags (governance_interface.h, used by PositionEngine/
  RiskEnforcer) and shm::GovernanceFlags (shm_layout.h, used by ingestor/
  l1_consumer/execution_router) are two independently-evolved structs with
  overlapping field names but different layouts. Neither PositionEngine nor
  RiskEnforcer is constructed in any production path yet (test-only). Before
  wiring L3/L4 into the real engine loop, resolve whether these should be one
  shared SHM-backed type or an explicit bridge — do not assume they're
  interchangeable.
