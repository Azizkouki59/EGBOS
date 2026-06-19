# engine/CMakeLists.txt — Execution plane build (placeholder at S0)
cmake_minimum_required(VERSION 3.27)

# S0: placeholder only — no engine source files yet
# S1 will add WAL, SPSC, and SHM skeleton targets

add_library(engine_placeholder INTERFACE)
target_include_directories(engine_placeholder INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
