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
# ── Dependencies via FetchContent ──
include(FetchContent)

FetchContent_Declare(yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
  GIT_SHALLOW TRUE
)

FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)

FetchContent_Declare(flatbuffers
  URL https://github.com/google/flatbuffers/archive/refs/tags/v24.3.25.tar.gz
)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)  # yaml-cpp 0.8.0 declares cmake_minimum_required < 3.5
FetchContent_MakeAvailable(yaml-cpp json googletest flatbuffers)

add_subdirectory(schemas)
add_subdirectory(govcheck)
add_subdirectory(engine)
