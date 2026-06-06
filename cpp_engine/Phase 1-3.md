# Project Walkthrough - Database Engine & macOS Touch ID Integration (Phases 1-3)

We have built and compiled the core database engine (Phases 1-3) as a high-performance C++ shared library and created an interactive console testing application (`gate_cli`) integrated with macOS Touch ID.

---

## Folder and File Structure

All source files are located inside the [cpp_engine](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine) directory:

```
cpp_engine/
|-- include/
|   |-- engine.h               <-- Structs, global constants, and core database engine API declarations
|   |-- serializer.h           <-- Declarations for binary disk serialization/deserialization helper functions
|   |-- indexer.h              <-- Declarations for the in-memory fingerprint cache and FNV-1a hashing functions
|   |-- touch_id.h             <-- Declaration of the Objective-C++ macOS LocalAuthentication wrapper
|
|-- src/
|   |-- master_db.cpp          <-- Implements student profile management (CRUD), global directory generation, and batch updates
|   |-- daily_log.cpp          <-- Implements per-day gate crossing directories (Everyday_data/YYYY/MM_Month/DD_MM_YYYY.dat) and date queries
|   |-- home_db.cpp            <-- Implements operations for the active home-leave temporary database
|   |-- serializer.cpp         <-- Implements sequential binary reading and writing of structs to disk
|   |-- indexer.cpp            <-- Manages the global in-memory fingerprint hash index (`master_index.dat`) to prevent duplicate disk lookups
|   |-- fingerprint.cpp        <-- Implements Level 1 (spatial-hash filter) & Level 2 (full template similarity) matching, and rejection logs
|   |-- touch_id.mm            <-- Objective-C++ module querying macOS LocalAuthentication biometrics for physical scanner interaction
|   |-- main.cpp               <-- A premium-designed console application orchestrating the entire system for interactive verification
|
|-- CMakeLists.txt              <-- Multi-language compiler directives compiling the shared library and target test binary
```

---

## File Explanations

### 1. [engine.h](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/include/engine.h)
Defines fixed-size data structures:
* `StudentRecord`: Main student database record. Note that we updated `phone_number` to `char phone_number[15]` to accommodate international prefixes and leading zeros.
* `LogEntry`: Everyday gate crossing entries, allowing up to 20 gate crossings per day with corresponding timestamps.
* `HomeRecord`: Tracks students approved to leave campus for home.
* `MatchResult`: Standard struct packaging verification results for matching calls.

### 2. [serializer.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/serializer.cpp)
Reads and writes records directly to disk in **raw binary formats** using `file.write` and `file.read`. This is orders of magnitude faster than parsing text or CSV files because entire structs are mapped to memory in a single disk operation.

### 3. [master_db.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/master_db.cpp)
Manages enrolling, updating, and removing students. It structures profiles into folders like `Student_data/2026_batch/` and updates a local batch index (`index.dat`). It also handles batch promotions (incrementing academic year for whole batches) and bulk database deletions.

### 4. [daily_log.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/daily_log.cpp)
Generates the daily files structured dynamically as `Everyday_data/YYYY/MM_Month/DD_MM_YYYY.dat`. It also contains range query algorithms to load all student records across selected date ranges.

### 5. [home_db.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/home_db.cpp)
Maintains `Home_data/home_active.dat` which lists students currently away on approved leave.

### 6. [indexer.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/indexer.cpp)
Initializes an in-memory database index cache (`g_fingerprint_cache`). When the system boots, it maps all student templates to FNV-1a hashes in RAM so lookups don't trigger heavy file system reads.

### 7. [fingerprint.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/fingerprint.cpp)
Implements matching:
* **Level 1 Coarse Filter**: Uses a spatial energy hash (sum of template bytes) to filter candidate students.
* **Level 2 Similarity Match**: Compares the 512-byte template byte-by-byte against candidates. If the match score crosses `0.75` (75% identical bytes), the student is successfully resolved.
* **Rejection Logs**: Logs failed attempts with timestamps and empty/unrecognized biometrics inside `Rejection_log/rejections_DD_MM_YYYY.dat`.

### 8. [touch_id.mm](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/touch_id.mm)
Exposes `macos_touch_id_authenticate` to C++. Uses Grand Central Dispatch semaphores to block execution synchronously while prompting the native macOS Touch ID system dialog to scan a fingerprint.

### 9. [main.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/main.cpp)
Orchestrates options for student enrollment, biometric scanning via macOS Touch ID, daily crossing log displays, gone home databases, curfew anomaly checking, and batch promotion.

---

## How to Compile the Code

1. Open your Mac Terminal.
2. Navigate to the `cpp_engine` directory:
   ```bash
   cd "/Users/devanshkhosla/Projects/Test folder/cpp_engine"
   ```
3. Compile the build folder using CMake:
   ```bash
   mkdir -p build && cd build
   cmake ..
   make
   ```
   *This compiles the shared library `libgate_engine.dylib` and the target executable `gate_cli`.*

---

## How to Test and Verify on your MacBook

Since you are running on your physical MacBook, you are the only one who can interact with the physical Touch ID sensor! Follow these steps to verify:

### 1. Run the Executable
From the build directory, launch the testing app:
```bash
./gate_cli
```

### 2. Enroll Students
Create test profiles:
1. Select **Option 1 (Enroll Student)**.
2. Create **Devansh** (Hosteller):
   * Roll Number: `26CSE001`
   * Program: `MSc`
   * Batch: `2026`
   * Academic Year: `1`
   * Is Hosteller?: `1` (Yes)
   * **Touch the MacBook Touch ID sensor when prompted to authorize and complete the enrollment.**
3. Select **Option 1** again and create **Bacchi** (Day Scholar):
   * Roll Number: `26CSE002`
   * Is Hosteller?: `0` (No)
   * **Touch the Touch ID sensor when prompted to complete the enrollment.**

### 3. Test Biometric Gate Scans & Parity Logic
Now test physical scanner verification:
1. Select **Option 2 (Gate Scan)**.
2. Select **Devansh** (Option 1) to simulate his template.
3. **Touch the Touch ID sensor on your MacBook keyboard when the system prompts you.**
4. Once verified, choose exit purpose: **Market** (Option 1).
   * *Status output should show Devansh marked as `OUT` (Count 1) since Hostellers default to INSIDE.*
5. Perform another scan for **Devansh** and authenticate.
   * *Status updates to `IN` (Count 2) and no purpose is requested.*
6. Perform a scan for Day Scholar **Bacchi** and select purpose **Class** (Option 1).
   * *Status updates to `IN` (Count 1) since Day Scholars default to OUTSIDE.*

### 4. Test Home Leave Approval
1. Scan **Devansh** again. Select purpose **Home** (Option 4).
2. The CLI will simulate the Admin panel asking: `Approve Home leave request? (y/n)`. Type `y` and press enter.
   * *Devansh status updates to `OUT` and is registered inside `Home_data/home_active.dat`.*
3. Select **Option 4** from the main menu (View Gone Home Registry). Verify Devansh is listed on active leave.

### 5. Verify Curfew Anomalies
1. Select **Option 5** (Curfew Compliance Check).
   * *Day Scholar Bacchi will be flagged as OUTSIDE COMPLIANCE because his status is `IN` (still inside campus after curfew).*
   * *Devansh is NOT flagged because, although he is `OUT`, he has an approved Home leave registry!*
2. Now select **Option 2 (Gate Scan)** for **Bacchi**. Scan and verify. His status becomes `OUT` (Count 2).
3. Run **Option 5** (Curfew Compliance Check) again. All students should now be compliant!

### 6. Verify Return from Home
1. Scan **Devansh** again.
   * *The database engine detects him returning from home, removes him from the gone-home registry, logs his arrival timestamp, and marks him `IN`.*
2. Check the Home registry list (**Option 4**) to verify it is now empty.

### 7. Inspect Student Master Database & Fingerprint Hashes
1. Select **Option 7** (View Student Master Database) from the main menu.
   * *The console will print a structured table of all registered students in the system (Roll No, Name, Program, Batch, Year, Type, Phone).*
   * *The table includes a column **Fingerprint Hash (FNV-1a)** showing the exact 32-bit hex hash (e.g. `0x57d62dc5`) computed from each student's binary fingerprint template on disk. This verifies that the biometric hashing indexer is fully operational!*

