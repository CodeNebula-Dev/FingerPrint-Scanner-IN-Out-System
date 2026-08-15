# Implementation Plan - Final Hardware C++ Engine (Phases 1-3)

This document outlines the step-by-step plan to implement the production-ready C++ database and biometric matching engine for our fingerprint scanning hardware.

---

## 1. Architectural Changes: Touch ID Simulation vs. Hardware Engine

The previous engine utilized macOS Touch ID for testing, which introduced platform-specific code and dependencies. For the final hardware deployment, we will build a pure, cross-platform C++ library designed to compile and run on any target host (e.g., Raspberry Pi, embedded controllers, standard PCs running Linux/Windows/macOS) that interfaces with the physical fingerprint scanner.

### key Modifications:
* **Remove macOS Touch ID Objective-C++ files:** Omit [touch_id.h](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/include/touch_id.h) and [touch_id.mm](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/touch_id.mm) entirely.
* **Standard C++17 only:** Rely strictly on standard C++ libraries (`<iostream>`, `<fstream>`, `<filesystem>`, `<chrono>`, etc.).
* **Pure API Signatures:** Retain all previous function names, parameters, and structural contracts so that the Python Bridge (Phases 4-5) can interface with it without code modifications.
* **Hardware-Independent CLI Testing:** Create an updated command-line interface (`gate_cli`) that simulates fingerprint scans by loading mock 512-byte template files or taking keyboard inputs, allowing verification without macOS Touch ID.

---

## 2. Directory Layout of the C++ Engine

The implementation will be structured under the new directory [Final_cppp_enginge](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_enginge):

```
Final_cppp_enginge/
│
├── include/
│   ├── engine.h               <-- Global constants, struct mappings, API declarations
│   ├── serializer.h           <-- Binary disk serialization / deserialization helpers
│   └── indexer.h              <-- In-memory fingerprint template cache & FNV-1a declarations
│
├── src/
│   ├── master_db.cpp          <-- Student profile CRUD and batch updates
│   ├── daily_log.cpp          <-- Daily log filesystem structures and operations
│   ├── home_db.cpp            <-- Temporary database for approved home leaves
│   ├── serializer.cpp         <-- Sequential raw binary file read/write logic
│   ├── indexer.cpp            <-- Global RAM index mapping hashes to files
│   ├── fingerprint.cpp        <-- Level 1 (coarse hash filter) & Level 2 (similarity) matching
│   └── main.cpp               <-- Standard testing command-line interface (without Touch ID)
│
├
└── CMakeLists.txt             <-- Platform-agnostic CMake file compiles the engine and CLI
```

---

## 3. Data Structures & API Specifications

To guarantee compatibility with the Python Bridge, all structs and API definitions in [engine.h](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_enginge/include/engine.h) will remain identical:

### 3.1 Data Structures
```cpp
const int MAX_TIMESTAMPS = 20;

// Master Student Record
struct StudentRecord {
    char roll_number[20];               // Unique student identifier (Primary Key)
    char name[100];                     // Full name of the student
    char program[20];                   // "BSc", "MSc", "PhD", etc.
    char batch[10];                     // Batch year (e.g. "2025")
    int  year;                          // Academic year (1, 2, 3...)
    char phone_number[15];              // Contact details
    bool is_hosteller;                  // true = hosteller, false = day scholar
    uint8_t fingerprint_template[512];  // 512-byte biometric template
};

// Daily Gate Activity Log Entry
struct LogEntry {
    char roll_number[20];
    char name[100];
    int  year;
    char reason[50];                    // Reason for exit/entry
    int  gate_count;                    // Scans today
    char status[10];                    // "IN" or "OUT"
    bool late_return;                   // Flagged if entry after curfew hour
    char timestamps[MAX_TIMESTAMPS][25]; // Format: "YYYY-MM-DD HH:MM:SS"
    int  timestamp_count;
};

// Approved Home Leave Entry
struct HomeRecord {
    char roll_number[20];
    char name[100];
    int  year;
    char phone_number[15];
    char date_of_leaving[12];           // "DD-MM-YYYY"
    char time_of_leaving[10];           // "HH:MM:SS"
};

// Match Result Packages
struct MatchResult {
    bool   matched;
    char   roll_number[20];
    char   name[100];
    char   program[20];
    char   batch[10];
    int    year;
    char   phone_number[15];
    bool   is_hosteller;
    float  confidence_score;
    int    match_count;                 // Count of matching records above threshold
};
```

### 3.2 C++ Engine API Function List
```cpp
// Core Lifecycle
bool engine_init(const char* project_root_path);
void engine_shutdown();
bool engine_wipe_all_data();

// Student Profile CRUD (Master DB)
bool student_add(const StudentRecord& record);
bool student_remove(const char* roll_number);
bool student_update(const char* roll_number, const StudentRecord& updated_record);
bool student_get(const char* roll_number, StudentRecord& record);
std::vector<StudentRecord> student_list_by_batch(const char* batch);
std::vector<StudentRecord> student_list_all();
int  batch_promote(const char* batch);
int  batch_promote_all();
bool batch_delete(const char* batch);

// Biometric Functions
MatchResult fingerprint_match(const uint8_t* live_scan, int scan_length);
bool fingerprint_enroll(const char* roll_number, const uint8_t* template_data, int length);

// Daily Logs Management
bool log_create_day(const char* date_string);
bool log_day_exists(const char* date_string);
bool log_add_entry(const char* date_string, const LogEntry& entry);
bool log_update_entry(const char* date_string, const char* roll_number, const LogEntry& updated_entry);
bool log_get_entry(const char* date_string, const char* roll_number, LogEntry& entry);
std::vector<LogEntry> log_get_all_entries(const char* date_string);
std::vector<LogEntry> log_get_entries_in_range(const char* start_date, const char* end_date);
bool log_delete_day(const char* date_string);

// Home Leaves database
bool home_add(const HomeRecord& record);
bool home_remove(const char* roll_number);
bool home_exists(const char* roll_number);
std::vector<HomeRecord> home_get_all();

// Rejections logging
bool rejection_log_write(const char* date_string, const uint8_t* failed_scan, int scan_length);
```

---

## 4. Phase-by-Phase Implementation Plan

### Phase 1: Storage Architecture & Student Profiles (Master DB)
* **Objective:** Establish binary serialization, FNV-1a biometric hash caching, and profile management.
* **Steps:**
  1. Implement `serializer.h/cpp` utilizing standard binary file operations (`std::ofstream`/`std::ifstream` in binary mode).
  2. Implement `indexer.h/cpp` to cache roll numbers and fingerprint template hashes (using FNV-1a) in RAM. This allows immediate coarse filters without heavy disk reads.
  3. Implement `master_db.cpp` with batch promotion, batch deletion, and batch directories (e.g. `Student_data/2026_batch/`).
  4. Ensure directories are created automatically in `engine_init()`.

### Phase 2: Everyday Gate Logs & Leaves Registries
* **Objective:** Manage daily log files and home-leave temporary stores.
* **Steps:**
  1. Implement `daily_log.cpp` to store daily files nested inside chronological directories: `Everyday_data/YYYY/MM_Month/DD_MM_YYYY.dat`.
  2. Implement range queries and log CRUD utilities.
  3. Implement `home_db.cpp` to manage the active gone-home list inside `Home_data/home_active.dat`.

### Phase 3: Biometric Matching & Curfew Checks
* **Objective:** Perform Level-1 and Level-2 matching, rejection logging, and curfew compliance check.
* **Steps:**
  1. Implement `fingerprint.cpp`. The matching logic includes:
     - **Level-1 Coarse Filter:** Filters candidates in the in-memory index.
     - **Level-2 Matcher:** Computes byte similarity scores. If similarity >= 0.75 (75% match), registers a match. Handles multiple matching templates.
     - **Rejection Logger:** Appends unrecognized template scans to `Rejection_log/rejections_DD_MM_YYYY.dat`.
  2. Implement `main.cpp` providing the menu interface. Since Touch ID is omitted, biometric scans will be simulated by matching mock templates dynamically generated from students' roll numbers or importing template byte sequences.

---

## 5. Verification Plan

### 5.1 Compilation & Build
We will compile the project using standard CMake:
```bash
cd "/Users/devanshkhosla/Projects/Test folder/Final_cppp_enginge"
mkdir -p build && cd build
cmake ..
make
```
Verify that the output contains:
* `libgate_engine.dylib` (Shared library for macOS/Unix targets)
* `gate_cli` (Cross-platform terminal testing utility)

### 5.2 Manual Test Sequence
1. Run `./gate_cli`.
2. **Enroll Students:** Enroll one Hosteller and one Day Scholar (details stored as binary `.dat` and `.fpt` fingerprint files).
3. **Simulate Scans:** Trigger fingerprint matching by selecting a student's mock template profile.
4. **Log Analysis:** Verify that daily logs are written correctly in `Everyday_data/` and that the gone-home list functions as intended.
5. **Curfew & Late Returns:** Simulate out-of-bounds gate counts and curfew violations to verify flagging logic.


# Final Hardware C++ Engine Implementation Plan (Phases 1-3)

We are transitioning from the macOS Touch ID simulation codebase to the final cross-platform C++ engine built for actual fingerprint scanning hardware. Since a separate team is developing the Python Bridge (Phases 4-5), our goal is to construct the core C++ database and matching engine as a standalone, standard C++17 library. All struct structures and function names must be preserved exactly as they were in the previous macOS version.

## User Review Required

> [!IMPORTANT]
> **Removal of macOS Touch ID Dependency**
> The Objective-C++ interface (`touch_id.h` and `touch_id.mm`) linking to macOS `LocalAuthentication.framework` will be omitted from the final hardware engine. This makes the database and matching code pure standard C++17, allowing compilation on any platform (Linux, embedded Linux, macOS, or Windows).

> [!IMPORTANT]
> **Live-Scan Identification & Security**
> * **Live-Scan Search:** The scanner reads the fingerprint template, and the engine identifies the student **purely by matching the live template** against the database (no manually inputted ID or roll number required at the gate). The UI immediately outputs their name and prompts for the next action.
> * **Separated Storage:** Fingerprint templates are written separately to `<roll_number>.fpt` files alongside the `<roll_number>.dat` student profile files to make it easy to audit and fetch.
> * **Biometric Encryption:** All fingerprint template bytes are cryptographically secured using standard block or stream ciphers (such as AES-256 or salted XOR-hash key rolling) before being written to disk (in both `.dat` and `.fpt` files) and decrypted only in memory when performing matching searches.

---

## Proposed Changes

We will create a new directory [Final_cppp_engine](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine) and implement the cross-platform source files.

### [C++ Core Engine]

#### [NEW] [engine.h](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/include/engine.h)
Defines the struct mappings and exports the API functions for database operations.

#### [NEW] [crypto.h](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/include/crypto.h) / [crypto.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/src/crypto.cpp)
Exposes encryption and decryption subroutines for standard C++ biometric template protection.

#### [NEW] [serializer.h](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/include/serializer.h) / [serializer.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/src/serializer.cpp)
Raw binary serialization/deserialization helpers, incorporating biometric encryption when writing to disk.

#### [NEW] [indexer.h](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/include/indexer.h) / [indexer.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/src/indexer.cpp)
In-memory caching of fingerprint template hashes using FNV-1a.

#### [NEW] [master_db.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/src/master_db.cpp)
Student profile CRUD operations, directories setup, batch promotion, and separate `.fpt` fingerprint writing.

#### [NEW] [daily_log.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/src/daily_log.cpp)
Chronological everyday gate movement logging inside `Everyday_data/YYYY/MM_Month/DD_MM_YYYY.dat`.

#### [NEW] [home_db.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/src/home_db.cpp)
Operations for active home leaves logged inside `Home_data/home_active.dat`.

#### [NEW] [fingerprint.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/src/fingerprint.cpp)
Level-1 coarse filtering and Level-2 template similarity matching on decrypted templates.

#### [NEW] [CMakeLists.txt](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/CMakeLists.txt)
Platform-agnostic CMake file compiling `libgate_engine` library and `gate_cli`.

#### [NEW] [main.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/Final_cppp_engine/src/main.cpp)
Testing console interface allowing fingerprint scanning simulation via mock templates.

---
