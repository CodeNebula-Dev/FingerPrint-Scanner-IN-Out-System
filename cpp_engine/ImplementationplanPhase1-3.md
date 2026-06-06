# Implementation Plan - C++ Database Engine & MacBook Touch ID Integration (Phases 1-3)

This plan outlines the implementation of the core C++ database engine (Phases 1 to 3) for the Campus Biometric Entry Management System, modified to support local testing using the MacBook's physical Touch ID scanner.

## User Review Required

> [!IMPORTANT]
> **macOS Touch ID Architectural Limitation**: 
> macOS Touch ID via the `LocalAuthentication` framework is a secure system that only returns a binary **Success/Failure** (indicating whether the current MacBook owner authenticated). It **does not expose raw fingerprint template bytes** or images to applications.
> 
> **Our Testing Solution**:
> To test the database engine's matching logic using the physical MacBook scanner:
> 1. We will implement the C++ database engine using standard 512-byte biometric templates.
> 2. We will compile a macOS Touch ID wrapper (`touch_id.mm`) that triggers the MacBook's physical scanner.
> 3. During testing, when a scan is requested, we will prompt the user to use Touch ID. If authentication succeeds, the CLI test utility will send a simulated 512-byte template (mapping to a selected student) into the C++ database engine to run the full matching and log-writing logic.
> This allows us to test the physical scanner interaction alongside the exact database logic.

> [!TIP]
> **Phone Number Storage Data Type**:
> In the original struct designs, `phone_number` was an `int`. Standard 10-digit phone numbers exceed the maximum value of a 32-bit signed integer (2,147,483,647). We propose changing this field to a character array `char phone_number[15]` to support international prefixes (e.g., "+91"), leading zeros, and prevent overflows.

---

## Proposed Changes

We will build the C++ database engine inside a new subdirectory `cpp_engine/` within the project root `/Users/devanshkhosla/Projects/Test folder`.

### [C++ Database Engine]

We will create a structured C++ database engine that compiles into a shared library (`libgate_engine.dylib` on macOS) and a test CLI app.

#### [NEW] [engine.h](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/include/engine.h)
Defines all structs, global constants, and API functions.
- `StudentRecord` (Master DB record with updated `char phone_number[15]`).
- `LogEntry` (Daily movement log record).
- `HomeRecord` (Temporary record of students gone home).
- `MatchResult` (Result returned to matching requests).
- API functions: `engine_init`, `engine_shutdown`, `student_add`, `student_remove`, `student_get`, `student_list_by_batch`, `student_list_all`, `batch_promote`, `batch_promote_all`, `batch_delete`, `fingerprint_match`, `fingerprint_enroll`, `log_create_day`, `log_add_entry`, `log_update_entry`, `log_get_entry`, `log_get_all_entries`, `log_get_entries_in_range`, `log_delete_day`, `home_add`, `home_remove`, `home_exists`, `home_get_all`, `rejection_log_write`.

#### [NEW] [serializer.h](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/include/serializer.h) / [serializer.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/serializer.cpp)
Handles binary serialization and deserialization of structs directly to disk files.

#### [NEW] [master_db.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/master_db.cpp)
Implements master database CRUD, directory generation, indices updates, and batch promotion logic.

#### [NEW] [daily_log.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/daily_log.cpp)
Implements daily gate crossing log operations within the `Everyday_data/YYYY/MM_Month/` folder structure.

#### [NEW] [home_db.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/home_db.cpp)
Implements operations for the active home-leave database (`Home_data/home_active.dat`).

#### [NEW] [indexer.h](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/include/indexer.h) / [indexer.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/indexer.cpp)
Maintains an in-memory cache of fingerprint hashes mapped to roll numbers for fast Level-1 search filtering, avoiding redundant disk reads during match requests.

#### [NEW] [fingerprint.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/fingerprint.cpp)
Implements Level 1 (Hash-based Coarse Filter) and Level 2 (Full template similarity score comparison) biometric matching.

---

### [macOS Touch ID Integration]

#### [NEW] [touch_id.h](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/include/touch_id.h) / [touch_id.mm](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/touch_id.mm)
An Objective-C++ module that links against macOS `LocalAuthentication.framework`.
- Exposes `bool macos_touch_id_authenticate(const char* prompt_reason)` to C++.
- Displays the native macOS Touch ID prompt to physical users.

---

### [Testing and Orchestration]

#### [NEW] [CMakeLists.txt](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/CMakeLists.txt)
Compilation settings using `CMake` to compile the library as a shared library (`.dylib`) and compile a command-line testing application (`gate_cli`).

#### [NEW] [main.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/main.cpp)
A comprehensive interactive CLI application that enables the user to:
1. Enrol a Student (generates a mock template and writes details).
2. Scan Fingerprint (triggers MacBook Touch ID. On success, prompts which enrolled student's template to match, then runs the full biometric matching engine and writes daily log entry).
3. View Daily Logs (displays the today's gate crossings table).
4. Approve Home Leave (sends a student to `home_active.dat`).
5. Run Curfew check (reports students still outside past 18:30).
6. Perform Batch Promotion.

---

## Verification Plan

### Automated/Build Verification
1. We will compile the project using CMake:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```
2. Confirm the production of `libgate_engine.dylib` and `gate_cli`.

### Manual CLI Verification
We will run `./gate_cli` and perform the following sequence:
1. **Enrollment**: Enroll two students:
   - "Devansh" (Hosteler, Roll: 26CSE001)
   - "Bacchi" (Day Scholar, Roll: 26CSE002)
2. **Scan IN/OUT**:
   - Choose "Scan Fingerprint".
   - The MacBook Touch ID popup should appear. Touch the sensor.
   - If Touch ID succeeds, select "Devansh" (Roll: 26CSE001) to simulate his template.
   - The CLI should show: `Student resolved: Devansh. Hosteler. Status: OUT (Count 1). Purpose: Market`.
   - Perform another scan for "Devansh". Touch sensor. Success. Status updates to `IN (Count 2)`.
   - Do a scan for Day Scholar "Bacchi". Touch sensor. Success. Status updates to `IN (Count 1)`.
3. **Home Leave Workflow**:
   - Perform a scan for "Devansh". Select Purpose: `Home`.
   - Since it's Home, it requires approval. CLI asks: `Approve Home leave for Devansh? (y/n)`. Select `y`.
   - Status updates to `OUT` and student record is written to `Home_data/home_active.dat`.
   - Verify curfew check (only "Bacchi" should be listed as "inside", since "Devansh" is on approved Home leave).
   - Scan "Devansh" again to return. Touch sensor. Success. Verify he is removed from `home_active.dat` and is marked `IN`.
4. **Log Validation**:
   - Select "View Daily Logs". Confirm directory structure is correct (`Everyday_data/2026/06_June/02_06_2026.dat` or current date).
   - Check that formatting is correct.
