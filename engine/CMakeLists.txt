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

# ── L2 Feature Engine transport boundary ──
# Current scope: L1→L2 snapshot reader + schema-versioned skeleton write.
# Quantitative feature values are intentionally absent — no EGBOS feature
# contract is active yet. See governance/rfcs/drafts/CLAIM-L2-HAWKES-001.yaml
# (pending) and governance/rfcs/RFC-003-L2-HAWKES-VPIN-FEATURE-SCOPING.yaml
# for the current state of blocked/proposed L2 feature decisions.
#
# feature_compute.cpp is the SOLE production feature computation TU.
# It MUST NOT include any header from l2/candidates/ or any research module.
# This is enforced by convention (comments) and audited at code review.
add_library(engine_l2 STATIC
    l2/feature_engine.cpp
    l2/feature_compute.cpp
)
target_include_directories(engine_l2 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(engine_l2 PUBLIC engine_core)
target_compile_features(engine_l2 PUBLIC cxx_std_20)

# ── L2 Candidate feature module (RESEARCH ONLY — header-only) ──
# COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
# No ratified EGBOS feature specification exists for this module (see
# RFC-003 evidence section: no such document is present in this repository).
#
# CRITICAL ISOLATION: engine_l2_candidates is intentionally NOT linked into
# engine_l2. This enforces the production/research boundary at the linker
# level: the FeatureEngine binary cannot accidentally call candidate functions.
# Any attempt to link engine_l2_candidates into engine_l2 is a governance
# violation and must be rejected in code review.
add_library(engine_l2_candidates INTERFACE)
target_include_directories(engine_l2_candidates INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
target_compile_features(engine_l2_candidates INTERFACE cxx_std_20)

# ── L2 Hawkes intensity module (PENDING — header-only) ──────────────────
# See governance/rfcs/drafts/CLAIM-L2-HAWKES-001.yaml (status: pending) and
# governance/rfcs/RFC-003-L2-HAWKES-VPIN-FEATURE-SCOPING.yaml.
#
# CRITICAL ISOLATION: engine_l2_hawkes is intentionally NOT linked into
# engine_l2. CLAIM-L2-HAWKES-001 is pending, not active — this module must
# not enter the production FeatureEngine binary until the claim is reviewed,
# an offline reference implementation exists for DIRC parity, and the claim
# status is updated to active with a feature_id assigned. Linking this into
# engine_l2 before then is a governance violation and must be rejected in
# code review.
add_library(engine_l2_hawkes INTERFACE)
target_include_directories(engine_l2_hawkes INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
target_compile_features(engine_l2_hawkes INTERFACE cxx_std_20)

install(TARGETS engine_core EXPORT egbosTargets)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/engine FILES_MATCHING PATTERN "*.h")

# â”€â”€ Tests â”€â”€
if(CMAKE_PROJECT_NAME STREQUAL "egbos")
  add_subdirectory(tests)
endif()
