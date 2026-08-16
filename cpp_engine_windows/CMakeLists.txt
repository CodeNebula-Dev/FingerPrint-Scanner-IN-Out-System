cmake_minimum_required(VERSION 3.12)
project(gate_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR
        "This project is configured for Windows only "
        "(uses the WinBio API). Build it on Windows with the Windows SDK installed."
    )
endif()

set(ENGINE_SOURCES
    src/serializer.cpp
    src/master_db.cpp
    src/daily_log.cpp
    src/home_db.cpp
    src/indexer.cpp
    src/fingerprint.cpp
    src/windows_biometric.cpp
)

add_library(gate_engine SHARED ${ENGINE_SOURCES})
set_target_properties(gate_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_include_directories(gate_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gate_engine
    PRIVATE
        winbio
        comsuppw
)

add_executable(gate_cli src/main_windows.cpp)
target_link_libraries(gate_cli PRIVATE gate_engine)