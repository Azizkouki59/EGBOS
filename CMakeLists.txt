cmake_minimum_required(VERSION 3.27)
project(egbos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Sanitizer options ──
option(ENABLE_ASAN  "Enable AddressSanitizer"  OFF)
option(ENABLE_TSAN  "Enable ThreadSanitizer"   OFF)
option(ENABLE_UBSAN "Enable UBSanitizer"       OFF)

# ── Subdirectories ──
enable_testing()
add_subdirectory(govcheck)
add_subdirectory(engine)
