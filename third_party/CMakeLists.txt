# third_party/CMakeLists.txt
# This file will be active once the vendored dependencies are dropped into the repository.
cmake_minimum_required(VERSION 3.27)

# We include dependencies but EXCLUDE_FROM_ALL so that only targets actually used by EGBOS are built.

# yaml-cpp
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp/CMakeLists.txt")
  add_subdirectory(yaml-cpp EXCLUDE_FROM_ALL)
endif()

# nlohmann json
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/json/CMakeLists.txt")
  add_subdirectory(json EXCLUDE_FROM_ALL)
endif()

# googletest
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/googletest/CMakeLists.txt")
  add_subdirectory(googletest EXCLUDE_FROM_ALL)
endif()

# flatbuffers
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers/CMakeLists.txt")
  add_subdirectory(flatbuffers EXCLUDE_FROM_ALL)
endif()
