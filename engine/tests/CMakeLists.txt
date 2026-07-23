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
endif()
