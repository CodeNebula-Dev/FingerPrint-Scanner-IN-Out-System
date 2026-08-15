# Python Bridge

We are using the pybind11, a C++ header-only library, that exposes C++ types and functions to Python- and vice-versa.

## Why 'pybind11'?

- debuging is cleaner: errors show up in compile time.
- natively understands C++ types and functions.

## File structure

PROJECT1/  
├── cpp_engiene/  
│   ├── include/  
│   │   ├── dailylog.h  
│   │   ├── engine.h  
│   │   ├── fingerprint.h  
│   │   ├── home_db.h  
│   │   ├── indexer.h  
│   │   ├── main.h  
│   │   ├── master_db.h  
│   │   ├── serializer.h  
│   │   └── touch_id.h  
│   └── src/  
│       ├── dailylog.cpp  
│       ├── fingerprint.cpp  
│       ├── home_db.cpp  
│       ├── indexer.cpp  
│       ├── main.cpp  
│       ├── master_db.cpp  
│       ├── serializer.cpp  
│       └── touch_id.mm    <-- Objective-C++ file (macOS)  
├── bindings.cpp           <-- SINGLE pybind11 binding file  
├── setup.py               <-- Python build script  
└── pyproject.toml         <-- Build tool configuration  

## Why one single bindings.cpp rather than one for each file

| criteria | single bindings file | 8 individual bindings file |  
| --- | --- | --- |
| complexity | Very Simple. Requires 1 binding .cpp file in Pybind11Extension. | High Overhead. Must configure 8 sub-modules, management functions, or sub-extensions. |  
| module import in python | Clean single import: import fingerprint_engine | Fragmented imports (from fingerprint_engine import indexer, serializer) OR requires boilerplate init function calls in C++. |  
| Compiler Memory (RAM) | Low overall memory footprint; compiles once in a single translation unit. | High total memory usage due to pybind11 template instantiations running 8 separate times. |  
| Code Duplication | Shared pybind11 headers and type bindings (e.g., custom structs) are written once. | Boilerplate pybind includes and type conversions are duplicated across 8 files. |  
| Type Conversion Errors | Zero. Types registered once inside PYBIND11_MODULE are immediately visible to all functions. | High Risk. Passing types between different binding files can cause type resolution or duplicate definition errors. |  

## Why native setuptools (with pybind11Extensions) and not CMake?

FOr this project, Native fits better for the following reasons:

- No Dependancy Clashing: No Cmake to be installed.  
- Minimal Maintenance: You maintain just one setup.py and one pyproject.toml. You won't have to sync source file paths across C++ build scripts and Python build scripts.  
- Pure C++ Standard Library: SInce our code relies on standard headers (`<filesystem>`, `<chrono>`, `<fstream>`, `<cstring>`). Native setuptools handles all of these natively out of the box without needing CMake's target lookup.  

### When to switch to CMake?

- if we start linking heavy C++ libraries like OpenCV, CUDA, or Boost.
- if we want to build a standalone C++ executable out of the exact same codebase in addition to the Python package.  
