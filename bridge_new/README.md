# fingerprint_engine — Python Bridge (`bridge_new/`)

Pybind11-based Python bindings for the **C++ Biometric Gate Engine v2.0 (Windows)** — the low-latency
database, indexing, and BioHashing biometric matching core of the Campus Biometric Gate Entry
Management System.

This module compiles the C++ engine directly into a native Python extension (`fingerprint_engine.pyd`
on Windows), so Python application code can call the engine's functions as if they were ordinary
Python calls, with no serialization or IPC overhead.

---

## Why pybind11?

- **Compile-time safety** — type mismatches between Python and C++ are caught when the extension is
  built, not at runtime.
- **Native understanding of C++ types** — structs, `std::vector`, and STL containers convert to/from
  Python automatically, with explicit control where custom conversion is needed (e.g. fixed-size
  `char[]` buffers exposed as Python `str`/`bytes` properties).

## Why a modular `bindings/` folder instead of one `bindings.cpp`?

| Metric | Monolithic file | Modular (`bindings/`) |
| --- | --- | --- |
| Incremental build time | Slow — recompiles everything | Fast — only touched files recompile |
| Compiler memory usage | High | Distributed / lower peak |
| Readability | Degrades past ~500–1500 lines | Each file stays 50–100 lines |
| Git merge conflicts | High risk | Low risk (separate files per subsystem) |

The engine exposes 6 distinct subsystems (types, lifecycle, database, biometric, indexer, serializer),
so splitting bindings by subsystem keeps each translation unit small and isolates platform-specific
code (Windows biometric hardware) from the rest.

---

## Directory Structure

``` txt
bridge_new/
├── include/
│   ├── engine.h                 # Core structs (StudentRecord, LogEntry, HomeRecord, MatchResult) + API
│   ├── indexer.h                # In-memory FNV-1a hash indexer
│   ├── serializer.h             # Binary disk I/O declarations
│   ├── crypto_placeholder.h     # Pluggable cancelable-biometric encryption interface (BioHashing)
│   └── windows_biometric.h      # Windows Hello / COM scanner hardware interface
├── src/
│   ├── master_db.cpp            # Engine lifecycle + Student CRUD
│   ├── daily_log.cpp            # Date-partitioned gate log storage
│   ├── home_db.cpp              # Approved home-leave registry
│   ├── fingerprint.cpp          # Dual-tier biometric matching (Level-1 hash + Level-2 BioHash)
│   ├── indexer.cpp              # FNV-1a hashing + RAM candidate index
│   ├── serializer.cpp           # Raw binary struct read/write
│   ├── crypto_placeholder.cpp   # BioHashing (random projection + binarization) implementation
│   ├── windows_biometric.cpp    # Windows Hello popup / COM scanner / simulation fallback
│   ├── main_windows.cpp         # Standalone CLI test harness (not bound to Python)
│   └── test_pipeline.cpp        # Automated integration test binary (not bound to Python)
├── bindings/
│   ├── bindings.h               # Shared prototypes + safe buffer-extraction helper
│   ├── main_module.cpp          # PYBIND11_MODULE entry point
│   ├── bind_types.cpp           # StudentRecord, LogEntry, HomeRecord, MatchResult, IndexEntry, BioHashConfig
│   ├── bind_engine.cpp          # engine_init / engine_shutdown / engine_wipe_all_data
│   ├── bind_database.cpp        # Student, Daily Log, and Home Leave CRUD
│   ├── bind_biometric.cpp       # fingerprint_match/enroll, hardware scanner, key management
│   ├── bind_indexer.cpp         # In-memory candidate indexer & hashing
│   └── bind_serializer.cpp      # Low-level binary file serialization
├── CMakeLists.txt               # Build config (scikit-build-core, dual .pyd + .exe targets)
├── pyproject.toml               # PEP 517/518 build config (`pip install .`)
└── test_bridge.py               # Manual interactive smoke-test script
```

### Mapping: implementation → binding → Python API

| Implemented in `src/` | Bound in `bindings/` | Exposed to Python |
| --- | --- | --- |
| `master_db.cpp` (lifecycle) | `bind_engine.cpp` | `engine_init()`, `engine_shutdown()`, `engine_wipe_all_data()` |
| `master_db.cpp` (students), `home_db.cpp`, `daily_log.cpp` | `bind_database.cpp` | `student_add/get/list_*`, `home_add/remove/exists`, `log_create_day/add_entry/...` |
| `fingerprint.cpp`, `windows_biometric.cpp`, `crypto_placeholder.cpp` | `bind_biometric.cpp` | `fingerprint_match/enroll`, `windows_biometric_authenticate`, `crypto_*` |
| `indexer.cpp` | `bind_indexer.cpp` | `indexer_insert`, `indexer_lookup_candidates`, `indexer_hash_template` |
| `serializer.cpp` | `bind_serializer.cpp` | `serializer_write_student`, `serializer_read_log_entries`, etc. |
| `main_windows.cpp`, `test_pipeline.cpp` | *not bound* | Standalone C++ executables only (`gate_cli`, `test_pipeline`) |

---

## Data Model

All records are fixed-width, POD C structs serialized directly to binary — no JSON/text parsing.

| Struct | Approx. Size | Purpose |
| --- | --- | --- |
| `StudentRecord` | ~656 B | Master profile: roll no., name, program, batch, year, phone, `is_hosteller`, 512-byte encrypted biometric template |
| `LogEntry` | ~730 B | One day's gate activity for one student: reason, scan count, status, late flag, up to 20 timestamps |
| `HomeRecord` | ~151 B | Active approved home-leave permit |
| `MatchResult` | — | Result of a biometric scan: matched student's identity, confidence score, duplicate count |
| `IndexEntry` | — | RAM-only: roll number → 64-bit FNV-1a template hash |
| `BioHashConfig` | — | Active encryption seed / projection dimension / key-loaded status |

Biometric templates are never stored in the clear: `crypto_enroll_transform()` (BioHashing — random
orthogonal projection + binarization) converts raw scans into a non-invertible 512-byte representation
before it ever touches disk, in line with ISO/IEC 24745.

---

## Building

Requires a C++20 compiler, CMake ≥ 3.15, and `pybind11` (via `pip` or `find_package`).

```bash
cd bridge_new
pip install .
```

This uses `scikit-build-core` (declared in `pyproject.toml`) to drive the CMake build and produces:

- `fingerprint_engine` — the importable Python extension module
- `gate_cli` — a standalone interactive CLI binary (menu-driven scan/enroll/audit tool)
- `test_pipeline` — an automated enrollment/matching/noise-tolerance integration test binary

On Windows, the extension links against `winbio`, `comsuppw`, `credui`, `advapi32`, and `bcrypt` for
Windows Hello and hardware scanner support.

### Why CMake + scikit-build-core over `setup.py`?

| Metric | CMakeLists.txt (scikit-build-core) | setup.py |
| --- | --- | --- |
| Architecture | C++-first: full control of compilers/linkers | Python-first: C++ wrapped in setuptools |
| Cross-platform conditionals | Native `if(WIN32)` / `if(UNIX)` | Manual `sys.platform` branching |
| System library linking | Declarative `target_link_libraries(...)` | Imperative `libraries=[...]` lists |
| Multiple build targets | Builds `.pyd` **and** standalone `.exe` binaries in one pass | Cannot easily build standalone executables alongside the extension |

---

## Quick Start

```python
import fingerprint_engine as fe

# 1. Initialize the engine (creates db_root/ with Student_data, Everyday_data, Home_data, Rejection_log)
fe.engine_init("./")

# 2. Enroll a student
student = fe.StudentRecord()
student.roll_number = "2026B001"
student.name = "ABC"
student.program = "BSc"
student.batch = "2026"
student.year = 1
student.phone_number = "1234567890"
student.is_hosteller = True
fe.student_add(student)

raw_template = bytes(range(256)) * 2  # 512 bytes from a real scanner in production
fe.fingerprint_enroll(student.roll_number, raw_template)

# 3. Match a live scan against the database
result = fe.fingerprint_match(raw_template)
if result.matched:
    print(f"Identified: {result.name} ({result.roll_number}) score={result.confidence_score:.2f}")

# 4. Gate log workflow
today = "29_08_2026"
fe.log_create_day(today)
entry = fe.LogEntry()
entry.roll_number = result.roll_number
entry.name = result.name
entry.year = result.year
entry.gate_count = 1
fe.log_add_entry(today, entry)

# 5. Shut down cleanly
fe.engine_shutdown()
```

---

## API Overview

| Category | Key functions |
| --- | --- |
| **Lifecycle** | `engine_init(path)`, `engine_shutdown()`, `engine_wipe_all_data()` |
| **Student CRUD** | `student_add/remove/update/get/list_by_batch/list_all`, `batch_promote/batch_promote_all/batch_delete` |
| **Daily Logs** | `log_create_day`, `log_day_exists`, `log_add_entry`, `log_update_entry`, `log_get_entry`, `log_get_all_entries`, `log_get_entries_in_range`, `log_delete_day` |
| **Home Leave Registry** | `home_add`, `home_remove`, `home_exists`, `home_get_all` |
| **Biometric Matching** | `fingerprint_match(live_scan)` → `MatchResult`, `fingerprint_enroll(roll, template)`, `rejection_log_write` |
| **Hardware / Windows Hello** | `windows_biometric_authenticate`, `windows_capture_template`, `windows_set_com_port`, `windows_list_com_ports` |
| **BioHash Key Management** | `crypto_get_scheme_name`, `crypto_get_active_scheme`, `crypto_set_key`, `crypto_rotate_key` |
| **In-Memory Indexer** | `indexer_init/clear/insert/remove`, `indexer_hash_template`, `indexer_lookup_candidates` |
| **Raw Serialization** | `serializer_write_student/read_student`, `serializer_write_log_entry/read_log_entries`, `serializer_write_home_record/read_home_records`, `serializer_append_rejection` |

All functions returning a "found / not found" style result return a `(success: bool, value)` tuple
(e.g. `student_get`, `log_get_entry`, `windows_capture_template`) rather than raising on miss.
