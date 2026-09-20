# engine/CMakeLists.txt â€” Execution plane build
# S1: Header-only core library + test targets
cmake_minimum_required(VERSION 3.27)

# â”€â”€ Core library (header-only) â”€â”€
# All S1 engine components are header-only:
#   pool_allocator.h, shm_layout.h, cross_plane.h, spsc_ring.h, wal_writer.h
add_library(engine_core INTERFACE)
target_include_directories(engine_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
target_link_libraries(engine_core INTERFACE schemas_lib)

# Backward compat alias
add_library(engine_placeholder INTERFACE)
target_include_directories(engine_placeholder INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(engine_placeholder INTERFACE schemas_lib)

# â”€â”€ L0 Compiled target â”€â”€
# D6 (io_uring) requires compiled object code for the listener.
# ── L0 Market-Data target ─────────────────────────────────────────────────────
# S5 market-data runtime must not link the order-entry implementation.
add_library(engine_l0_market_data STATIC
    l0/binance_json_normalizer.cpp
    l0/ingestor.cpp
    l0/live_feed_listener.cpp
    l0/binance_feed_listener.cpp
)

target_include_directories(engine_l0_market_data PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# OpenSSL is required by the Binance market-data WebSocket listener.
find_package(OpenSSL REQUIRED)

target_link_libraries(engine_l0_market_data PUBLIC
    engine_core
    flatbuffers
    OpenSSL::SSL
    OpenSSL::Crypto
)

# ── L0 Trading target ──────────────────────────────────────────────────────────
# Explicitly isolated from the S5 market-data target.
add_library(engine_l0_trading STATIC
    l0/binance_trading_client.cpp
)

target_include_directories(engine_l0_trading PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(engine_l0_trading PUBLIC
    engine_core
    flatbuffers
    OpenSSL::SSL
    OpenSSL::Crypto
)


# ── L1 Compiled target ──
# depth_parser.cpp + l1_consumer.cpp require compiled object code.
add_library(engine_l1 STATIC
    l1/depth_parser.cpp
    l1/l1_consumer.cpp
    l1/lob_engine.cpp
)
target_include_directories(engine_l1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
target_link_libraries(engine_l1 PUBLIC engine_core)
target_compile_features(engine_l1 PUBLIC cxx_std_20)

install(TARGETS engine_core EXPORT egbosTargets)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/engine FILES_MATCHING PATTERN "*.h")

# â”€â”€ Tests â”€â”€
if(CMAKE_PROJECT_NAME STREQUAL "egbos")
  add_subdirectory(tests)
endif()
