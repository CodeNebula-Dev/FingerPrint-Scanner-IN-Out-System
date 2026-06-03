cmake_minimum_required(VERSION 3.12)
project(gate_engine C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(ENGINE_SOURCES
    src/serializer.cpp
    src/master_db.cpp
    src/daily_log.cpp
    src/home_db.cpp
    src/indexer.cpp
    src/fingerprint.cpp
)

# Objective-C++ file for macOS Touch ID
if(APPLE)
    list(APPEND ENGINE_SOURCES src/touch_id.mm)
endif()

# Include directories
include_directories(include)

# Shared library
add_library(gate_engine SHARED ${ENGINE_SOURCES})

if(APPLE)
    # Link LocalAuthentication and Foundation frameworks on macOS
    find_library(LOCAL_AUTH_FRAMEWORK LocalAuthentication)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    target_link_libraries(gate_engine PRIVATE ${LOCAL_AUTH_FRAMEWORK} ${FOUNDATION_FRAMEWORK})
endif()

# CLI application for testing
add_executable(gate_cli src/main.cpp)
target_link_libraries(gate_cli PRIVATE gate_engine)
