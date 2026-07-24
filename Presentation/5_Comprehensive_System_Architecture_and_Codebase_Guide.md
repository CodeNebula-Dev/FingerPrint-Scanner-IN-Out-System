# Comprehensive System Architecture, Codebase & Progress Guide

> **Document Version**: 2.0  
> **Last Updated**: July 2026  
> **Target Audience**: Core Development Team, Academic Supervisors, Project Reviewers  
> **Scope**: Complete Architectural Specification, Code Module Analysis, State Machine Models, and Progress Milestones

---

## 1. Executive Summary & Project Identity

### 1.1 Project Vision
The **Campus Biometric Gate Entry Management System** is an end-to-end, high-performance access control and attendance tracking platform designed for educational institutions. The system replaces legacy paper logbooks, RFID proximity cards, and touch-screen biometric terminals with a **zero-friction, hardware-software co-designed gateway**.

### 1.2 The Core Problem
Conventional campus access control systems face four critical operational failure points:
1. **Gate Bottlenecks & Transaction Latency**: Manual paper registers require 15–30 seconds per student. Biometric terminals with manual "IN/OUT" soft-buttons add 3–5 seconds of user decision latency per transaction, creating severe peak-hour queueing ($\rho > 1$).
2. **Hardware Duplication Cost**: The industry-standard solution for direction tracking (e.g., ZKTeco, Hikvision, Suprema) requires deploying **two separate physical readers** per gate lane—one dedicated entry reader and one dedicated exit reader. Enterprise anti-passback systems (e.g., HID Mercury, Lenel OnGuard) require dual infrared (IR) beam-break sensors spaced 30–100mm apart.
3. **Proxy & Buddy Punching Vulnerabilities**: Token-based systems (RFID, Mifare) allow students to swipe cards for absent peers. Mobile GPS geofencing applications are susceptible to location-spoofing software.
4. **Multi-Day Extended Absence Anomalies**: Standard daily attendance logs treat each 24-hour cycle in isolation. When a hosteller leaves for home over a weekend, traditional systems generate false curfew violation alerts on subsequent nights.

### 1.3 Key Innovation: Zero-Friction Residency-Aware Parity State Machine
Our primary algorithmic contribution is a **software-only direction detection paradigm**. By combining student residency metadata (`is_hosteller`) with transaction scan count parity, the system dynamically derives movement direction (`IN` vs `OUT`) upon a single fingerprint scan—**requiring zero manual button input and zero additional sensor hardware**.

---

## 2. End-to-End System Architecture

The system utilizes a **Decoupled Dual-Tier Hybrid Architecture**, separating low-level biometric hardware interaction and binary data processing from high-level administrative orchestration.

```
                  ┌─────────────────────────────────────────────────────────┐
                  │                 PHYSICAL GATE HARDWARE                  │
                  │   Biometric Sensor (Touch ID / R307 / AS608 Optical)    │
                  └────────────────────────────┬────────────────────────────┘
                                               │ Raw Live Template (512 Bytes)
                                               ▼
┌───────────────────────────────────────────────────────────────────────────────────────────┐
│                               PERFORMANCE TIER (C++ ENGINE)                               │
│                                                                                           │
│  ┌──────────────────────┐   ┌──────────────────────┐   ┌───────────────────────────────┐  │
│  │   fingerprint.cpp    │   │     indexer.cpp      │   │        serializer.cpp         │  │
│  │ Biometric Matcher    │───│ FNV-1a Hash Index    │───│ Binary POD Serialization      │  │
│  │ (Threshold: 0.75)    │   │ (master_index.dat)   │   │ (Direct struct memcpy read)   │  │
│  └──────────────────────┘   └──────────────────────┘   └───────────────────────────────┘  │
│             │                          │                               │                  │
│             ▼                          ▼                               ▼                  │
│  ┌─────────────────────────────────────────────────────────────────────────────────────┐  │
│  │                             BINARY DATABASE FILESYSTEM                              │  │
│  │  Student_data/           Everyday_data/            Home_data/       Rejection_log/ │  │
│  │  {batch}/{roll}.dat      {YYYY}/{MM}/{date}.dat    home_active.dat  rejections.dat │  │
│  └─────────────────────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────┬────────────────────────────────────────────┘
                                               │ C-Bindings Interop Bridge
                                               │ (ctypes / pybind11 shared library)
                                               ▼
┌───────────────────────────────────────────────────────────────────────────────────────────┐
│                        APPLICATION & ADMIN TIER (PYTHON CONTROL PANEL)                    │
│                                                                                           │
│  ┌──────────────────────┐   ┌──────────────────────┐   ┌───────────────────────────────┐  │
│  │  Admin Dashboard UI  │   │ Curfew Audit Engine  │   │  Excel Archival Generator     │  │
│  │  Student Management  │   │  (18:30 Auto-Audit)  │   │  (openpyxl Date Partitioning) │  │
│  └──────────────────────┘   └──────────────────────┘   └───────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────────────────────────────┘
```

### 2.1 Performance Tier (C++ Core)
- **Role**: Handles critical-path latency operations: raw biometric template matching, binary serialization, indexing, file I/O, and hardware sensor integration.
- **Key Design Principles**:
  - **Zero Dynamic Allocation in Critical Loop**: Uses Plain Old Data (POD) fixed-width C-structs (`StudentRecord`, `LogEntry`, `HomeRecord`).
  - **Direct Memory I/O**: `fstream::read()` and `fstream::write()` copy binary structures directly between RAM and disk without text/JSON parsing overhead.
  - **Native Hardware Bindings**: Objective-C++ wrapper (`touch_id.mm`) binds Apple macOS `LocalAuthentication.framework` for local development and testing.

### 2.2 Application & Admin Tier (Python Control Panel)
- **Role**: Provides administrative management interface, automated nightly curfew reporting, extended leave queue management, and Excel export tools.
- **Inter-Layer Bridge**: Python interfaces with compiled C++ shared libraries (`.dylib` on macOS / `.so` on Linux / `.dll` on Windows) via standard `ctypes` or `pybind11` wrapper modules.

---

## 3. Formal State Machine & Logic Models

### 3.1 Residency-Aware Parity State Machine

The core transition logic depends on two parameters:
1. **Residency Classification**: `is_hosteller` ($\text{True} = \text{Hosteller}$, $\text{False} = \text{Day Scholar}$).
2. **Daily Scan Parity**: $Count = \text{number of scans recorded for student today}$.

#### Initial Default Residency States:
- **Hosteller**: Default physical state is **INSIDE campus**.
- **Day Scholar**: Default physical state is **OUTSIDE campus**.

#### Formal Direction Matrix:

$$\text{Status}(Count, \text{is\_hosteller}) = \begin{cases} 
\text{OUT} & \text{if } \text{is\_hosteller} = \text{True} \text{ and } Count \pmod 2 \neq 0 \\
\text{IN} & \text{if } \text{is\_hosteller} = \text{True} \text{ and } Count \pmod 2 = 0 \\
\text{IN} & \text{if } \text{is\_hosteller} = \text{False} \text{ and } Count \pmod 2 \neq 0 \\
\text{OUT} & \text{if } \text{is\_hosteller} = \text{False} \text{ and } Count \pmod 2 = 0 
\end{cases}$$

#### State Transition Execution Flow:

```
[START SCAN]
     │
     ▼
Biometric Authentication (fingerprint_match)
     │
     ├─► Match Failed ────────► Log Rejection & Abort
     │
     └─► Match Success
             │
             ▼
Check Home Database (home_exists)
     │
     ├─► Exists in Home DB ────► Remove from Home DB ──► Set Status: IN ("Home Return") ──► Done
     │
     └─► Not in Home DB
             │
             ▼
Fetch Daily Log (log_get_entry)
     │
     ├─► First Scan Today ────► Set gate_count = 1
     │
     └─► Subsequent Scan ─────► Increment gate_count (gate_count++)
             │
             ▼
Evaluate Parity Equation
     │
     ├─► Hosteller + Odd Count ────► Status: OUT
     ├─► Hosteller + Even Count ───► Status: IN
     ├─► Day Scholar + Odd Count ──► Status: IN
     └─► Day Scholar + Even Count ─► Status: OUT
             │
             ▼
Check Curfew Violation (Hosteller + Status IN + Time > 18:30)
     │
     ├─► True ─────────────────► Mark late_return = true
     └─► False ────────────────► Regular entry
             │
             ▼
Persist Log Entry (log_add_entry / log_update_entry)
```

### 3.2 HOME Database Multi-Day Absence Reconciliation

When a student leaves campus for home, standard parity tracking across 24-hour daily log boundaries would break. Our system addresses this through the **Temporary Reconciliation Queue** (`HomeRecord`):

1. **Leave Approval**: Student scans out selecting "Home" as purpose $\rightarrow$ Administrator approves request $\rightarrow$ `HomeRecord` is written to `Home_data/home_active.dat`.
2. **Absence State**: While active in `Home_data/home_active.dat`, the student is excluded from nightly curfew audit processing.
3. **Return Reconciliation**: Upon returning to campus days later, the student scans their fingerprint. The matcher identifies the student, `home_exists()` returns `True`, the record is removed from `home_active.dat`, and an entry is created in today's log with status `IN` and reason `Home Return`. Parity tracking resumes cleanly without manual administrative intervention.

### 3.3 Automated Curfew Anomaly Audit (Default Threshold: 18:30)

Every evening at 18:30, the system executes an automated audit:
1. Loads today's active binary log partition (`Everyday_data/YYYY/MM_Month/DD_MM_YYYY.dat`).
2. Scans all entries where `is_hosteller = true`.
3. Identifies records where `gate_count` is **odd** (indicating physical state is currently `OUT`).
4. Cross-checks candidates against `Home_data/home_active.dat`. If present, candidate is excluded.
5. Generates an actionable unaccounted-for report containing student name, roll number, hosteller batch, and emergency contact phone numbers.

---

## 4. Binary Storage & Database Filesystem Layout

To maintain sub-millisecond execution times without external database dependencies (e.g., MySQL, SQLite), the system utilizes a **hierarchical binary storage architecture**.

```
CS-Club project/
├── cpp_engine/
│   ├── include/
│   │   ├── engine.h             # Structural contracts & API declarations
│   │   ├── indexer.h            # In-memory index cache header
│   │   ├── serializer.h         # POD struct binary I/O header
│   │   └── touch_id.h           # macOS Touch ID hardware bridge header
│   ├── src/
│   │   ├── main2.0.cpp          # CLI Application & gate scan execution engine
│   │   ├── master_db.cpp        # Student master database CRUD operations
│   │   ├── daily_log.cpp        # Date-partitioned log lifecycle manager
│   │   ├── home_db.cpp          # Active home leave queue persistence
│   │   ├── fingerprint.cpp      # Biometric matching & rejection logger
│   │   ├── indexer.cpp          # FNV-1a hash generator & index manager
│   │   ├── serializer.cpp       # Raw binary buffer read/write routines
│   │   └── touch_id.mm          # Objective-C++ LocalAuthentication wrapper
│   └── CMakeLists.txt           # Build configuration file
├── Student_data/
│   ├── master_index.dat         # Binary master index of all enrolled students
│   └── {batch}_batch/
│       ├── index.dat            # Batch roll number list
│       ├── {roll_number}.dat    # Binary StudentRecord (680 bytes)
│       └── {roll_number}.fpt    # Binary fingerprint template (512 bytes)
├── Everyday_data/
│   └── {YYYY}/
│       └── {MM}_Month/
│           └── {DD_MM_YYYY}.dat # Date-partitioned daily LogEntry records
├── Home_data/
│   └── home_active.dat          # Binary array of active HomeRecord structs
└── Rejection_log/
    └── rejections_{date}.dat    # Binary record of failed scan attempts
```

### 4.1 Data Structure Specifications (`engine.h`)

#### 1. `StudentRecord` (Master Student File — 680 Bytes Fixed Width)
```cpp
struct StudentRecord {
    char roll_number[20];               // Unique Identifier (Primary Key)
    char name[100];                     // Full Name
    char program[20];                   // Degree Program ("BSc", "MSc", "PhD")
    char batch[10];                     // Admission Batch Year ("2026")
    int  year;                          // Current Academic Year (1, 2, 3...)
    char phone_number[15];              // Contact Number with country code
    bool is_hosteller;                  // true = Hosteller, false = Day Scholar
    uint8_t fingerprint_template[512];  // Biometric Minutiae Feature Vector
};
```

#### 2. `LogEntry` (Daily Transaction Record — ~730 Bytes Fixed Width)
```cpp
struct LogEntry {
    char roll_number[20];               // Student Roll Number
    char name[100];                     // De-normalized Full Name for fast export
    int  year;                          // Academic Year
    char reason[50];                    // Movement Purpose ("Market", "Home", etc.)
    int  gate_count;                    // Daily scan counter
    char status[10];                    // Computed State ("IN" or "OUT")
    bool late_return;                   // Curfew violation flag (true if past 18:30)
    char timestamps[20][25];            // Up to 20 daily timestamps ("YYYY-MM-DD HH:MM:SS")
    int  timestamp_count;               // Active timestamp count
};
```

#### 3. `HomeRecord` (Active Home Leave Entry — 151 Bytes Fixed Width)
```cpp
struct HomeRecord {
    char roll_number[20];               // Student Roll Number
    char name[100];                     // Student Full Name
    int  year;                          // Academic Year
    char phone_number[15];              // Emergency Contact Number
    char date_of_leaving[12];           // Date of Departure ("DD_MM_YYYY")
    char time_of_leaving[10];           // Time of Departure ("HH:MM:SS")
};
```

#### 4. `CachedFingerprint` (In-Memory Indexer Entry — 280 Bytes Fixed Width)
```cpp
struct CachedFingerprint {
    uint32_t hash;                      // 32-bit FNV-1a Hash of template
    char roll_number[20];               // Roll Number key
    char file_path[256];                // Relative filesystem path to student .dat
};
```

---

## 5. Codebase Module Deep-Dive & Execution Guide

### 5.1 `master_db.cpp` — Student Master Database Manager
- **Responsibilities**: Creates and updates student records, maintains batch directories (`Student_data/2026_batch/`), performs batch promotions (`batch_promote_all()`), and manages master indexing.
- **Key Functions**:
  - `student_add(const StudentRecord& record)`: Writes `{roll}.dat` and `{roll}.fpt`, updates batch index, and computes FNV-1a hash for `master_index.dat`.
  - `student_get(const char* roll_number, StudentRecord& record)`: Fetches student record via relative path stored in indexer cache.
  - `batch_promote_all()`: Increments `year` field across all enrolled students and auto-deletes graduated batches.

### 5.2 `fingerprint.cpp` — Biometric Matcher & Rejection Engine
- **Responsibilities**: Performs template comparison against enrolled students, enforces confidence thresholds, and logs failed scan attempts.
- **Key Parameters**:
  - `MATCH_THRESHOLD = 0.75f`: Configurable floating-point confidence score requirement.
- **Matching Pipeline**:
  1. Computes FNV-1a hash of incoming live scan.
  2. Iterates over candidate entries in `g_fingerprint_cache`.
  3. Deserializes student template and calculates byte-level match ratio:
     $$\text{Score} = \frac{\sum_{i=0}^{512} (t_1[i] == t_2[i])}{512}$$
  4. Returns `MatchResult` struct with top candidate, confidence score, and match multiplicity count (`match_count > 1` triggers duplicate template warning).
  5. If match fails, calls `rejection_log_write()` to write the raw timestamped 512-byte scan into `Rejection_log/rejections_{date}.dat`.

### 5.3 `daily_log.cpp` — Date-Partitioned Log Manager
- **Responsibilities**: Enforces hierarchical daily logging in `Everyday_data/YYYY/MM_Month/DD_MM_YYYY.dat`.
- **Key Functions**:
  - `log_create_day(const char* date_string)`: Auto-creates nested directories for year and month.
  - `log_add_entry()`, `log_update_entry()`, `log_get_entry()`: Binary serialization routines for daily student scan arrays.
  - `log_get_entries_in_range(start_date, end_date)`: Traverses daily partitions across arbitrary date ranges for historic reporting.

### 5.4 `home_db.cpp` — Active Home Leave Registry
- **Responsibilities**: Manages `Home_data/home_active.dat` binary storage.
- **Key Functions**:
  - `home_add(const HomeRecord& record)`: Appends an approved leave record.
  - `home_remove(const char* roll_number)`: Removes record upon student return scan.
  - `home_exists(const char* roll_number)`: Instant check used in primary scan pipeline to detect returning students.

### 5.5 `indexer.cpp` — FNV-1a Hashing & In-Memory Cache
- **Responsibilities**: Generates 32-bit FNV-1a hash values and maintains `g_fingerprint_cache` in RAM.
- **FNV-1a Algorithm Implementation**:
  ```cpp
  uint32_t compute_fnv1a_hash(const uint8_t* data, size_t length) {
      uint32_t hash = 2166136261u;
      for (size_t i = 0; i < length; ++i) {
          hash ^= data[i];
          hash *= 16777619u;
      }
      return hash;
  }
  ```

### 5.6 `serializer.cpp` — Direct Binary Struct Serialization
- **Responsibilities**: Encapsulates raw file I/O operations using POD struct memory interpretation.
- **Code Pattern**:
  ```cpp
  bool serialize_student(const std::string& filepath, const StudentRecord& student) {
      std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
      if (!file.is_open()) return false;
      file.write(reinterpret_cast<const char*>(&student), sizeof(StudentRecord));
      return file.good();
  }
  ```

### 5.7 `touch_id.mm` — macOS Hardware Biometric Wrapper
- **Responsibilities**: Integrates macOS native Touch ID sensor using Objective-C++ `LocalAuthentication.framework`.
- **Synchronization Pattern**: Uses `dispatch_semaphore_t` to block synchronous C++ execution while asynchronous Objective-C `evaluatePolicy:localizedReason:reply:` executes on Apple system UI thread.

### 5.8 `main2.0.cpp` — Core Execution Application & CLI Engine
- **Responsibilities**: Provides the primary user interface, menu driver, scan execution flow, parity status evaluation, curfew warning triggers, and interactive purpose selection.

---

## 6. Code Execution & Integration Walkthrough

### 6.1 Building the C++ Engine

#### Prerequisites:
- C++17 compliant compiler (`clang++` on macOS, `g++` on Linux/GCC, `MSVC` on Windows)
- CMake 3.15+ (optional, or direct compilation script)

#### Direct Compilation Command (macOS with Touch ID support):
```bash
cd cpp_engine
clang++ -std=c++17 -O3 \
    src/main2.0.cpp \
    src/master_db.cpp \
    src/daily_log.cpp \
    src/home_db.cpp \
    src/fingerprint.cpp \
    src/indexer.cpp \
    src/serializer.cpp \
    src/touch_id.mm \
    -Iinclude \
    -framework Foundation -framework LocalAuthentication \
    -o gate_engine
```

#### Shared Library Compilation for Python Interop (`ctypes` / `pybind11`):
```bash
clang++ -std=c++17 -O3 -shared -fPIC \
    src/master_db.cpp \
    src/daily_log.cpp \
    src/home_db.cpp \
    src/fingerprint.cpp \
    src/indexer.cpp \
    src/serializer.cpp \
    src/touch_id.mm \
    -Iinclude \
    -framework Foundation -framework LocalAuthentication \
    -o libengine.dylib
```

### 6.2 Running the Application
```bash
./gate_engine
```

#### Interactive CLI Menu Options:
```
======================================================================
  CAMPUS BIOMETRIC GATE ENTRY MANAGEMENT SYSTEM v2.0
======================================================================
  [1] Scan Fingerprint (Gate Access)
  [2] Enroll New Student Biometric
  [3] View Today's Gate Log
  [4] Run Curfew Audit (Unaccounted Hostellers)
  [5] Manage Approved Home Leaves
  [6] Master Student Database Operations
  [7] Administrative Utilities & Export
  [0] Exit Application
======================================================================
```

---

## 7. Project Progress & Development Milestones

### 7.1 Accomplished Milestones to Date

| Phase | Component | Status | Key Deliverable |
|---|---|---|---|
| **Phase 1** | C++ Binary Storage & Engine Core | **COMPLETED** | Fixed-width struct serialization (`engine.h`), master indexing (`indexer.cpp`), binary file layout (`master_db.cpp`). |
| **Phase 2** | Residency-Aware Parity FSM | **COMPLETED** | Software-only direction logic (`main2.0.cpp`), 18:30 curfew detection, `LogEntry` status tracking. |
| **Phase 3** | HOME Leave Queue & Reconciliation | **COMPLETED** | Active home database (`home_db.cpp`), auto-removal on return scan, curfew audit exclusion. |
| **Phase 4** | Hardware Interop & Touch ID Bridge | **COMPLETED** | Objective-C++ `LocalAuthentication` wrapper (`touch_id.mm`), rejection logging (`fingerprint.cpp`). |
| **Phase 5** | Academic & Supervisor Presentation Package | **COMPLETED** | Strategy guide (`1_Patent_...`), Related Work & M/M/1 Queueing review (`2_Related_...`), Supervisor Pitch (`3_Unique_...`), Performance Audit (`4_Performance_...`). |

### 7.2 Performance Verification Summary (from Performance Audit)

- **Software Processing Latency**: **< 200 ms** for student populations up to 1,000 enrolled students.
- **End-to-End Return Scan Throughput**: **~300–500 ms** (including 200–400 ms hardware capture), matching RFID card tap speed while offering biometric security.
- **Language Performance Differential**: C++ binary struct serialization executes **5–100× faster** than an equivalent Python text/JSON file parser.

### 7.3 Future Technical Roadmap

1. **Cryptographic Biometric Template Protection (ISO/IEC 24745)**:
   - Implement AES-256-CBC template encryption using student roll number salt before writing `.fpt` files to disk (detailed in [resources.md](file:///Users/devanshkhosla/Projects/CS-Club%20project/resources.md#L162-L235)).
2. **ISO/IEC 30107 Presentation Attack Detection (PAD)**:
   - Integrate hardware liveness detection metrics to combat silicone/print spoof attacks.
3. **Level-1 Active FNV-1a Pruning**:
   - Enable active hash pruning in `fingerprint.cpp` to reduce candidate sets by 50–90% before disk reads, scaling matching support to 10,000+ students.
4. **Socket-Based Multi-Gate Network Architecture**:
   - Expose the C++ engine over a local TCP/IP socket to support multi-terminal gate synchronization across multiple campus entry gates.

---

## 8. Summary of Presentation Documentation Hierarchy

All presentation materials are structured under the [Presentation/](file:///Users/devanshkhosla/Projects/CS-Club%20project/Presentation) directory:

1. [1_Patent_vs_Publishing_Strategy.md](file:///Users/devanshkhosla/Projects/CS-Club%20project/Presentation/1_Patent_vs_Publishing_Strategy.md) — Strategic IP decision framework, IPC/CPC search strings, target publication venues.
2. [2_Related_Work_and_Comparative_Analysis.md](file:///Users/devanshkhosla/Projects/CS-Club%20project/Presentation/2_Related_Work_and_Comparative_Analysis.md) — Literature review taxonomy, 12-dimension comparative matrix, anti-passback analysis, M/M/1 queueing theory model.
3. [3_Unique_Advantages_and_Supervisor_Pitch.md](file:///Users/devanshkhosla/Projects/CS-Club%20project/Presentation/3_Unique_Advantages_and_Supervisor_Pitch.md) — Core novelties, supervisor pitch script, anticipated Q&A table.
4. [4_Performance_Analysis_and_Latency_Audit.md](file:///Users/devanshkhosla/Projects/CS-Club%20project/Presentation/4_Performance_Analysis_and_Latency_Audit.md) — Step-by-step code execution audit, scaling projections, C++ vs Python benchmark comparison.
5. [5_Comprehensive_System_Architecture_and_Codebase_Guide.md](file:///Users/devanshkhosla/Projects/CS-Club%20project/Presentation/5_Comprehensive_System_Architecture_and_Codebase_Guide.md) — (This document) End-to-end unified specification, struct layouts, file-by-file codebase guide, and complete progress report.
