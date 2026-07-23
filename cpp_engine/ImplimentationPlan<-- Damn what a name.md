# Implementation Plan -- Campus Biometric Entry Management System

This is the full build plan for the project described in [Final_Draft.md](file:///Users/devanshkhosla/Projects/untitled%20folder/Docs/Final_Draft.md). It will be covering **what the C++ engine will do**, **what Python does**, how will will connect them, and the order in which everything gets built.

---

## Architecture (What does it Look like...)

```
                HARDWARE <-- Still needs to be viewed at
                   |
          fingerprint bytes <-- this converts the Fingerprint input into bytes
                   |             Beacause its  easier to handle in C++
                   |
                   v
    +---------------------------------+
    |      C++ DATABASE ENGINE      <-- compiled as a shared library (.so / .dll)
    |                                 |
    |  - Binary file storage          |
    |  - Fingerprint matching         |
    |  - Hashing / indexing           |
    |  - CRUD on all 3 databases      |<--Hopefully All of you know what CRUD is.
    |  - Returns structured results   |
    +---------------------------------+
                   ^
                   |  (pybind11 / ctypes)  <-- This is a foreign function library
                   |
    +---------------------------------+
    |      PYTHON APPLICATION         |
    |                                 |
    |  - Parity logic (IN/OUT)        |
    |  - Purpose selection            |
    |  - HOME approval workflow       |
    |  - Curfew monitoring            |
    |  - Session management           |
    |  - Admin Control Panel (GUI)    | <-- Not sure about this one.
    |  - Excel export / archival      |
    +---------------------------------+
```

#ForeingFunctionlibrary 
- `ctypes` - is a built-in Python foreign function library. It allows your Python script to directly load compiled C libraries (like .dll, .so, or .dylib files) and call functions within them.
- This is best used for us to Call simple, legacy C APIs or hardware drivers from Python
- `ybind11`- is a modern, lightweight C++ library that lets you seamlessly expose C++ types and classes to Python (and vice versa). It generates Python "bindings" so that our C++ code behaves exactly like a native Python module.
---

## Part 1 -- The C++ Database Engine (What C++ Will Do)

The C++ engine is the **only** component that touches the disk. It owns all file creation, reading, writing, and deletion. It also owns fingerprint template matching because that is a compute-heavy, latency-sensitive operation. Everything the engine does falls into five categories.

---

### 1.1 On-Disk Folder Structure

The engine creates and manages the following folder hierarchy:

```
project_root/
|
|-- Student_data/                         <-- MASTER DATABASE
|   |-- 2024_batch/
|   |   |-- index.dat                     <-- batch index (list of roll numbers in this batch)
|   |   |-- 24CSE001.dat                  <-- one binary file per student
|   |   |-- 24CSE002.dat
|   |   +-- ...
|   |-- 2025_batch/
|   |   |-- index.dat
|   |   +-- ...
|   +-- master_index.dat                  <-- global index mapping fingerprint hash -> roll number -> file path
|
|-- Everyday_data/                        <-- DAILY LOG DATABASE
|   |-- 2026/
|   |   |-- 01_January/
|   |   |   |-- 01_01_2026.dat
|   |   |   |-- 02_01_2026.dat
|   |   |   +-- ...
|   |   |-- 02_February/
|   |   +-- ...
|   +-- ...
|
|-- Home_data/                            <-- HOME DATABASE
|   |-- home_active.dat                   <-- currently-away students (temporary, entries come and go)
|
|-- Rejection_log/                        <-- UNRECOGNISED SCANS
|   |-- rejections_01_01_2026.dat
|   +-- ...
|
+-- Archive/                              <-- EXPORTED EXCEL FILES
    |-- 2026/
    +-- ...
```

**Why .dat (binary) instead of .csv?**
Binary files are smaller and faster to read/write. The C++ engine serialises structs directly to bytes. CSV is human-readable but slow -- the engine will only produce CSV/Excel when Python explicitly asks for an export.

---

### 1.2 Data Structures (C++ Structs)

These are the core structs that the engine stores and operates on.

#### Student Record (Master Database) Our First Major Function

```cpp
#include <cstdint>
#include <string>
#include <vector>

struct StudentRecord {
    char roll_number[20];               // fixed-size for fast binary I/O
    char name[100];
    char program[20];                   // "BSc", "MSc", "PhD", etc.
    char batch[10];                     // "2025", "2024", etc.
    int  year;                          // current academic year (1, 2, 3...)
    int  phone_number;
    bool is_hosteller;                  // true = hosteler, false = day scholar
    uint8_t fingerprint_template[512];  // raw biometric template (typical size: 256-512 bytes)
};
```

> Imp-Note: The fingerprint template is stored as a fixed-size byte array, not a single uint8_t. Real fingerprint sensors (like the R307, GT-521F, or ZFM-20 these are the only i got to review till now.) produce templates that are typically 256 to 512 bytes long. The exact size will depend on the sensor model chosen.

#### Daily Log Entry

```cpp
struct LogEntry {
    char roll_number[20];
    char name[100];
    int  year;
    char reason[50];                    // "Market", "Exam", "Medical", "Home", "Class", or custom
    int  gate_count;                    // incremented on each scan
    char status[4];                     // "IN" or "OUT"
    bool late_return;                   // true if scanned after 18:30
    char timestamps[20][25];            // up to 20 scans per day, each as "YYYY-MM-DD HH:MM:SS"
    int  timestamp_count;               // how many timestamps have been recorded
};
```

#### Home Record

```cpp
struct HomeRecord {
    char roll_number[20];
    char name[100];
    int  year;
    int  contact_number;
    char date_of_leaving[12];           // "DD-MM-YYYY"
    char time_of_leaving[10];           // "HH:MM:SS"
};
```

#### Match Result (returned to Python)

```cpp
struct MatchResult {
    bool   matched;                     // true if a match was found
    char   roll_number[20];
    char   name[100];
    char   program[20];
    char   batch[10];
    int    year;
    int    phone_number;
    bool   is_hosteller;
    float  confidence_score;            // 0.0 to 1.0, how confident the match is
};
```

---

### 1.3 The C++ Engine API -- Every Function It Exposes For Python to see 

This is the complete list of functions the C++ engine will expose. Python calls these through the bridge. This is Upto the research i have done so far, few Things might get added here.

#### Master Database Operations

```
student_add(StudentRecord record) -> bool
```
Takes a fully populated StudentRecord, writes it as a binary .dat file inside the correct batch folder, and updates both the batch index and the master fingerprint index. Returns true on success.

```
student_remove(const char* roll_number) -> bool
```
Finds the student file by roll number, deletes it, and removes the entry from all indices. Returns true on success.

```
student_update(const char* roll_number, StudentRecord updated_record) -> bool
```
Overwrites the existing student file with the new data and refreshes the indices. Returns true on success.

```
student_get(const char* roll_number) -> StudentRecord
```
Reads and returns the full student record from disk by roll number.

```
student_list_by_batch(const char* batch) -> vector<StudentRecord>
```
Returns all students in a given batch by reading the batch index and loading each record.

```
student_list_all() -> vector<StudentRecord>
```
Returns every student in the master database. Used by the control panel's "View Students" feature.

```
batch_promote(const char* batch) -> int
```
Increments the `year` field by 1 for every student in the specified batch. Returns the number of students updated.

```
batch_promote_all() -> int
```
Increments `year` for every student across all batches.

```
batch_delete(const char* batch) -> bool
```
Deletes the entire batch folder and all student files inside it. Removes all entries from the master index.

---

#### Fingerprint Matching

```
fingerprint_match(const uint8_t* live_scan, int scan_length) -> MatchResult
```
This is the most performance-critical function. It:
1. Loads the master fingerprint index from memory (or disk, on first call).
2. Compares the incoming `live_scan` bytes against every stored template.
3. Uses the hashing/indexing strategy (discussed in section 1.4) to narrow the search space.
4. Returns a MatchResult with `matched = true` and the full student details if the best match exceeds the confidence threshold.
5. Returns `matched = false` if no template scores above the threshold.

```
fingerprint_enroll(const char* roll_number, const uint8_t* template_data, int length) -> bool
```
Writes the fingerprint template to the student's record file and adds it to the fingerprint index. This is called during student enrollment (the "Add" operation in the control panel).

---

#### Daily Log Operations

```
log_create_day(const char* date_string) -> bool
```
Creates a new daily log file (e.g., `01_01_2026.dat`) inside the correct year/month folder. If the folders do not exist, it creates them. Returns false if the file already exists.

```
log_day_exists(const char* date_string) -> bool
```
Checks whether a log file for the given date already exists.

```
log_add_entry(const char* date_string, LogEntry entry) -> bool
```
Appends a new student entry to the daily log file. Called when a student scans for the first time that day.

```
log_update_entry(const char* date_string, const char* roll_number, LogEntry updated_entry) -> bool
```
Overwrites an existing entry in the daily log. Called every time a student scans again (to update gate_count, timestamps, status, etc.).

```
log_get_entry(const char* date_string, const char* roll_number) -> LogEntry
```
Returns the log entry for a specific student on a specific date. If the student has not scanned that day, returns an entry with gate_count = 0.

```
log_get_all_entries(const char* date_string) -> vector<LogEntry>
```
Returns every entry in the daily log. Used by the control panel's Log tab and by the curfew check.

```
log_get_entries_in_range(const char* start_date, const char* end_date) -> vector<LogEntry>
```
Returns all log entries across a range of dates. Used by the "Select Period of Log" feature.

```
log_delete_day(const char* date_string) -> bool
```
Deletes a daily log file. Used after "Export and Delete".

---

#### Home Database Operations

```
home_add(HomeRecord record) -> bool
```
Adds a student to the active home-away list. Called when an admin approves a HOME request.

```
home_remove(const char* roll_number) -> bool
```
Removes a student from the home-away list. Called when the student returns and scans their fingerprint.

```
home_exists(const char* roll_number) -> bool
```
Checks if a student is currently in the home-away list. Called on every scan to check if this is a home-return scenario.

```
home_get_all() -> vector<HomeRecord>
```
Returns all students currently away on approved home leave. Used by the control panel's Home tab.

---

#### Rejection Log

```
rejection_log_write(const char* date_string, const uint8_t* failed_scan, int scan_length) -> bool
```
Writes the unrecognised fingerprint data and a timestamp to the rejection log for that day. The admin can review these later.

---

#### Utility

```
engine_init(const char* project_root_path) -> bool
```
Initialises the engine: sets the root path, creates the folder hierarchy if it does not exist, and loads the master fingerprint index into memory. Must be called once when the Python application starts.

```
engine_shutdown() -> void
```
Flushes any buffered writes and releases memory. Called when the Python application exits.

---

### 1.4 Hashing and Indexing Strategy

Doing a byte-by-byte comparison of a live fingerprint scan against every single stored template is slow when there are many students. The engine uses a two-level approach:

**Level 1 -- Coarse Hash Filter:**
When a student is enrolled, the engine computes a short hash (e.g., 32-bit or 64-bit) of their fingerprint template. This hash is stored in the `master_index.dat` file alongside the roll number and file path. When a live scan comes in, the engine first computes the hash of the live scan and only does full byte-level comparison against templates whose hash is "close" to the live hash (within a configurable Hamming distance). This eliminates the majority of candidates.

**Level 2 -- Full Template Comparison:**
For the small set of candidates that pass the coarse filter, the engine does a full comparison. The comparison algorithm depends on the fingerprint sensor's SDK -- most sensors provide a matching function that returns a score between 0 and 100. If we ditch the SDK approch or its not available for our case, we go for a more basic approach that is to compute the normalized cross-correlation or Euclidean distance between the two byte arrays. DONT WORRY THIS IS THEORY WE WILL JUST DO THE CODING AND NOT GET INTO THIS STUFF But it is good to know.

**In-Memory Cache:**
On `engine_init()`, the engine loads all fingerprint hashes (and optionally all full templates if memory permits) into RAM. This means the matching operation never needs to hit the disk -- it works entirely in memory, falling back to disk only for edge cases.

---

### 1.5 Binary Serialisation

Every struct is written to disk using direct binary serialisation:

```cpp
// Writing a StudentRecord to disk
void write_student(const std::string& filepath, const StudentRecord& student) {
    std::ofstream file(filepath, std::ios::binary);
    file.write(reinterpret_cast<const char*>(&student), sizeof(StudentRecord));
    file.close();
}

// Reading a StudentRecord from disk
StudentRecord read_student(const std::string& filepath) {
    StudentRecord student;
    std::ifstream file(filepath, std::ios::binary);
    file.read(reinterpret_cast<char*>(&student), sizeof(StudentRecord));
    file.close();
    return student;
}
```

This is orders of magnitude faster than parsing CSV line by line. A single `read()` call loads the entire struct into memory in one disk operation.

---

### 1.6 C++ Source File Organisation

```
cpp_engine/
|-- include/
|   |-- engine.h               <-- all struct definitions and function declarations
|   |-- master_db.h             <-- master database operations
|   |-- daily_log.h             <-- daily log operations
|   |-- home_db.h               <-- home database operations
|   |-- fingerprint.h           <-- matching and enrollment
|   |-- indexer.h               <-- hashing, indexing, in-memory cache
|   +-- serializer.h            <-- binary read/write utilities
|
|-- src/
|   |-- engine.cpp              <-- init, shutdown, top-level orchestration
|   |-- master_db.cpp
|   |-- daily_log.cpp
|   |-- home_db.cpp
|   |-- fingerprint.cpp
|   |-- indexer.cpp
|   +-- serializer.cpp
|
|-- bindings/
|   +-- pybind_module.cpp       <-- pybind11 wrapper that exposes all functions to Python
|
+-- CMakeLists.txt              <-- build configuration (compiles to shared library)
```

---

## Part 2 -- The Python Application (What Python Does)

Python is everything the user sees and every piece of logic that is not bottlenecked by speed. It calls the C++ engine for all data operations and handles the rest.

---

### 2.1 Python's Role -- In a nutshell

| Responsibility                       | Why Python (not C++)                                                         |
| ------------------------------------ | ---------------------------------------------------------------------------- |
| Parity logic (IN/OUT calculation)    | Simple arithmetic, changes frequently, no speed concern                      |
| Purpose selection UI at the terminal | UI code, needs rapid iteration                                               |
| HOME approval workflow               | Business logic with admin interaction, not speed-critical                    |
| Curfew monitoring                    | Queries C++ for data, then filters in Python                                 |
| Session management (start/end day)   | Orchestration logic, calls C++ for file creation                             |
| Admin Control Panel (5-tab GUI)      | GUI code, best done in Python with tkinter/PyQt                              |
| Excel export                         | openpyxl/xlsxwriter are mature Python libraries                              |
| Search and filter in the UI          | Operates on data already fetched from C++ into memory                        |
| Batch promotion trigger              | One button press, calls C++ engine function                                  |
| CRUD operations from the admin panel | Form handling will be in Python however the actual read/write is done by C++ |

---

### 2.2 Python-to-C++ Bridge

The best approach for us as of now is the **pybind11**. It compiles the C++ engine into a Python module (e.g., `gate_engine.so`) that Python imports like any other package:

```python
import gate_engine

# Initialise the C++ engine
gate_engine.engine_init("/path/to/project_root")

# Match a fingerprint (live_scan is a bytes object from the scanner)
result = gate_engine.fingerprint_match(live_scan_bytes)

if result.matched:
    print(f"Welcome, {result.name}")
    print(f"Roll: {result.roll_number}")
    print(f"Hosteller: {result.is_hosteller}")
else:
    print("Fingerprint not recognised")
```

The pybind11 bindings file (`pybind_module.cpp`) wraps every C++ function and converts C++ structs to Python objects (dictionaries or named tuples).

---

### 2.3 Python Module Breakdown

```
python_app/
|
|-- main.py                     <-- entry point, initialises engine, launches GUI
|
|-- bridge/
|   |-- __init__.py
|   +-- engine_wrapper.py       <-- clean Python functions that call gate_engine C++ module
|
|-- logic/
|   |-- __init__.py
|   |-- parity.py               <-- IN/OUT status computation from gate_count + is_hosteller
|   |-- purpose.py              <-- purpose selection handling
|   |-- home_workflow.py         <-- HOME request, approval, rejection, return logic
|   |-- curfew.py               <-- curfew check: who is still outside / still inside
|   +-- session.py              <-- session start, resume, end-of-day
|
|-- ui/
|   |-- __init__.py
|   |-- app.py                  <-- main window, tab navigation
|   |-- log_panel.py            <-- Log tab: table, search, filter, date range, export
|   |-- out_panel.py            <-- Out tab: currently-outside list, reason filter
|   |-- requests_panel.py       <-- Requests tab: pending HOME approvals, approve/reject buttons
|   |-- home_panel.py           <-- Home tab: students on approved leave, program filter
|   |-- system_control.py       <-- Systems Control tab: password gate, add/remove/edit/view
|   +-- terminal_display.py     <-- gate terminal screen: shows scan result, purpose selection
|
|-- export/
|   |-- __init__.py
|   +-- excel_exporter.py       <-- openpyxl/xlsxwriter code to generate .xlsx files
|
+-- config.py                   <-- paths, curfew time, confidence threshold, etc.
```

---

### 2.4 Key Python Logic -- How Each Workflow Uses C++

#### Scan Processing (the main loop at the gate)

```
1. Scanner hardware sends raw fingerprint bytes to Python
2. Python calls:  engine_wrapper.match_fingerprint(raw_bytes)
   --> This calls C++: fingerprint_match(raw_bytes)
   --> C++ returns: MatchResult (name, roll, is_hosteller, etc.)
3. If no match:
   - Python calls: engine_wrapper.log_rejection(date, raw_bytes)
   - Display "Not recognised" on terminal
   - Alert admin
4. If match found:
   - Python calls: engine_wrapper.check_home(roll_number)
     --> C++: home_exists(roll_number)
     If student is in home database (returning from home):
       - Python calls: engine_wrapper.remove_from_home(roll_number)
       - Python calls: engine_wrapper.add_log_entry(date, entry_with_return_time_only)
       - Done
   - Python calls: engine_wrapper.get_log_entry(date, roll_number)
     --> C++: log_get_entry(date, roll_number)
     If gate_count == 0 (first scan today):
       - Python calls: engine_wrapper.create_log_entry(date, new_entry)
     Else:
       - Python increments gate_count
   - Python runs parity.compute_status(gate_count, is_hosteller) --> "IN" or "OUT"
   - Python shows purpose selection on terminal screen
   - Student selects purpose
   - If purpose == "HOME":
       - Python adds request to the Requests panel queue
       - Waits for admin approval
       - If approved:
           - Python calls: engine_wrapper.add_to_home(home_record)
           - Python calls: engine_wrapper.update_log_entry(date, roll, updated_entry)
       - If rejected:
           - Python decrements gate_count, discards event
   - Else:
       - Python calls: engine_wrapper.update_log_entry(date, roll, updated_entry)
   - Terminal displays: name, status, timestamp
```

#### Curfew Check

```
1. Python calls: engine_wrapper.get_all_log_entries(today_date)
   --> C++: log_get_all_entries(date)
2. Python calls: engine_wrapper.get_all_home()
   --> C++: home_get_all()
3. Python filters:
   - Hostelers with odd gate_count (they are OUTSIDE)
   - Day scholars with odd gate_count (they are INSIDE past curfew)
   - Exclude anyone in the home list
4. Python fetches contact info for flagged students:
   engine_wrapper.get_student(roll_number) for each
5. Python displays the list on the Out panel
```

#### Excel Export

```
1. Python calls: engine_wrapper.get_all_log_entries(date)
   --> C++: log_get_all_entries(date) returns all LogEntry structs
2. Python converts them to rows using openpyxl/xlsxwriter
3. Python saves the .xlsx file to the Archive folder
4. If "Export and Delete":
   Python calls: engine_wrapper.delete_log_day(date)
   --> C++: log_delete_day(date)
```

---

## Part 3 -- Build Order (Phased Implementation)

### Phase 1: C++ Core -- Structs and File I/O

What to build:
- All struct definitions in `engine.h`
- `serializer.cpp`: binary read/write functions for each struct
- `engine.cpp`: `engine_init()` and `engine_shutdown()` -- creates folder hierarchy
- `master_db.cpp`: `student_add`, `student_remove`, `student_get`, `student_list_by_batch`

How to verify:
- Write a small C++ test program that adds 10 students, reads them back, deletes one, and confirms the file system is correct. i believe this has been done by laxmi already for his previous Project....

---

### Phase 2: C++ Core -- Daily Log and Home Database

What to build:
- `daily_log.cpp`: `log_create_day`, `log_add_entry`, `log_update_entry`, `log_get_entry`, `log_get_all_entries`, `log_get_entries_in_range`, `log_delete_day`
- `home_db.cpp`: `home_add`, `home_remove`, `home_exists`, `home_get_all`

How to verify:
- Test program that creates a day's log, adds entries, updates gate counts, reads them back.
- Test program that adds/removes home records and verifies existence checks.

---

### Phase 3: C++ Core -- Fingerprint Matching and Indexing

What to build:
- `indexer.cpp`: hash computation, master index building, in-memory cache loading
- `fingerprint.cpp`: `fingerprint_match`, `fingerprint_enroll`
- `master_db.cpp` additions: `batch_promote`, `batch_promote_all`, `batch_delete`

How to verify:
- Enroll 100 dummy fingerprint templates (random byte arrays).
- Run `fingerprint_match` with one of the known templates and confirm it matches correctly.
- Run with an unknown template and confirm it returns `matched = false`.

---

### Phase 4: Python-to-C++ Bridge

What to build:
- `pybind_module.cpp`: pybind11 bindings for every function and struct
- `CMakeLists.txt`: build configuration that produces `gate_engine.so` / `gate_engine.pyd`
- `engine_wrapper.py`: Python wrapper functions with clean type hints and error handling

How to verify:
- From a Python script, import `gate_engine`, call `engine_init`, add a student, read it back, and print the result.

---

### Phase 5: Python -- Core Logic

What to build:
- `parity.py`: `compute_status(gate_count, is_hosteller) -> str`
- `purpose.py`: purpose selection logic
- `home_workflow.py`: request queue, approval/rejection handlers
- `curfew.py`: curfew check logic
- `session.py`: session start/resume/end

How to verify:
- Unit tests for parity logic (hosteler with count 1 returns "OUT", day scholar with count 1 returns "IN", etc.)
- Simulated scan processing loop without GUI -- print results to terminal.

---

### Phase 6: Python -- Control Panel GUI

What to build:
- `app.py`: main window with 5 tabs
- `log_panel.py`: table display, search, filter, date range selector, export button
- `out_panel.py`: currently-outside list with reason filter
- `requests_panel.py`: pending HOME requests with approve/reject
- `home_panel.py`: away-on-leave list with program filter
- `system_control.py`: password gate, add/remove/edit student, view databases
- `terminal_display.py`: the gate-side screen showing scan results and purpose selection

How to verify:
- Launch the GUI, manually add a student through Systems Control, simulate a scan, see it appear in the Log panel.

---

### Phase 7: Python -- Excel Export and Archival

What to build:
- `excel_exporter.py`: converts LogEntry data into formatted Excel workbooks
- Export and "Export and Delete" workflows

How to verify:
- Export a day's log, open the resulting .xlsx file, confirm all columns and data are correct.

---

### Phase 8: Integration and End-to-End Testing

What to verify:
- Full pipeline: enroll student via GUI --> scan fingerprint --> see entry in Log panel --> select HOME --> approve in Requests panel --> see student in Home panel --> student returns and scans --> removed from Home panel --> curfew check shows correct lists --> export to Excel --> delete log.
- Performance: match latency with 500+ enrolled students should be under 500 milliseconds.
- Edge cases: unrecognised scan, admin rejects HOME, student scans multiple times in a day, late return after 18:30, batch promotion.

---
