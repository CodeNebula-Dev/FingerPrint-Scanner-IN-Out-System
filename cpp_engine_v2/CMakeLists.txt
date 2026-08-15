cmake_minimum_required(VERSION 3.14)
project(gate_engine_v2 VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files for Version 2.0 Engine
set(ENGINE_SOURCES
    src/master_db.cpp
    src/daily_log.cpp
    src/home_db.cpp
    src/serializer.cpp
    src/indexer.cpp
    src/fingerprint.cpp
    src/crypto_placeholder.cpp
)

# Compile shared engine library for Python bridge & host binding
add_library(gate_engine_v2 SHARED ${ENGINE_SOURCES})

# Compile standalone CLI testing executable
add_executable(gate_cli_v2 ${ENGINE_SOURCES} src/main.cpp)

# Compile pipeline validation test executable
add_executable(test_pipeline ${ENGINE_SOURCES} src/test_pipeline.cpp)

# Target properties
set_target_properties(gate_engine_v2 PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

message(STATUS "Configured C++ Biometric Matching Engine v2.0 (Cancelable Encryption Architecture)")
