# engine/tests/CMakeLists.txt — S1 engine core unit tests
# Spec: §15.1 (WAL), §15.2 (SPSC), §15.3 (Pool Allocator)
# CI gates: P3 (unit), P4 (TSAN), P5 (latency regression)
cmake_minimum_required(VERSION 3.27)

# GoogleTest is already fetched by the root govcheck CMakeLists.
# We just link against it here.

# ── SPSC Ring Tests (§15.2) ──
# Pure header-only — no system dependencies required.
add_executable(spsc_ring_test spsc_ring_test.cpp)
target_link_libraries(spsc_ring_test PRIVATE
  engine_core
  GTest::gtest
  GTest::gtest_main)
target_compile_features(spsc_ring_test PRIVATE cxx_std_20)
gtest_discover_tests(spsc_ring_test)

# ── Pool Allocator Tests (§15.3) ──
# Pure header-only — no system dependencies required.
add_executable(pool_allocator_test pool_allocator_test.cpp)
target_link_libraries(pool_allocator_test PRIVATE
  engine_core
  GTest::gtest
  GTest::gtest_main)
target_compile_features(pool_allocator_test PRIVATE cxx_std_20)
gtest_discover_tests(pool_allocator_test)

# ── WAL Writer Tests (§15.1) ──
# Linux-only: requires io_uring (kernel 5.1+).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(wal_writer_test wal_writer_test.cpp)
  target_link_libraries(wal_writer_test PRIVATE
    engine_core
    GTest::gtest
    GTest::gtest_main)
  target_compile_features(wal_writer_test PRIVATE cxx_std_20)
  gtest_discover_tests(wal_writer_test)
endif()



# ── S2 Targets (L0, L1) ──
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(test_gap_detection test_gap_detection.cpp ../l0/ingestor.cpp ../l0/binance_json_normalizer.cpp)
  target_link_libraries(test_gap_detection PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_gap_detection PRIVATE cxx_std_20)
  gtest_discover_tests(test_gap_detection)

  # S2/S5: Tier-0 write gate regression.
  # A Tier-0 I/O failure must prevent publication to the L0->L1 SPSC ring.
  # Linux-only because /dev/full is used as a deterministic write-failure device.
  add_executable(test_tier0_write_failure
    test_tier0_write_failure.cpp
    ../l0/ingestor.cpp
    ../l0/binance_json_normalizer.cpp
  )
  target_link_libraries(test_tier0_write_failure PRIVATE
    engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_tier0_write_failure PRIVATE cxx_std_20)
  gtest_discover_tests(test_tier0_write_failure)

  # S2/AD-003: Tier-0 threshold durability regression.
  add_executable(test_tier0_threshold
    test_tier0_threshold.cpp
    ../l0/ingestor.cpp
    ../l0/binance_json_normalizer.cpp
  )
  target_link_libraries(test_tier0_threshold PRIVATE
    engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_tier0_threshold PRIVATE cxx_std_20)
  target_link_options(test_tier0_threshold PRIVATE "-Wl,--wrap=fsync")
  gtest_discover_tests(test_tier0_threshold)

  add_executable(test_lob_determinism test_lob_determinism.cpp ../l1/lob_engine.cpp ../l0/ingestor.cpp ../l0/binance_json_normalizer.cpp)
  target_link_libraries(test_lob_determinism PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_lob_determinism PRIVATE cxx_std_20)
  gtest_discover_tests(test_lob_determinism PROPERTIES ENVIRONMENT "EGBOS_REPO_ROOT=${PROJECT_SOURCE_DIR}")

  add_executable(test_lob_fuzz test_lob_fuzz.cpp ../l1/lob_engine.cpp ../l0/ingestor.cpp ../l0/binance_json_normalizer.cpp)
  target_link_libraries(test_lob_fuzz PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_lob_fuzz PRIVATE cxx_std_20)

  # ── S2-REQ-007: LibFuzzer harness for LOB engine ──
  # NOTE: test_lob_fuzz above is misconfigured as a gtest binary linking
  # GTest::gtest_main, which silently discards LLVMFuzzerTestOneInput and
  # runs 0 tests. This is the working fuzzer. Fixed in S6, requires clang.
  # -fsanitize=fuzzer is Clang-only; GCC does not support it.
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(test_lob_fuzz_run test_lob_fuzz.cpp ../l1/lob_engine.cpp ../l0/ingestor.cpp ../l0/binance_json_normalizer.cpp)
    target_link_libraries(test_lob_fuzz_run PRIVATE engine_core flatbuffers)
    target_compile_features(test_lob_fuzz_run PRIVATE cxx_std_20)
    target_compile_options(test_lob_fuzz_run PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all -UNDEBUG)
    target_link_options(test_lob_fuzz_run PRIVATE -fsanitize=fuzzer,address,undefined)
  endif()

  # ── S3 Targets (L3, L4) ──
  add_executable(position_engine_test position_engine_test.cpp ../core/position_engine.cpp)
  target_link_libraries(position_engine_test PRIVATE engine_core GTest::gtest GTest::gtest_main)
  target_compile_features(position_engine_test PRIVATE cxx_std_20)
  gtest_discover_tests(position_engine_test)

  add_executable(risk_enforcer_test risk_enforcer_test.cpp ../core/risk_enforcer.cpp ../core/position_engine.cpp)
  target_link_libraries(risk_enforcer_test PRIVATE engine_core GTest::gtest GTest::gtest_main)
  target_compile_features(risk_enforcer_test PRIVATE cxx_std_20)
  gtest_discover_tests(risk_enforcer_test)

  # ── S5 Step 2 Evidence Infrastructure Tests ──────────────────────────────────
  # S5-REQ-002: Offline deterministic gap-detection framework.
  # Links ingestor.cpp (read-only; ingestor.cpp is NOT modified by this task).
  add_executable(test_gap_framework test_gap_framework.cpp ../l0/ingestor.cpp ../l0/binance_json_normalizer.cpp)
  target_link_libraries(test_gap_framework PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_gap_framework PRIVATE cxx_std_20)
  gtest_discover_tests(test_gap_framework)

  # S5-REQ-003: Uptime / INV-001 violation telemetry (header-only, no extra source).
  add_executable(test_uptime_telemetry test_uptime_telemetry.cpp)
  target_link_libraries(test_uptime_telemetry PRIVATE engine_core GTest::gtest GTest::gtest_main)
  target_compile_features(test_uptime_telemetry PRIVATE cxx_std_20)
  gtest_discover_tests(test_uptime_telemetry)

  # S5-REQ-004: Deterministic WAL replay comparison (positive + negative).
  add_executable(test_replay_comparison test_replay_comparison.cpp)
  target_link_libraries(test_replay_comparison PRIVATE engine_core GTest::gtest GTest::gtest_main)
  target_compile_features(test_replay_comparison PRIVATE cxx_std_20)
  gtest_discover_tests(test_replay_comparison)

  # S5-REQ-005: Zero-order-path application-level validation.
  # Intentionally does NOT link fix_consumer.cpp — link-time isolation proof.
  add_executable(test_zero_order_path test_zero_order_path.cpp ../l0/ingestor.cpp ../l0/binance_json_normalizer.cpp)
  target_link_libraries(test_zero_order_path PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_zero_order_path PRIVATE cxx_std_20)
  gtest_discover_tests(test_zero_order_path)

  # RFC-004: Binance @trade L0 normalization tests.
  add_executable(test_binance_trade_normalizer test_binance_trade_normalizer.cpp ../l0/binance_json_normalizer.cpp)
  target_link_libraries(test_binance_trade_normalizer PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_binance_trade_normalizer PRIVATE cxx_std_20)
  gtest_discover_tests(test_binance_trade_normalizer)

  # S5-REQ-002: Timestamp injection unit tests.
  # Classification: LOCAL_SYNTHETIC
  add_executable(test_timestamp_injection test_timestamp_injection.cpp ../l0/ingestor.cpp ../l0/binance_json_normalizer.cpp)
  target_link_libraries(test_timestamp_injection PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_timestamp_injection PRIVATE cxx_std_20)
  gtest_discover_tests(test_timestamp_injection)

  # Exact Binance screenshot book: RAW -> L0 -> L1 -> LOB parity test.
  add_executable(test_exact_market_pipeline
    test_exact_market_pipeline.cpp
    ../l0/ingestor.cpp
    ../l0/binance_json_normalizer.cpp
    ../l1/l1_consumer.cpp
    ../l1/lob_engine.cpp
  )
  target_link_libraries(test_exact_market_pipeline PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_exact_market_pipeline PRIVATE cxx_std_20)
  gtest_discover_tests(test_exact_market_pipeline)

  add_executable(test_cli_production_parity
    test_cli_production_parity.cpp
    ../l0/ingestor.cpp
    ../l0/binance_json_normalizer.cpp
    ../l1/depth_parser.cpp
    ../l1/l1_consumer.cpp
    ../l1/lob_engine.cpp
  )
  target_link_libraries(test_cli_production_parity PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_cli_production_parity PRIVATE cxx_std_20)
  gtest_discover_tests(test_cli_production_parity)


  # S5-REQ-002: End-to-End Timestamp Determinism Proof.
  add_executable(test_replay_timestamp_determinism test_replay_timestamp_determinism.cpp ../l0/ingestor.cpp ../l0/binance_json_normalizer.cpp)
  target_link_libraries(test_replay_timestamp_determinism PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_replay_timestamp_determinism PRIVATE cxx_std_20)
  gtest_discover_tests(test_replay_timestamp_determinism)

  # S5-REQ-004: Snapshot Serialization Contract test.
  add_executable(test_snapshot_contract test_snapshot_contract.cpp ../core/snapshot_serializer.cpp ../core/snapshot_deserializer.cpp ../core/position_engine.cpp ../core/risk_enforcer.cpp ../l1/lob_engine.cpp)
  target_link_libraries(test_snapshot_contract PRIVATE engine_core GTest::gtest GTest::gtest_main crypto)
  target_compile_features(test_snapshot_contract PRIVATE cxx_std_20)
  gtest_discover_tests(test_snapshot_contract)

  # S5-REQ-001: LiveFeedListener D1–D7 unit tests (DEVELOPMENT_TEST provenance).
  # Classification: LOCAL_SYNTHETIC — NOT LIVE_OPERATIONAL
  # Governance: GOV-S5-L0-ARCH-DECISIONS (EGBOS-GOV-DECISION-2026-009)
  add_executable(test_live_feed_listener test_live_feed_listener.cpp
                 ../l0/live_feed_listener.cpp
                 ../l0/ingestor.cpp ../l0/binance_json_normalizer.cpp)
  target_link_libraries(test_live_feed_listener PRIVATE engine_core engine_l0_market_data GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_live_feed_listener PRIVATE cxx_std_20)
  gtest_discover_tests(test_live_feed_listener)

  # S5-REQ-005: targeted network-namespace connectivity verification.
  # Each discovered case runs inside bwrap --unshare-net via TEST_LAUNCHER.
  # Capability state is measured directly from /proc/self/status.
  add_executable(test_netns_egress test_netns_egress.cpp)
  target_link_libraries(test_netns_egress PRIVATE engine_core GTest::gtest GTest::gtest_main)
  target_compile_features(test_netns_egress PRIVATE cxx_std_20)
  # Run each discovered GoogleTest case inside an isolated network namespace.
  # TEST_LAUNCHER is honored by gtest_discover_tests() for both discovery
  # and execution (CMake >= 3.29).
  set_property(TARGET test_netns_egress PROPERTY TEST_LAUNCHER
               bwrap --unshare-net --dev-bind / /)
  gtest_discover_tests(test_netns_egress)
  # Binance Feed Listener + Trading Client tests.
  # Classification: DEVELOPMENT_TEST / LOCAL_SYNTHETIC — NOT LIVE_OPERATIONAL.
  # Live connection sub-tests skip gracefully when testnet credentials are absent.
  # Governance: D7 (provenance classification), D9 (testnet != live_operational).
  add_executable(test_binance_ws_protocol test_binance_ws_protocol.cpp)

  target_link_libraries(test_binance_ws_protocol PRIVATE engine_core engine_l0_market_data OpenSSL::SSL OpenSSL::Crypto GTest::gtest GTest::gtest_main)

  target_compile_features(test_binance_ws_protocol PRIVATE cxx_std_20)

  gtest_discover_tests(test_binance_ws_protocol)

  add_executable(test_binance_feed_listener
                 test_binance_feed_listener.cpp)
  target_link_libraries(test_binance_feed_listener PRIVATE
    engine_core engine_l0_market_data engine_l0_trading
    GTest::gtest GTest::gtest_main)
  target_compile_features(test_binance_feed_listener PRIVATE cxx_std_20)
  gtest_discover_tests(test_binance_feed_listener)

  # S5-REQ-001: Live Qualification Harness (Binance production)
  add_executable(s5_live_binance_qualification ../../scripts/s5_live_binance_qualification.cpp)
  target_link_libraries(s5_live_binance_qualification PRIVATE engine_core engine_l0_market_data OpenSSL::SSL OpenSSL::Crypto)
  target_compile_features(s5_live_binance_qualification PRIVATE cxx_std_20)
  # S5-REQ-002: Zero-allocation JSON depth-update parser tests.
  add_executable(test_depth_parser test_depth_parser.cpp ../l1/depth_parser.cpp)
  target_link_libraries(test_depth_parser PRIVATE engine_core GTest::gtest GTest::gtest_main)
  target_compile_features(test_depth_parser PRIVATE cxx_std_20)
  gtest_discover_tests(test_depth_parser)

  # S5-REQ-002: L1 consumer (DEGRADED-latching) tests.
  add_executable(test_l1_consumer test_l1_consumer.cpp ../l1/depth_parser.cpp ../l1/l1_consumer.cpp ../l1/lob_engine.cpp)
  target_link_libraries(test_l1_consumer PRIVATE engine_core GTest::gtest GTest::gtest_main)
  target_compile_features(test_l1_consumer PRIVATE cxx_std_20)
  gtest_discover_tests(test_l1_consumer)

  # S2-REQ-006: LobEngine upsert_synthetic_level tests.
  add_executable(test_lob_upsert test_lob_upsert.cpp ../l1/lob_engine.cpp)
  target_link_libraries(test_lob_upsert PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_lob_upsert PRIVATE cxx_std_20)
  gtest_discover_tests(test_lob_upsert)

  # ── L2: L1->L2 snapshot transport / ABI test ──
  add_executable(test_l2_snapshot_reader
    test_l2_snapshot_reader.cpp
    test_feature_engine.cpp
    ../l1/lob_engine.cpp
    ../l1/lob_snapshot_publisher.h
  )
  target_link_libraries(test_l2_snapshot_reader PRIVATE
    engine_core engine_l2 GTest::gtest GTest::gtest_main)
  target_compile_features(test_l2_snapshot_reader PRIVATE cxx_std_20)
  gtest_discover_tests(test_l2_snapshot_reader)

  # ── L2 → L3 FeatureRegion ABI transport tests ─────────────────────────────
  # Tests the per-instrument FeatureSlot seqlock protocol:
  #   publish/read, replacement, unpublished/torn/schema rejection,
  #   source_seq preservation, slot independence, concurrent SPSC correctness.
  #
  # CRITICAL ISOLATION: links engine_l2 (production module) and engine_core only.
  # Must NOT link engine_l2_candidates or engine_l2_hawkes — the transport ABI
  # must be validated without any research module contamination.
  add_executable(test_l2_feature_transport
    test_l2_feature_transport.cpp
  )
  target_link_libraries(test_l2_feature_transport PRIVATE
    engine_core engine_l2 GTest::gtest GTest::gtest_main pthread)
  target_compile_features(test_l2_feature_transport PRIVATE cxx_std_20)
  gtest_discover_tests(test_l2_feature_transport)



  # ── L2: Hawkes intensity unit tests ──────────────────────────────────────
  # governance/claims/CLAIM-L2-HAWKES-001.yaml (status: pending)
  #
  # CRITICAL ISOLATION: links engine_l2_hawkes (pending module) but NOT
  # engine_l2 (production module). This must not enter the production
  # FeatureEngine object file until the claim is active.
  add_executable(test_hawkes_intensity
    test_hawkes_intensity.cpp
  )
  target_link_libraries(test_hawkes_intensity PRIVATE
    engine_l2_hawkes GTest::gtest GTest::gtest_main)
  target_compile_features(test_hawkes_intensity PRIVATE cxx_std_20)
  gtest_discover_tests(test_hawkes_intensity)

  # CLI and Binance Frame Semantic Tests (DEVELOPMENT_TEST)
  add_executable(test_binance_frame_semantics test_binance_frame_semantics.cpp ../l1/lob_engine.cpp ../l1/depth_parser.cpp)
  target_link_libraries(test_binance_frame_semantics PRIVATE engine_core GTest::gtest GTest::gtest_main)
  target_compile_features(test_binance_frame_semantics PRIVATE cxx_std_20)
  gtest_discover_tests(test_binance_frame_semantics)
  
  add_executable(test_binance_frame_callback test_binance_frame_callback.cpp ../l0/live_feed_listener.cpp ../l0/ingestor.cpp ../l0/binance_json_normalizer.cpp ../l1/lob_engine.cpp ../l1/depth_parser.cpp ../l1/l1_consumer.cpp)
  target_link_libraries(test_binance_frame_callback PRIVATE engine_core engine_l0_market_data flatbuffers GTest::gtest GTest::gtest_main)
  target_compile_features(test_binance_frame_callback PRIVATE cxx_std_20)
  gtest_discover_tests(test_binance_frame_callback)
  
  add_executable(test_cli_sync_replay test_cli_sync_replay.cpp ../l1/lob_engine.cpp ../l1/depth_parser.cpp)
  target_link_libraries(test_cli_sync_replay PRIVATE engine_core GTest::gtest GTest::gtest_main)
  target_compile_features(test_cli_sync_replay PRIVATE cxx_std_20)
  gtest_discover_tests(test_cli_sync_replay)


endif()

add_executable(live_pipeline_diagnostic ../../scripts/live_pipeline_diagnostic.cpp)
target_link_libraries(live_pipeline_diagnostic PRIVATE engine_core engine_l0_market_data engine_l1 OpenSSL::SSL OpenSSL::Crypto)
target_compile_features(live_pipeline_diagnostic PRIVATE cxx_std_20)

add_executable(e2e_live_diagnostic ../../scripts/e2e_live_diagnostic.cpp)
target_link_libraries(e2e_live_diagnostic PRIVATE engine_core engine_l0_market_data engine_l1 flatbuffers OpenSSL::SSL OpenSSL::Crypto)
target_compile_features(e2e_live_diagnostic PRIVATE cxx_std_20)

add_executable(egbos_binance_cli ../../scripts/egbos_binance_cli.cpp)
target_link_libraries(egbos_binance_cli PRIVATE engine_core engine_l0_market_data engine_l1 nlohmann_json::nlohmann_json OpenSSL::SSL OpenSSL::Crypto pthread)
target_compile_features(egbos_binance_cli PRIVATE cxx_std_20)

# S5-REQ-005: structural market-data/trading binary boundary regression guard.
# Verifies built S5 market-data binaries contain no order-entry implementation
# symbols or Binance trading REST endpoint strings.
add_test(
  NAME S5MarketDataBoundary
  COMMAND ${CMAKE_COMMAND} -E env
          EGBOS_BUILD_DIR=${CMAKE_BINARY_DIR}
          ${CMAKE_SOURCE_DIR}/scripts/check_s5_market_data_boundary.sh
)
