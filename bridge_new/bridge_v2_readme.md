# Python Bridge

We are using the pybind11, a C++ header-only library, that exposes C++ types and functions to Python- and vice-versa.

## Why 'pybind11'?

- debuging is cleaner: errors show up in compile time.
- natively understands C++ types and functions.

## SIngle bindings file vs multiple binding files

| Feature / Metric | Case 1: Monolithic (bindings.cpp) | Case 2: Modular (bindings/) |
| ------ | ------ | ------ |
| Initial Setup Complexity | Very Low (just 1 extra file) | Moderate (multiple files + forward declarations) |
| Incremental Build Time | Slow (recompiles the whole module on any small change) | Fast (only modified binding translation units recompile) |
| Compiler Memory Usage | High (heavy template instantiation in one Translation Unit) | Distributed (parallel compilation, lower peak RAM) |
| Code Readability & Navigability | Drops quickly as the API grows (can easily hit 500–1,500+ lines) | Clean and isolated (each module has a 50–100 line file) |
| Team Collaboration / Git Merges | High risk of Git merge conflicts in one file | Low risk (developers edit separate binding files) |
| Python Module Organization | Usually a flat namespace (e.g., import engine_py) | Supports flat or structured submodules (engine_py.biometric, engine_py.indexer) |

## File Structure

```.txt
cpp_engine_v2/
├── include/  
│   ├── crypto_placeholder.h  
│   ├── engine.h  
│   ├── indexer.h  
│   ├── serializer.h  
│   └── windows_biometric.h  
├── src/  
│   ├── crypto_placeholder.cpp  
│   ├── daily_log.cpp  
│   ├── fingerprint.cpp  
│   ├── home_db.cpp  
│   ├── indexer.cpp  
│   ├── master_db.cpp  
│   ├── serializer.cpp  
│   ├── windows_biometric.cpp  
|   └── main_windows.cpp  
├── bindings/                     <-- Dedicated binding directory  
│   ├── bindings.h                <-- Shared header with all prototypes & buffer helper
│   ├── main_module.cpp           <--PYBIND11_MODULE entry point (calls all init_* functions)  
│   ├── bind_types.cpp            <-- StudentRecord, LogEntry, HomeRecord, MatchResult classes
│   ├── bind_engine.cpp           <-- engine_init, engine_shutdown, engine_wipe_all_data  
│   ├── bind_indexer.cpp          <-- in-memory candidate indexer & hashing  
│   ├── bind_serializer.cpp       <-- binary file read/write serialization functions  
|   ├── bind_database.cpp         <-- Student, Home Leave, and Daily Log CRUD operations  
│   └── bind_biometric.cpp        <-- fingerprint_match, fingerprint_enroll, Windows COM/WinBio  
└── CMakeLists.txt / setup.py  
```

### Exact Mapping: src/ → bindings/

| C++ Implementation File in `src/` | Where it is bound in `bindings/` | What it exposes to Python |
| ---- | ---- | ---- |
| `master_db.cpp` (_Lifecycle_) | `bind_engine.cpp` | `engine_init()`, `engine_shutdown()`, `engine_wipe_all_data()` |
| `master_db.cpp` (_Students_), `home_db.cpp`, `daily_log.cpp` | `bind_database.cpp` | `student_add()`, `student_get()`, `student_list_all()`, `home_add()`, `log_create_day()`, `log_add_entry()`, etc. |
| `fingerprint.cpp`, `windows_biometric.cpp` | `bind_biometric.cpp` | `fingerprint_match()`, `fingerprint_enroll()`, `windows_biometric_authenticate()`, `windows_list_com_ports()` |
| `indexer.cpp` | `bind_indexer.cpp` | `indexer_insert()`, `indexer_lookup_candidates()`, `indexer_hash_template()` |
| `serializer.cpp` | `bind_serializer.cpp` | `serializer_write_student()`, `serializer_read_student()`, etc. |
| `crypto_placeholder.cpp` | _Used internally by_ `fingerprint.cpp` | Key management / Admin helpers in `bind_biometric.cpp` |
| `main_windows.cpp` | **Not Bound** | This contains int main() (the standalone C++ CLI application). In Python, your Python app acts as `main`. |

### Why multiple bindings files

- expose multiple subsystems (Biometrics, Indexer, Serializer, Database).
- fast incremental build times during rapid Python/C++ debugging.
- isolate platform-specific logic (windows_biometric).

## CMakeLists.txt vs setup.py

| Comparison Metric | CMakeLists.txt (via scikit-build-core) | setup.py (setuptools / Pybind11Extension) |
| ---- | ---- | ---- |
| Core Architecture Focus | C++ First: Full control over compilers, flags, and linkers | Python First: Wrapped C++ builds inside Python distutils/setuptools |
| Cross-Platform Conditionals | Native & Clean: if(WIN32) vs if(UNIX) handles OS-specific source files | Manual Scripting: Requires custom sys.platform branching in Python |
| System Library Linking | Declarative: target_link_libraries(m PRIVATE winbio bcrypt) on Win, crypto on Linux | Imperative: Must pass lists to libraries=['winbio', 'bcrypt'] or parse system paths |
| Multiple Build Targets | Built-in: Builds both .pyd/.so module AND native test binaries (main_windows.cpp) | Complex: Cannot easily build standalone C++ executables alongside the extension |
