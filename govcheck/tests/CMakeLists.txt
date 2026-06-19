# govcheck/tests/CMakeLists.txt
# Test fixtures are referenced by test binaries defined in parent CMakeLists.txt.
# This file exists to satisfy the add_subdirectory() call.

# Copy test fixtures to build directory so tests can locate them at runtime
file(GLOB FIXTURE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/*.yaml")
foreach(FIXTURE ${FIXTURE_FILES})
  get_filename_component(FIXTURE_NAME ${FIXTURE} NAME)
  configure_file(
    ${FIXTURE}
    ${CMAKE_CURRENT_BINARY_DIR}/fixtures/${FIXTURE_NAME}
    COPYONLY)
endforeach()
