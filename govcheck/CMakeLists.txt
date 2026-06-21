cmake_minimum_required(VERSION 3.27)
project(govcheck LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies via FetchContent ──
# Spec: S0 §4.7 — exact pinned versions, no find_package fallback.
include(FetchContent)

FetchContent_Declare(yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0)

FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3)

FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0)

FetchContent_MakeAvailable(yaml-cpp json googletest)

# ── OpenSSL (system — provides SHA-256 for GC-E-018) ──
find_package(OpenSSL REQUIRED)

# ── Sanitizer profiles ──
# Applied at directory scope so both govcheck and govcheck_tests are sanitized.
# Required by S0 §7.4: all tests must pass under ASAN, TSAN, UBSAN.
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UBSanitizer" OFF)
if(ENABLE_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
endif()
if(ENABLE_TSAN)
  add_compile_options(-fsanitize=thread)
  add_link_options(-fsanitize=thread)
endif()
if(ENABLE_UBSAN)
  add_compile_options(-fsanitize=undefined)
  add_link_options(-fsanitize=undefined)
endif()

# ── Source files ──
set(GOVCHECK_SOURCES
  src/main.cpp
  src/parser/yaml_parser.cpp
  src/parser/claim_schema.cpp
  src/validator/claim_validator.cpp
  src/validator/artifact_validator.cpp
  src/validator/ownership_validator.cpp
  src/graph/dag_builder.cpp
  src/graph/cycle_detector.cpp
  src/graph/composition_engine.cpp
  src/graph/namespace_checker.cpp
  src/reporter/json_reporter.cpp
  src/commands/cmd_validate.cpp
  src/commands/cmd_status.cpp
  src/commands/cmd_claims.cpp
)

# ── Main binary ──
add_executable(govcheck ${GOVCHECK_SOURCES})
target_link_libraries(govcheck PRIVATE
  yaml-cpp
  nlohmann_json::nlohmann_json
  OpenSSL::Crypto)
target_include_directories(govcheck PRIVATE src/)

# ── Tests ──
enable_testing()

set(TEST_SOURCES
  tests/test_parser.cpp
  tests/test_validator.cpp
  tests/test_graph.cpp
  tests/test_reporter.cpp
  tests/test_commands.cpp
)

add_executable(govcheck_tests
  ${TEST_SOURCES}
  ${GOVCHECK_SOURCES})

# GOVCHECK_TEST_MODE suppresses main() in src/main.cpp so GTest::gtest_main
# can provide its own entry point. See main.cpp #ifndef GOVCHECK_TEST_MODE.
target_compile_definitions(govcheck_tests PRIVATE GOVCHECK_TEST_MODE)
target_link_libraries(govcheck_tests PRIVATE
  yaml-cpp
  nlohmann_json::nlohmann_json
  OpenSSL::Crypto
  GTest::gtest
  GTest::gtest_main)
target_include_directories(govcheck_tests PRIVATE src/)

include(GoogleTest)
gtest_discover_tests(govcheck_tests)

# Copy test fixtures to build dir (required by test runtime file paths).
add_subdirectory(tests)
