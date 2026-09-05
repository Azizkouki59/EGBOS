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
  add_executable(test_gap_detection test_gap_detection.cpp ../l0/ingestor.cpp)
  target_link_libraries(test_gap_detection PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_gap_detection PRIVATE cxx_std_20)
  gtest_discover_tests(test_gap_detection)

  add_executable(test_lob_determinism test_lob_determinism.cpp ../l1/lob_engine.cpp ../l0/ingestor.cpp)
  target_link_libraries(test_lob_determinism PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_lob_determinism PRIVATE cxx_std_20)
  gtest_discover_tests(test_lob_determinism)

  add_executable(test_lob_fuzz test_lob_fuzz.cpp ../l1/lob_engine.cpp ../l0/ingestor.cpp)
  target_link_libraries(test_lob_fuzz PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_lob_fuzz PRIVATE cxx_std_20)

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
  add_executable(test_gap_framework test_gap_framework.cpp ../l0/ingestor.cpp)
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
  add_executable(test_zero_order_path test_zero_order_path.cpp ../l0/ingestor.cpp)
  target_link_libraries(test_zero_order_path PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_zero_order_path PRIVATE cxx_std_20)
  gtest_discover_tests(test_zero_order_path)

  # S5-REQ-002: Timestamp injection unit tests.
  # Classification: LOCAL_SYNTHETIC
  add_executable(test_timestamp_injection test_timestamp_injection.cpp ../l0/ingestor.cpp)
  target_link_libraries(test_timestamp_injection PRIVATE engine_core GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_timestamp_injection PRIVATE cxx_std_20)
  gtest_discover_tests(test_timestamp_injection)

  # S5-REQ-002: End-to-End Timestamp Determinism Proof.
  add_executable(test_replay_timestamp_determinism test_replay_timestamp_determinism.cpp ../l0/ingestor.cpp)
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
                 ../l0/ingestor.cpp)
  target_link_libraries(test_live_feed_listener PRIVATE engine_core engine_l0 GTest::gtest GTest::gtest_main flatbuffers)
  target_compile_features(test_live_feed_listener PRIVATE cxx_std_20)
  gtest_discover_tests(test_live_feed_listener)

  # S5-REQ-005: OS-level network namespace egress block verification.
  # Requires CAP_NET_ADMIN; skips gracefully if environment lacks it.
  # libcap is optional: if absent, capability checks report 'absent' (conservative).
  add_executable(test_netns_egress test_netns_egress.cpp)
  find_library(CAP_LIB cap)
  if(CAP_LIB)
    target_link_libraries(test_netns_egress PRIVATE engine_core GTest::gtest GTest::gtest_main ${CAP_LIB})
    target_compile_definitions(test_netns_egress PRIVATE HAVE_LIBCAP=1)
  else()
    target_link_libraries(test_netns_egress PRIVATE engine_core GTest::gtest GTest::gtest_main)
    message(STATUS "libcap not found: test_netns_egress will skip capability checks (ENVIRONMENT_BLOCKED path)")
  endif()
  target_compile_features(test_netns_egress PRIVATE cxx_std_20)
  gtest_discover_tests(test_netns_egress)
  # Binance Feed Listener + Trading Client tests.
  # Classification: DEVELOPMENT_TEST / LOCAL_SYNTHETIC — NOT LIVE_OPERATIONAL.
  # Live connection sub-tests skip gracefully when testnet credentials are absent.
  # Governance: D7 (provenance classification), D9 (testnet != live_operational).
  add_executable(test_binance_feed_listener
                 test_binance_feed_listener.cpp)
  target_link_libraries(test_binance_feed_listener PRIVATE
    engine_core engine_l0 GTest::gtest GTest::gtest_main)
  target_compile_features(test_binance_feed_listener PRIVATE cxx_std_20)
  gtest_discover_tests(test_binance_feed_listener)

  # S5-REQ-001: Live Qualification Harness (Binance production)
  add_executable(s5_live_binance_qualification ../../scripts/s5_live_binance_qualification.cpp)
  target_link_libraries(s5_live_binance_qualification PRIVATE engine_core engine_l0 OpenSSL::SSL OpenSSL::Crypto)
  target_compile_features(s5_live_binance_qualification PRIVATE cxx_std_20)
endif()
