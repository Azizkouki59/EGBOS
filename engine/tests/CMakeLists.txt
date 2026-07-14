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
