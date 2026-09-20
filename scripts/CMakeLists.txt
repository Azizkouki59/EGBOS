# scripts/CMakeLists.txt
# Diagnostic and qualification binaries — not production; not included in ctest.

add_executable(live_pipeline_diagnostic
    live_pipeline_diagnostic.cpp
)

target_include_directories(live_pipeline_diagnostic PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(live_pipeline_diagnostic PRIVATE
    engine_core
    flatbuffers
    ssl
    crypto
)

target_compile_features(live_pipeline_diagnostic PRIVATE cxx_std_20)

add_executable(e2e_live_diagnostic
    e2e_live_diagnostic.cpp
)

target_include_directories(e2e_live_diagnostic PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(e2e_live_diagnostic PRIVATE
    engine_core
    engine_l0
    flatbuffers
    ssl
    crypto
)

target_compile_features(e2e_live_diagnostic PRIVATE cxx_std_20)

add_executable(egbos_binance_cli
    egbos_binance_cli.cpp
)

target_include_directories(egbos_binance_cli PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(egbos_binance_cli PRIVATE
    engine_core
    engine_l0
    engine_l1
    nlohmann_json::nlohmann_json
    ssl
    crypto
)

target_compile_features(egbos_binance_cli PRIVATE cxx_std_20)
