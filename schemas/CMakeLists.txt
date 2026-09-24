# schemas/CMakeLists.txt
cmake_minimum_required(VERSION 3.27)

set(FBS_FILES
    common.fbs
    market_event.fbs
)

set(GENERATED_HEADERS "")

# Create the output directory so flatc doesn't fail
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)

foreach(fbs ${FBS_FILES})
    get_filename_component(fbs_name ${fbs} NAME_WE)
    set(generated_header "${CMAKE_CURRENT_BINARY_DIR}/generated/${fbs_name}_generated.h")
    
    add_custom_command(
        OUTPUT ${generated_header}
        COMMAND flatc --cpp -o ${CMAKE_CURRENT_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR}/${fbs}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${fbs} flatc
        COMMENT "Generating FlatBuffers C++ headers for ${fbs}"
    )
    list(APPEND GENERATED_HEADERS ${generated_header})
endforeach()

add_custom_target(generate_schemas DEPENDS ${GENERATED_HEADERS})

add_library(schemas_lib INTERFACE)
target_include_directories(schemas_lib INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(schemas_lib INTERFACE flatbuffers)
add_dependencies(schemas_lib generate_schemas)

install(TARGETS schemas_lib EXPORT egbosTargets)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated/ DESTINATION include/schemas/generated FILES_MATCHING PATTERN "*.h")
