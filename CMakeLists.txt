cmake_minimum_required(VERSION 3.27)
project(egbos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Sanitizer options ──
# Options are declared here so -DENABLE_ASAN=ON etc. affect every target
# in the repository (engine/, govcheck/, wal/, shm/, benchmarks/, tests).
option(ENABLE_ASAN  "Enable AddressSanitizer"  OFF)
option(ENABLE_TSAN  "Enable ThreadSanitizer"   OFF)
option(ENABLE_UBSAN "Enable UBSanitizer"       OFF)

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

# ── Subdirectories ──
enable_testing()
add_subdirectory(govcheck)
add_subdirectory(engine)
