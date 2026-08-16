cmake_minimum_required(VERSION 3.12)
project(gate_engine C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT WIN32)
    message(FATAL_ERROR "This project is configured for Windows only "
                         "(uses the WinBio API). Build it on a Windows "
                         "machine with the Windows SDK installed.")
endif()

# Source files
set(ENGINE_SOURCES
    src/serializer.cpp
    src/master_db.cpp
    src/daily_log.cpp
    src/home_db.cpp
    src/indexer.cpp
    src/fingerprint.cpp
    src/windows_biometric.cpp
)

# Include directories
include_directories(include)

# Shared library
add_library(gate_engine SHARED ${ENGINE_SOURCES})
set_target_properties(gate_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Link WinBio (fingerprint biometrics) and comsuppw (for _com_error,
# used to turn HRESULTs into readable messages)
target_link_libraries(gate_engine PRIVATE winbio comsuppw)

# CLI application for testing
# add_executable(gate_cli src/main.cpp)
add_executable(gate_cli src/main_windows.cpp)
target_link_libraries(gate_cli PRIVATE gate_engine)
