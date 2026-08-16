cmake_minimum_required(VERSION 3.12)
project(gate_engine C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Suppress MSVC legacy CRT warnings
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

# Source files for Windows Engine v2.0
set(ENGINE_SOURCES
    src/serializer.cpp
    src/master_db.cpp
    src/daily_log.cpp
    src/home_db.cpp
    src/indexer.cpp
    src/fingerprint.cpp
    src/crypto_placeholder.cpp
    src/windows_biometric.cpp
)

# Include directories
include_directories(include)

# Shared library
add_library(gate_engine SHARED ${ENGINE_SOURCES})

# Link WinBio, CredUI, comsuppw, and advapi32 on Windows if building with MSVC
if (WIN32)
    target_link_libraries(gate_engine PRIVATE winbio comsuppw credui advapi32)
endif()

# CLI application for testing
add_executable(gate_cli ${ENGINE_SOURCES} src/main_windows.cpp)
if (WIN32)
    target_link_libraries(gate_cli PRIVATE winbio comsuppw credui advapi32)
endif()

# Pipeline validation test executable
add_executable(test_pipeline ${ENGINE_SOURCES} src/test_pipeline.cpp)
if (WIN32)
    target_link_libraries(test_pipeline PRIVATE winbio comsuppw credui advapi32)
endif()

message(STATUS "Configured C++ Biometric Matching Engine v2.0 (Windows Cancelable Encryption Architecture)")
