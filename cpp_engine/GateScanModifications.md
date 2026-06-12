#### dated 12th june 2026
# Gate Scan Modification Log - Phase 1-3 Updates

This document covers all modifications made to the Campus Biometric Gate System during the Phase 1-3 development period. These changes address a critical fingerprint matching bug, introduce multiple-match detection, add separate fingerprint storage files, and provide a development-only database wipe utility.

---

## Problem:

When executing a gate scan (Option 2), the system consistently rejected all fingerprint matches with the following output:

```

[Matcher] No match found. Best score: 0.00390625 (Threshold: 0.75)

[ERROR] Fingerprint match rejected by database engine!

Student is not enrolled

Match not found

```
This occurred even when all enrolled students were confirmed to be in the database and linked to valid fingerprint templates.

---

## Root Cause Analysis

The bug was located in the `do_scan()` function inside `main2.0.cpp`.
### The Original Flow (main.cpp - Working)

In the original [main.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/main.cpp), the gate scan followed this sequence:

1. List all enrolled students on screen.

2. User selects which student is scanning (by index number).

3. `generate_mock_template(selected_student.roll_number, live_scan)` fills the `live_scan` buffer with a deterministic 512-byte template derived from the selected student's roll number.

4. `fingerprint_match(live_scan, 512)` compares this buffer against all stored templates in the database.

5. Match is found because the generated template is identical to the one stored during enrollment.
### The Broken Flow (main2.0.cpp - Before Fix)

In [main2.0.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/main2.0.cpp), the student selection step was removed (the intent being that a real scanner would identify the student automatically). However, the `live_scan` buffer was never populated:
```cpp

uint8_t live_scan[512]; // Declared but NEVER initialized

// ...

MatchResult match = fingerprint_match(live_scan, 512); // Compares uninitialized garbage

```
The `live_scan` array contained random stack memory, which when compared byte-by-byte against stored templates, produced a match score of approximately 0.004 (0.4%) -- far below the 0.75 (75%) threshold required for a positive match.
### Additional Logic Bug

The loop structure was also flawed:
```cpp

MatchResult match = fingerprint_match(live_scan, 512); // Computed ONCE

for (size_t i = 0; i < students.size(); ++i) {

if (match.matched) { // Same value every iteration

const auto &selected_student = students[i]; // Wrong student on i > 0

```
The `fingerprint_match()` result was computed once outside the loop, but the loop iterated over all students checking `match.matched` which never changed between iterations. If it ever matched (it did not, due to the uninitialized buffer), it would assign the wrong student record on iterations beyond the first.

---

## Modifications Made

### 1. Fixed Gate Scan Logic in main2.0.cpp

**File**: [main2.0.cpp - do_scan()](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/main2.0.cpp#L159)

The gate scan now follows this corrected flow:

1. Check that students exist in the database.

2. Prompt for macOS Touch ID authentication (pass/fail gate).

3. On success, enter DEV MODE: ask for a roll number to simulate the fingerprint scan.

4. Call `generate_mock_template(sim_roll, live_scan)` to fill the scan buffer with the deterministic template for that roll number.

5. Call `fingerprint_match(live_scan, 512)` which searches ALL enrolled students for a match.

6. If matched, display the identified student. If multiple students match, display a warning.

7. If no match, reject and log to the rejection file.

The DEV MODE prompt exists because macOS Touch ID does not provide raw fingerprint template data. It only returns a boolean pass/fail result. When real fingerprint scanner hardware is integrated in future phases, this block will be replaced by actual sensor data capture. The mock template system gives each student a unique 512-byte fingerprint derived from their roll number, so different roll numbers always produce different templates.

The broken for-loop was replaced with a direct conditional check:
```cpp

MatchResult match = fingerprint_match(live_scan, 512);

if (!match.matched) {

// Reject

}

```
---

### 2. Multiple-Match Detection in fingerprint.cpp

**File**: [fingerprint.cpp - fingerprint_match()](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/fingerprint.cpp#L45)

The matcher now counts every student whose template scores at or above the 0.75 threshold, not just the best one. This is stored in the new `match_count` field on `MatchResult`.
When the matcher runs:

- It iterates through all candidates in the fingerprint cache.

- For each candidate scoring above the threshold, it logs the student name, roll number, and score.

- It tracks the highest-scoring match as the primary result.

- After scanning all candidates, it sets `result.match_count` to the total number of above-threshold matches.

- If `match_count > 1`, the matcher prints a warning indicating that multiple students share the same fingerprint.

The calling code in `do_scan()` also checks `match.match_count` and displays a yellow warning banner when duplicates are detected, then proceeds with the best (highest confidence) match.

**File**: [engine.h - MatchResult](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/include/engine.h#L47)

Added `int match_count` field to the `MatchResult` struct to carry the duplicate count from the matcher to the caller.

---

### 3. Separate Fingerprint Template Files (.fpt)

**Files**: [serializer.h](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/include/serializer.h), [serializer.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/serializer.cpp)

Two new functions were added for reading and writing raw fingerprint data to standalone binary files:
- `serialize_fingerprint(filepath, template_data, length)` -- writes the raw 512-byte fingerprint template to a `.fpt` file.

- `deserialize_fingerprint(filepath, template_data, max_length)` -- reads the raw 512-byte fingerprint template from a `.fpt` file.

**File**: [master_db.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/master_db.cpp)

The following operations now handle `.fpt` files alongside `.dat` files:

- `student_add()`: After writing the student `.dat` record, also writes `<roll_number>.fpt` in the same batch directory. A helper function `get_fingerprint_file_path()` was added to generate the file path.

- `student_update()`: After overwriting the `.dat` file, also rewrites the `.fpt` file with the updated template.

- `student_remove()`: After deleting the `.dat` file, also deletes the corresponding `.fpt` file.

After enrollment, the batch directory will contain:

```

db_root/Student_data/2026_batch/

26CSE001.dat <-- Full student record (binary struct)

26CSE001.fpt <-- Raw 512-byte fingerprint template only

26CSE002.dat

26CSE002.fpt

index.dat <-- Batch index

```

The `.fpt` files make it straightforward to inspect, compare, or hex-dump fingerprint data without parsing the full student record struct.

  
---

### 4. Database Wipe Utility (Option 9 - DEV ONLY)

**File**: [main2.0.cpp - do_nuke_database()](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/main2.0.cpp#L553)

A new menu option (9) was added for development use only. It permanently deletes all data in the system for a fresh start. The function requires double confirmation to prevent accidental data loss:

1. First prompt: type `DELETE` to confirm.

2. Second prompt: type `YES` to proceed.

If either confirmation fails, the operation is cancelled and no data is modified.

When confirmed, it calls `engine_wipe_all_data()` which:

- Deletes the `Student_data/` directory (all enrolled students, batch indexes, `.dat` and `.fpt` files).

- Deletes the `Everyday_data/` directory (all daily gate crossing logs).

- Deletes the `Home_data/` directory (all home leave records).

- Deletes the `Rejection_log/` directory (all rejection logs).

- Clears the in-memory `g_fingerprint_cache` vector.

- Re-creates all four directories as empty folders so the engine is immediately ready for fresh enrollments.

**File**: [engine.h](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/include/engine.h) -- Added `engine_wipe_all_data()` declaration.

**File**: [master_db.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/master_db.cpp#L349) -- Added `engine_wipe_all_data()` implementation.

The menu option is displayed in red text and labelled `[DEV ONLY]` to make it visually distinct from production options.

---
### 5. Gate Count and Exit Bug Fixes

**File**: [main2.0.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/main2.0.cpp)

**Dead code removal**: Three instances of `(entry.gate_count++) % 2` were found across the file. The modulo operation result was never assigned or used, making the `% 2` portion dead code. The increment (`++`) still worked, but the expression was misleading. These were cleaned up:

- First scan: changed to `entry.gate_count = 1` (explicit initialization).

- Subsequent scans: changed to `log_entry.gate_count++` (clean increment without dead modulo).
**Switch-case exit bug**: The `case 8` (Shutdown and Exit) used `break` which only exits the `switch` statement, not the enclosing `while(true)` loop. This meant the program would never actually exit when selecting Option 8. Fixed by replacing `break` with `return 0` to properly terminate the program.

**Input validation range**: Updated the invalid input error message from "Please choose 1-8" to "Please choose 1-9" to account for the new Option 9.

---
### 6. Build Configuration Update

**File**: [CMakeLists.txt](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/CMakeLists.txt)

Changed the CLI executable source from `src/main.cpp` to `src/main2.0.cpp`:
# CLI application for testing

add_executable(gate_cli src/main2.0.cpp)

target_link_libraries(gate_cli PRIVATE gate_engine)

The original `main.cpp` is preserved in the source directory as a reference but is no longer compiled into the executable.

---

### 7. Security Note -- Fingerprint Template Encryption (Future Phase)

**Current state**: All fingerprint templates are stored in plaintext on disk, both inside the `.dat` student record files and the separate `.fpt` files. Anyone with file system access can read the raw biometric data.

**Why hashing does not work here**: Hashing (FNV-1a, SHA-256, etc.) is a one-way operation. Once hashed, the original template data is destroyed. The matching engine needs the original 512-byte template to perform byte-by-byte similarity comparison and compute a confidence score. If both the stored and scanned templates were hashed, even a single byte difference would produce completely different hashes, making fuzzy/partial matching impossible. Real fingerprint scans are never perfectly identical to the enrolled scan.

**What should be done instead**: The templates should be encrypted at rest using AES-256 (symmetric encryption). The flow would be:

1. Enrollment: Encrypt the 512-byte template with AES-256 before writing to disk (.dat and .fpt files).
2. Gate scan: Read the encrypted template from disk, decrypt it into memory, then run `compare_templates()`.
3. After matching: Wipe the decrypted template from memory immediately.
4. Key management: The encryption key should be stored securely (e.g. macOS Keychain, environment variable, or a hardware security module).

**The FNV-1a hash in master_index.dat is not a security measure** -- it is a non-cryptographic 4-byte hash used purely as a fast lookup index to avoid loading every student file from disk during matching. It functions like a table of contents, not a lock.

**Files that would need modification**:
- `serializer.cpp` -- encrypt before writing, decrypt after reading
- `fingerprint.cpp` -- decrypt candidates in memory before comparison
- `engine.h` -- add key initialization and management API
- A new `crypto.cpp` / `crypto.h` module for AES-256 operations

This is tracked as a future phase item and should be implemented before any production deployment.

---
