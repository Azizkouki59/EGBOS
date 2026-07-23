# engine/CMakeLists.txt — Execution plane build
# S1: Header-only core library + test targets
cmake_minimum_required(VERSION 3.27)

# ── Core library (header-only) ──
# All S1 engine components are header-only:
#   pool_allocator.h, shm_layout.h, cross_plane.h, spsc_ring.h, wal_writer.h
add_library(engine_core INTERFACE)
target_include_directories(engine_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
target_link_libraries(engine_core INTERFACE schemas_lib)

# Backward compat alias
add_library(engine_placeholder INTERFACE)
target_include_directories(engine_placeholder INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(engine_placeholder INTERFACE schemas_lib)

install(TARGETS engine_core EXPORT egbosTargets)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/engine FILES_MATCHING PATTERN "*.h")

# ── Tests ──
if(CMAKE_PROJECT_NAME STREQUAL "egbos")
  add_subdirectory(tests)
endif()
