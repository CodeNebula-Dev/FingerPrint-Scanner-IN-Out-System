# Campus Biometric Entry Management System -- Final Draft

---

## Table of Contents

1. Problem Statement
2. Proposed Solution
3. System Overview
4. Core Logic -- The State Transition Model
5. Purpose Selection and the HOME Workflow
6. Curfew Monitoring
7. Database Architecture
8. Data Structures
9. Daily Log Lifecycle and Archival
10. Control Panel and User Interface
11. Implementation Path
12. Known Flaws and Limitations
13. Future Additions


---

## 1. Problem Statement

College campuses that rely on manual logbooks at the gate face a set of recurring problems:

- Long queues form during peak hours (class changes, exam times, curfew), wasting time for both students and gate staff.
- Handwriting is often illegible, making records unreliable for audits or disciplinary reviews.
- There is no real-time visibility into who is currently inside or outside the campus.
- Manual tracking of hostelers versus day scholars is error-prone; the two groups have opposite default positions (hostelers start inside, day scholars start outside), and logbooks do not account for this.
- Leave approvals for students going home are handled on paper, with no guarantee the gate staff and hostel warden are synchronised.
- End-of-day accountability -- identifying which hostelers have not returned and which day scholars have not left -- depends entirely on human cross-referencing.

The system we propose eliminates every one of these bottlenecks by replacing the logbook with a biometric fingerprint scanner, a structured database, and an administrative control panel.

---

## 2. Proposed Solution

A fingerprint-based gate management system that:

- Identifies students instantly via biometric scan instead of manual name entry.
- Automatically determines whether a student is going IN or OUT based on their scan count and residency type, with no manual toggle required.
- Enforces an approval workflow for students requesting to go HOME.
- Generates a real-time list of students currently outside the campus at curfew time.
- Archives daily logs into Excel files for long-term record-keeping and audit compliance.
- Provides a full-featured admin control panel for managing student records, viewing logs, handling requests, and editing data.

---

## 3. System Overview

The system is composed of four major layers:

**Hardware Layer** -- A fingerprint scanner connected to the gate terminal. The scanner captures a live fingerprint and passes it to the software for matching.

**Database Engine Layer (C++)** -- A custom-built database engine written in C++ from scratch. This is the performance-critical core of the system. It handles all raw data storage, retrieval, indexing, and file I/O for the three databases (Master, Daily Log, and Home). C++ is chosen here because gate scans demand near-instant lookup latency -- the biometric template matching and record fetching must happen in milliseconds, and C++ gives direct control over memory layout, binary file operations, and hashing algorithms without interpreter overhead.

**Application Layer (Python)** -- Everything above the database engine is written in Python. This includes the state transition status logic, purpose selection flow, HOME approval workflow, curfew monitoring, session management, Excel export, and all control panel UI. Python is chosen for this layer because it offers rapid development, cleaner code for business logic, rich library support (for Excel generation, UI frameworks, and future web integration), and easier maintenance. Python communicates with the C++ database engine through bindings (such as ctypes, pybind11, or a subprocess-based interface), sending queries and receiving structured results.

**Data Layer** -- Three distinct data stores, all managed by the C++ engine:
- A Master Database (Student_data) holding all enrolled student records.
- A Daily Log Database (Everyday_data) that creates a new file each day to record gate crossings.
- A Home Database (student_gone_home) that temporarily holds records of students approved to go home.

**Interface Layer** -- An administrative control panel with five tabs: Log, Out, Systems Control, Requests, and Home. Built in Python, this is where gate staff and administrators interact with the system.

---

## 4. Technology Stack

The project uses a dual-language architecture, splitting responsibilities based on where raw speed matters versus where development flexibility matters.

### C++ -- The Database Engine

The database engine is the only component written in C++. It is responsible for:

- Storing and retrieving student records (fingerprint templates, names, roll numbers, residency type).
- Performing one-to-many fingerprint template matching against the master database.
- Creating, reading, writing, and deleting daily log files and HOME database entries.
- Hashing and indexing fingerprint templates for fast lookup.
- Binary file I/O for compact, efficient storage of biometric data.

C++ is necessary here because these operations sit directly in the critical path of every gate scan. A student should not have to wait more than a fraction of a second for identity resolution. C++ provides the low-level control over memory, binary data, and file system operations needed to meet that latency target.

### Python -- Everything Else

All application logic, workflows, and the user interface are written in Python. This includes:

- The state transition model for status derivation (IN/OUT computation).
- Purpose selection handling and the HOME approval workflow.
- Curfew monitoring and the "still outside" list generation.
- Session management (start, resume, end).
- The five-tab admin control panel (Log, Out, Systems Control, Requests, Home).
- Excel export and archival.
- All search, filter, and CRUD operations exposed through the UI.

Python is chosen for this layer because business logic and UI code change far more frequently than the underlying data engine. Python's readability, rapid iteration speed, and extensive library ecosystem (openpyxl for Excel, tkinter or PyQt for desktop UI, Flask/Django for a future web panel) make it the right tool for everything that is not bottlenecked by raw speed.

### How They Connect

The Python application layer calls into the C++ database engine through a defined interface. This can be implemented via:

- **pybind11** -- Compiling the C++ engine as a Python-importable shared library (.so / .pyd), giving Python direct function-call access to C++ routines.
- **ctypes** -- Loading the compiled C++ library at runtime and calling exported C functions from Python.
- **Subprocess / IPC** -- Running the C++ engine as a separate process and communicating via stdin/stdout, pipes, or sockets.

The choice among these will be finalised during implementation, but the principle remains: Python handles the logic, C++ handles the data.

---

## 5. Core Logic -- The State Transition Model

Rather than asking each student to press an "IN" or "OUT" button (which is error-prone and adds friction), the system dynamically determines their status by tracking their movement sequence.

Instead of relying on a pure mathematical parity count of daily scans (which is easily desynchronized when a student returns from home leave or external events), the system uses a **State Transition Model**:

1. **First Scan of the Day**:
   - The status is initialized based on the student's residency type:
     - **Hosteller** (starts day INSIDE): First scan marks them as **OUTSIDE** (`OUT`).
     - **Day Scholar** (starts day OUTSIDE): First scan marks them as **INSIDE** (`IN`).
2. **Subsequent Scans**:
   - If the student has already scanned today, the system simply toggles their previous daily status (`IN` becomes `OUT`, and `OUT` becomes `IN`).

This ensures that status transitions are perfectly sequential and self-correcting regardless of external database updates.

### State Transition Reference Table

| Scan Scenario | Student Type | Derived Status |
|---------------|--------------|----------------|
| First Scan    | Hosteller    | OUTSIDE (`OUT`)|
| First Scan    | Day Scholar  | INSIDE (`IN`)  |
| Toggle Scan   | Hosteller/Day Scholar | Opposite of previous daily status |

The residency type (hosteler or day scholar) is stored as a boolean field in the master database and is fetched alongside the student's name on every scan. This is used to set the initial default status for the first scan of the day.

---

## 6. Purpose Selection and the HOME Workflow

After the student's identity is confirmed and before the timestamp is committed, the system prompts the student to select a purpose for their gate crossing.

### Standard Purposes (Market, Exam, Medical, Class, Others)

For any non-HOME purpose, the flow is straightforward:
1. The student selects their reason.
2. The timestamp is written immediately.
3. The scan count increments by one.
4. The status is recalculated via the state transition model.
5. The record is complete.

If the student selects "Others", they enter a custom reason via keyboard.

### The HOME Purpose -- Approval Required

Going home implies an extended, possibly overnight absence. This requires explicit administrative authorisation. The flow diverges:

1. The student selects HOME.
2. A request is sent to the admin panel under the Requests tab.
3. The student is held in a pending state at the terminal. No timestamp is written yet. No status change occurs.
4. The admin reviews the request and either approves or rejects it.
   - **If approved:** The timestamp is committed, the scan count increments, status updates to OUTSIDE, and the student's record is added to a separate HOME database (student_gone_home). This student is excluded from the end-of-day "still outside" report since their absence is authorised.
   - **If rejected:** The event is discarded entirely. The scan count is decremented by one (effectively rolling back the attempt). The student remains INSIDE. A message is displayed: "The leave form is not submitted."

### Returning from HOME

When a student who was approved for HOME returns and scans their fingerprint:

1. The system first checks the HOME database (student_gone_home).
2. If the student is found there, their name is removed from the HOME database.
3. The system checks the daily log database:
   - If they **already have a log entry today** (e.g. they left for home earlier today), the system increments their gate count, appends the return timestamp, and updates their status to `IN` (with the reason updated to `Home Return`).
   - If they **do not have a log entry today** (e.g. they left for home on a previous day), a new log entry is created for today with status `IN` and count `1` (reason `Home Return`).
4. Normal gate tracking resumes from this point.

This integration resolves the duplicate daily log errors and ensures that returning students are marked inside immediately with correct transaction records.

---

## 7. Curfew Monitoring

At the designated curfew time (default: 18:30), or when the admin manually triggers a check, the system queries the daily log to identify anomalies:

- **Hostelers whose scan count is odd** -- they are OUTSIDE and have not returned.
- **Day scholars whose scan count is odd** -- they are INSIDE and have not left.

Students who have been approved for HOME are excluded from this report (they exist in the HOME database, not as "missing" students).

The resulting list is displayed on the admin panel's Out tab, complete with contact information pulled from the master database so the admin can follow up.

### Late Returns

If a student scans after curfew time (after 18:30):
1. Their scan is processed normally.
2. Their scan count increments, flipping their status back.
3. They are automatically removed from the "still outside" list.
4. A late return flag is set in their record.
5. No purpose is required for the return scan.

---

## 8. Database Architecture

### Why Separate Databases

The master student data and daily movement logs have fundamentally different lifecycles. Student records change once a year. Gate logs change hundreds of times a day. Mixing them into one database would mean running heavy write operations against a table that also needs to serve fast identity lookups on every scan. Keeping them separate means each can be optimised independently -- the master database stays largely read-only, and the log database is optimised for rapid sequential writes.

### 7.1 Master Database (Student_data)

Stores everything that defines a student's identity in the system. Updated in bulk at the start of each academic year when a new batch enrols. Individual corrections can be made through a restricted admin operation.

| Field                  | Type       | Description                                      |
|------------------------|------------|--------------------------------------------------|
| roll_number            | string     | Unique student identifier (Primary Key)          |
| name                   | string     | Full name of the student                         |
| year                   | integer    | Current academic year                            |
| phone_number           | integer    | Contact / emergency number                       |
| fingerprint_template   | uint8_t    | Biometric template stored as 8-bit binary data   |
| hosteller              | boolean    | true = hosteler, false = day scholar             |
| program                | string     | Academic program (BSc, MSc, PhD, etc.)           |
| batch                  | string     | Admission batch (e.g., 2025)                     |

Students are stored as individual CSV/TXT files, grouped inside batch folders (e.g., `2025_batch/student_name.csv`). Each batch folder also contains an index file listing all students in that batch, enabling batch-level operations like year promotion.

### 7.2 Daily Log Database (Everyday_data)

A new file is created each day, named by date (e.g., `01_01_2026.csv`). Files are organised in a folder hierarchy: Year > Month > Day.

Each student who scans during the day gets a row in this file. The row accumulates every scan as a sequence of timestamps.

| Field                          | Type      | Description                                        |
|--------------------------------|-----------|----------------------------------------------------|
| name                           | string    | Student name (fetched from master)                 |
| roll_number                    | string    | Links to master database                           |
| year                           | integer   | Current academic year                              |
| reason                         | string    | Purpose of exit (Market, Exam, Medical, Home, etc.)|
| entry_time                     | datetime  | Timestamp(s) of entry scans                        |
| exit_time                      | datetime  | Timestamp(s) of exit scans                         |
| student_pass_through_gate_count| integer   | Incremented on each scan, tracks total daily gate crossings  |
| status                         | enum      | IN or OUT (derived from state transition model)    |
| late_return                    | boolean   | true if the student scanned after 18:30            |

### 7.3 Home Database (student_gone_home)

A temporary database that exists only while students are away on approved home leave. Records are added when a HOME request is approved and removed when the student returns.

| Field          | Type      | Description                          |
|----------------|-----------|--------------------------------------|
| name           | string    | Student name                         |
| roll_number    | string    | Unique identifier                    |
| year           | integer   | Academic year                        |
| contact_number | integer   | Phone number                         |
| date_of_leaving| date      | Date the student left                |
| time_of_leaving| time      | Time the student left                |

---

## 9. Data Structures

The student record in the C++ database engine is defined as a struct:

```
struct student {
    string roll_number;
    string name;
    int year;
    int phone_number;
    uint8_t fingerprint_template;
    bool hosteller;
};
```

The `uint8_t` type (from the `<cstdint>` header) is explicitly 8 bits wide, making it the natural choice for storing fingerprint templates and serialising them to binary files. The `hosteller` boolean is passed to the application layer alongside every lookup result so the state transition logic can determine the correct default direction.

### C++ Libraries (Database Engine)

| Library        | Purpose                                                        |
|----------------|----------------------------------------------------------------|
| cstdint        | Provides uint8_t for fingerprint template storage              |
| filesystem     | File and directory creation, renaming, and path manipulation   |
| chrono         | High-resolution clocks, durations, and time points             |
| ctime          | C-style time functions for human-readable timestamps           |
| fstream        | File I/O for reading and writing student record files          |

### Python Libraries (Application Layer)

| Library            | Purpose                                                    |
|--------------------|------------------------------------------------------------|
| openpyxl / xlsxwriter | Excel file generation for log archival and export       |
| tkinter / PyQt     | Desktop GUI for the admin control panel                    |
| pybind11 / ctypes  | Interface to call the C++ database engine from Python      |
| datetime           | Date and time handling for session management              |
| os / pathlib       | File path operations for navigating the data folder hierarchy |

---

## 10. Daily Log Lifecycle and Archival

### Session Start

1. The admin starts the program.
2. The system reads the current date from the system clock.
3. It checks whether a log file for today already exists in the Everyday_data folder.
   - If yes: the admin is asked whether to continue recording on the same file or return to the admin panel.
   - If no: a new file is created for today's date.
4. The fingerprint scanner is activated and begins accepting scans.

### During the Session

- The system is in a locked recording state. No manual edits are permitted to the daily log while recording is active.
- Each fingerprint scan triggers the full flow: identity resolution, state transition status update, purpose selection, and (if applicable) HOME approval.

### Session End

1. At 18:30 (or when the admin clicks "End"), the curfew check runs.
2. The "still outside" list is generated and displayed.
3. Late-returning students are processed as they scan.
4. The admin reviews the report and shuts down the session.
5. The daily log is locked.

### Archival Strategy

Two approaches are viable, and the choice depends on institutional needs:

**Option A -- Daily Export:**
At the end of each day, the day's log file is exported to an Excel spreadsheet and the source file is cleared. This keeps the log database consistently small but produces many individual files.

**Option B -- Annual Export:**
Log files accumulate for the full academic year. At year-end, the entire year's data is exported into a single Excel workbook (one sheet per day). This allows cross-day queries and pattern analysis during the year but requires more storage.

In either case, the exported Excel files serve as the permanent archive for audits, attendance reviews, and administrative reporting.

---

## 11. Control Panel and User Interface

The control panel is the administrative interface through which all non-scanning interactions occur. It is built entirely in Python using a GUI framework (tkinter or PyQt), and communicates with the C++ database engine for all data operations. It has five main tabs.

### 11.1 Log Panel

The primary view for reviewing gate activity. Displays a table with columns: Name, Roll No., Year, Reason, Program, Role, Entry Time, Exit Time.

Features:
- **Search** -- Parse through all cells to find a specific student or value.
- **Select Period of Log** -- A calendar popup to select a date range; the system loads and displays all log files from that period.
- **Today** -- Quick shortcut to view only today's log.
- **Export** -- Export the currently displayed data to Excel.
- **Filter** -- Narrow the table by Year, Program, Role, or Reason.

### 11.2 Out Panel

Displays a real-time list of students currently outside the campus. The table includes: Name, Roll No., Program, Batch, Year, Contact Info, and Role.

A "Reason" filter allows the admin to view only students who went out for a specific purpose (Market, Exam, Medical, Home).

### 11.3 Requests Panel

Displays all pending HOME approval requests. Each request shows the student's name and provides two actions: Approve or Reject. The admin handles each request individually.

### 11.4 Home Panel

Displays students currently away on approved home leave (records from the student_gone_home database). The table includes full student details. A "Program" filter allows narrowing by UG, PG, or PhD.

### 11.5 Systems Control Panel

A password-protected administrative panel for direct database management. After authentication, two options are available:

**Edit Database:**
- **Add** -- Enrol a new student by entering: Name, Roll Number, Program, Batch, Year, Contact Info, Role, and Fingerprint. The system creates the appropriate file in the correct batch folder.
- **Remove** -- Delete a student by name and roll number. If an entire batch needs to be removed, the admin provides the program and batch, and the system deletes all student files in that batch folder followed by the folder itself.

**View Database:**
- **Log Files** -- Browse archived daily logs. Options include "Export" (copy to Excel) and "Export and Delete" (copy to Excel and remove from the database).
- **Students** -- Browse the master student database. Supports search and filtering by Program, Batch, and Year.

Additional capabilities within Edit Database:
- **Edit Student Records** -- Select a field to modify (name, roll number, etc.). The current value is shown first for confirmation before replacement.
- **Edit Gate Records** -- Enter a date (DD/MM/YYYY), load that day's log, select a student's record, and modify their entries.
- **Batch Promotion** -- Increment the academic year for an entire batch or the entire student database with a single action, using a loop to update each student's year field.

---

## 12. Implementation Path

The project will be built in the following structured phases, respecting the C++/Python split:

### Phase 1 -- C++ Database Engine

- Define the student struct with all required fields in C++.
- Build the file/folder hierarchy: Student_data (master) with batch subfolders, Everyday_data with year/month/day structure, and the student_gone_home temporary store.
- Write C++ utility functions for creating, reading, updating, and deleting student record files.
- Build the batch index file system for quick lookups.
- Compile the engine as a shared library (.so / .dll / .dylib) with a clean API surface.

### Phase 2 -- Fingerprint Input and Matching (C++)

- Integrate the fingerprint scanner hardware with the C++ database engine.
- Implement the biometric capture routine using the uint8_t template format.
- Build the one-to-many matching algorithm that compares a live scan against all enrolled templates in the master database.
- Define the acceptance threshold and implement the rejection/fail-safe path for unrecognised scans.

### Phase 3 -- Hashing and Search Optimisation (C++)

- Implement a hashing algorithm to speed up fingerprint template lookups instead of brute-force sequential comparison.
- Optimise the search path so that scan-to-match latency remains acceptable even as the enrolled student count grows.

### Phase 3.5 -- Python-to-C++ Bridge

- Build the interface layer (pybind11, ctypes, or subprocess/IPC) that allows Python to call C++ engine functions.
- Define the API contract: what Python sends (e.g., raw fingerprint data, roll number, date) and what C++ returns (e.g., matched student record, log rows, success/failure codes).
- Write Python wrapper functions that abstract the bridge so the rest of the Python codebase calls clean, Pythonic functions.

### Phase 4 -- Core Logic Integration (Python)

- Implement the state transition model for status derivation, consuming residency type from lookups.
- Build the purpose selection interface at the gate terminal in Python.
- Implement the HOME approval workflow with the request queue and admin notification.
- Build the HOME database management (Python calls C++ engine to add on approval, remove on return).
- Implement the curfew check and "still outside" list generation.

### Phase 5 -- Daily Log Management (Python + C++)

- Implement automatic date detection in Python; delegate daily file creation to the C++ engine.
- Build the session start/resume/end flow in Python.
- Implement timestamp recording and scan count incrementing (Python triggers, C++ writes).
- Build the late return handling logic in Python.

### Phase 6 -- Control Panel Development (Python)

- Build the five-tab interface (Log, Out, Systems Control, Requests, Home) using tkinter or PyQt.
- Implement search, filter, and export functionality for the Log panel.
- Implement the real-time Out panel with reason-based filtering.
- Build the Requests panel with approve/reject actions.
- Build the Home panel with program-based filtering.
- Implement the password-protected Systems Control panel with all CRUD operations (all data operations routed through the C++ engine via the bridge).

### Phase 7 -- Archival and Export (Python)

- Implement the Excel export functionality using openpyxl or xlsxwriter in Python.
- Build the archival workflow (daily or annual, per institutional preference).
- Implement the "Export and Delete" option (Python triggers export, then calls C++ engine to delete source data).

### Phase 8 -- Integration and End-to-End Testing

- Ensure seamless data flow between the C++ engine (managing all three databases) and the Python application layer.
- Verify that every Python workflow (scan processing, HOME approval, curfew check, CRUD operations) correctly round-trips through the C++ bridge.
- End-to-end testing of the full scan-to-archive pipeline across both languages.

---

## 13. Known Flaws and Limitations

### State Transition Assumptions

The state transition model assumes a student always scans at the gate. If a student tailgates (follows someone through without scanning), the state sequence becomes desynchronised. Their status will be inverted from that point forward until an admin manually corrects it, or the day rolls over.

The current design assumes a single gate. If the campus has multiple gates, a student could exit through one gate and enter through another. Without a shared, centralised database accessible from all gates in real time, the scan sequence would become inconsistent.

### Fingerprint Quality Degradation

Biometric scanners can fail to recognise a legitimate student due to wet fingers, cuts, worn-out sensor surfaces, or poor initial enrollment quality. The system handles this via the fail-safe (unrecognised scan is logged and admin is alerted), but it creates friction for the affected student.

### Offline Operation

The system is designed as a local application. If the machine running the software crashes mid-day, recovery depends on the last state of the daily log file. There is no replication or cloud backup built into the current design.

### HOME Workflow Bottleneck

The HOME approval is synchronous -- the student waits at the terminal until the admin responds. If the admin is not actively monitoring the Requests panel, the student could be stuck waiting for an indeterminate amount of time, blocking the scanner for other students.

### File-Based Storage Limitations

The custom C++ database engine uses flat files rather than a relational database. While C++ provides the speed for individual lookups, flat files still lack complex query support, referential integrity enforcement, and concurrent access handling. As the student population grows, the engine's indexing strategy must scale accordingly.

### Bridge Complexity

The interface between Python and C++ introduces a coupling point. If the C++ engine's API changes, the Python bridge layer must be updated in lockstep. Debugging issues that span both languages (e.g., a corrupted record from C++ surfacing as unexpected behaviour in Python) can be more complex than debugging a single-language system.

### No Authentication at the Terminal

The system relies entirely on the fingerprint for identity. There is no secondary verification (such as a PIN or student ID card). If the biometric engine produces a false positive match, the wrong student's record is updated.

### C++ Compiler Dependency

The C++ database engine must be compiled for the target platform. If the deployment machine differs from the development machine (e.g., different OS, architecture), cross-compilation or platform-specific builds will be required. Using only standard C++ headers (avoiding non-standard headers like `<bits/stdc++.h>`) is important for portability.

---

## 14. Future Additions

### Multi-Gate Support

Deploy scanners at multiple gates, all connecting to the same C++ database engine (potentially exposed over a local network socket). The state transition model would still work since the status and sequence are stored centrally, not per-gate.

### Evolving the C++ Engine to Support SQL

Extend the custom C++ database engine to support a subset of SQL-like query syntax internally, or alternatively integrate SQLite as the storage backend within the engine. This would enable more complex queries and transactions while preserving the speed advantage of native C++ I/O.

### Web-Based Control Panel

Since the control panel is already written in Python, transitioning to a web-based interface (using Flask or Django) is a natural evolution. The C++ database engine would remain unchanged; only the Python UI layer would be swapped from desktop (tkinter/PyQt) to web. This would allow administrators to monitor gate activity, approve HOME requests, and manage records from any device on the campus network.

### Mobile Notifications for HOME Approvals

Instead of requiring the admin to watch the Requests panel, send push notifications or SMS alerts when a HOME request is submitted. This would reduce the bottleneck at the terminal.

### Attendance Analytics

With daily logs accumulated over time, the system has the raw data to generate attendance reports, identify patterns (students who are frequently late, who leave for medical reasons often, etc.), and provide dashboards for hostel wardens and academic administrators.

### Dual Biometric or Multi-Factor Authentication

Add a secondary verification method (RFID card, PIN, or facial recognition) to reduce the risk of false positive matches and handle cases where fingerprint scanning fails.

### Automated Batch Promotion Scheduling

Instead of requiring the admin to manually trigger batch promotion at year-end, schedule it to run automatically at a configured date.

### Parent/Guardian Notification System

When a student's HOME request is approved or when a student appears on the "still outside after curfew" list, automatically send an SMS or email notification to their registered parent or guardian.

### Cloud Backup and Disaster Recovery

Implement automatic backup of all databases to a cloud storage provider so that data is recoverable in case of hardware failure.

### Tailgating Detection

Integrate with turnstile hardware or infrared sensors to detect when multiple people pass through on a single scan, flagging potential sequence desynchronisation.

