cmake_minimum_required(VERSION 3.14)
project(gate_engine_v2 VERSION 2.0.0 LANGUAGES CXX OBJCXX)

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

if (APPLE)
    list(APPEND ENGINE_SOURCES src/touch_id.mm)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(LOCALAUTH_FRAMEWORK LocalAuthentication REQUIRED)
endif()

# Compile shared engine library for Python bridge & host binding
add_library(gate_engine_v2 SHARED ${ENGINE_SOURCES})

# Compile standalone CLI testing executable
add_executable(gate_cli_v2 ${ENGINE_SOURCES} src/main.cpp)

# Compile pipeline validation test executable
add_executable(test_pipeline ${ENGINE_SOURCES} src/test_pipeline.cpp)

if (APPLE)
    target_link_libraries(gate_engine_v2 PRIVATE ${FOUNDATION_FRAMEWORK} ${LOCALAUTH_FRAMEWORK})
    target_link_libraries(gate_cli_v2 PRIVATE ${FOUNDATION_FRAMEWORK} ${LOCALAUTH_FRAMEWORK})
    target_link_libraries(test_pipeline PRIVATE ${FOUNDATION_FRAMEWORK} ${LOCALAUTH_FRAMEWORK})
endif()

# Target properties
set_target_properties(gate_engine_v2 PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

message(STATUS "Configured C++ Biometric Matching Engine v2.0 (Cancelable Encryption Architecture)")
