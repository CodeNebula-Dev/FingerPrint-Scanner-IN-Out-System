cmake_minimum_required(VERSION 3.15)
project(fingerprint_engine C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Supress MSVC legacy CRT warnings
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

# 1.Source Files Definition

# Core C++ Engine Logic
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

# PyBind11 Modular Bridge Sources
set(BINDING_SOURCES
    bindings/main_module.cpp
    bindings/bind_types.cpp
    bindings/bind_engine.cpp
    bindings/bind_database.cpp
    bindings/bind_biometric.cpp
    bindings/bind_indexer.cpp
    bindings/bind_serializer.cpp
)

# Common Include Directories
include_directories(include bindings)

# 2. Python PyBind11 Extension Module (fingerprint_engine.pyd)

find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(fingerprint_engine
    ${ENGINE_SOURCES}
    ${BINDING_SOURCES}
)

install(TARGETS fingerprint_engine LIBRARY DESTINATION . RUNTIME DESTINATION .)

if (WIN32)
    # Link Windows Biometric Framework, Crypto & COMlibraries
    target_link_libraries(fingerprint_engine PRIVATE winbio comsuppw credui advapi32 bcrypt)

    # MSVC template optimization flag for PyBind11
    if (MSVC)
        target_compile_options(fingerprint_engine PRIVATE /bigobj)
    endif()
endif()

# 3. Standalone C++ Executables (for standalone testing / debugging)

# Interactive Terminal CLI Menu App (gate_cli.exe)
add_executable(gate_cli ${ENGINE_SOURCES} src/main_windows.cpp)
if (WIN32)
    target_link_libraries(gate_cli PRIVATE winbio comsuppw credui advapi32 bcrypt)
endif()

# Automateḑ Integration Test Binary (test_pipeline.exe)
add_executable(test_pipeline ${ENGINE_SOURCES} src/test_pipeline.cpp)
if (WIN32)
    target_link_libraries(test_pipeline PRIVATE winbio comsuppw credui advapi32 bcrypt)
endif()

message(STATUS "Configured Biometric ENgine v2.0 (Dual Target: Python Extentions+ Native Executabbles)")