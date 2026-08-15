# Implementation Plan — Phase 4 & 5: Python Bridge + Core Logic

---

## What Phase 4 & 5 Will Produce

After these two phases, you will have:

1. A **pybind11 binding module** (`gate_engine`) that Python can `import` natively.
2. A **Pythonic wrapper layer** (`engine_wrapper.py`) with type hints, error handling, and dataclass conversions.
3. Five **core logic modules** — parity, purpose, home workflow, curfew, and session management.
4. A **console-based integration test** that runs the full scan → identify → log → curfew pipeline from Python (no GUI yet — that's Phase 6).

---

## Phase 4 — Python-to-C++ Bridge (pybind11)

### 4.1 Technology Choice: Why pybind11

> [!NOTE]
> The main implementation plan discusses both `ctypes` and `pybind11`. We are going with **pybind11** because:
>
> - It natively understands C++ types — `std::vector`, `std::string`, structs, booleans — and maps them directly to Python objects. No manual `ctypes.Structure` definitions needed.
> - It compiles a proper Python extension module (`.so` on macOS/Linux, `.pyd` on Windows) that you `import` like any Python package.
> - Debugging is cleaner — errors in bindings show up at compile time, not at runtime as cryptic segfaults.
> - The existing C++ API in [engine.h](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/include/engine.h) already returns `std::vector<T>` and passes `const T&` — pybind11 handles these natively.

### 4.2 Dependencies to Install

```bash
# pybind11 (the C++ header-only library)
pip install pybind11

# Verify Python + pybind11 paths (needed by CMake)
python3 -c "import pybind11; print(pybind11.get_cmake_dir())"
```

### 4.3 Files to Create / Modify

---

#### [NEW] `cpp_engine/bindings/pybind_module.cpp`

This is the single binding file that wraps every C++ function and struct for Python.

**What it does:**

- Registers all four structs (`StudentRecord`, `LogEntry`, `HomeRecord`, `MatchResult`) as Python-accessible classes with readable property attributes.
- Wraps every API function from [engine.h](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/include/engine.h) — all 25 functions.
- Handles the `uint8_t fingerprint_template[512]` field by exposing it as a Python `bytes` object (for reads) and accepting `bytes` input (for writes/matching).
- Handles `char[]` fields as Python `str` via automatic conversion.

**Struct binding example (StudentRecord):**

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>       // for std::vector conversion
#include "engine.h"

namespace py = pybind11;

PYBIND11_MODULE(gate_engine, m) {
    m.doc() = "Campus Biometric Entry Management System — C++ Database Engine";

    // -- StudentRecord --
    py::class_<StudentRecord>(m, "StudentRecord")
        .def(py::init<>())
        .def_property("roll_number",
            [](const StudentRecord& s) { return std::string(s.roll_number); },
            [](StudentRecord& s, const std::string& v) {
                std::strncpy(s.roll_number, v.c_str(), sizeof(s.roll_number) - 1);
                s.roll_number[sizeof(s.roll_number) - 1] = '\0';
            })
        .def_property("name",
            [](const StudentRecord& s) { return std::string(s.name); },
            [](StudentRecord& s, const std::string& v) {
                std::strncpy(s.name, v.c_str(), sizeof(s.name) - 1);
                s.name[sizeof(s.name) - 1] = '\0';
            })
        // ... same pattern for program, batch, phone_number ...
        .def_readwrite("year", &StudentRecord::year)
        .def_readwrite("is_hosteller", &StudentRecord::is_hosteller)
        .def_property("fingerprint_template",
            [](const StudentRecord& s) {
                return py::bytes(reinterpret_cast<const char*>(s.fingerprint_template), 512);
            },
            [](StudentRecord& s, py::bytes data) {
                std::string raw = std::string(data);
                std::memcpy(s.fingerprint_template, raw.data(), std::min(raw.size(), (size_t)512));
            });

    // ... LogEntry, HomeRecord, MatchResult similarly ...

    // -- Engine lifecycle --
    m.def("engine_init", &engine_init, py::arg("project_root_path"));
    m.def("engine_shutdown", &engine_shutdown);

    // -- Master DB --
    m.def("student_add", &student_add, py::arg("record"));
    m.def("student_remove", &student_remove, py::arg("roll_number"));
    // ... all 25 functions ...
}
```

**Full function list to bind** (all from [engine.h](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/include/engine.h)):

| # | C++ Function | Python Signature |
| --- | --- | --- |
| 1 | `engine_init(const char*)` | `engine_init(path: str) -> bool` |
| 2 | `engine_shutdown()` | `engine_shutdown() -> None` |
| 3 | `student_add(const StudentRecord&)` | `student_add(record: StudentRecord) -> bool` |
| 4 | `student_remove(const char*)` | `student_remove(roll: str) -> bool` |
| 5 | `student_update(const char*, const StudentRecord&)` | `student_update(roll: str, record: StudentRecord) -> bool` |
| 6 | `student_get(const char*, StudentRecord&)` | `student_get(roll: str) -> StudentRecord or None` |
| 7 | `student_list_by_batch(const char*)` | `student_list_by_batch(batch: str) -> list[StudentRecord]` |
| 8 | `student_list_all()` | `student_list_all() -> list[StudentRecord]` |
| 9 | `batch_promote(const char*)` | `batch_promote(batch: str) -> int` |
| 10 | `batch_promote_all()` | `batch_promote_all() -> int` |
| 11 | `batch_delete(const char*)` | `batch_delete(batch: str) -> bool` |
| 12 | `fingerprint_match(const uint8_t*, int)` | `fingerprint_match(scan_bytes: bytes) -> MatchResult` |
| 13 | `fingerprint_enroll(const char*, const uint8_t*, int)` | `fingerprint_enroll(roll: str, template_bytes: bytes) -> bool` |
| 14 | `log_create_day(const char*)` | `log_create_day(date: str) -> bool` |
| 15 | `log_day_exists(const char*)` | `log_day_exists(date: str) -> bool` |
| 16 | `log_add_entry(const char*, const LogEntry&)` | `log_add_entry(date: str, entry: LogEntry) -> bool` |
| 17 | `log_update_entry(const char*, const char*, const LogEntry&)` | `log_update_entry(date: str, roll: str, entry: LogEntry) -> bool` |
| 18 | `log_get_entry(const char*, const char*, LogEntry&)` | `log_get_entry(date: str, roll: str) -> LogEntry or None` |
| 19 | `log_get_all_entries(const char*)` | `log_get_all_entries(date: str) -> list[LogEntry]` |
| 20 | `log_get_entries_in_range(const char*, const char*)` | `log_get_entries_in_range(start: str, end: str) -> list[LogEntry]` |
| 21 | `log_delete_day(const char*)` | `log_delete_day(date: str) -> bool` |
| 22 | `home_add(const HomeRecord&)` | `home_add(record: HomeRecord) -> bool` |
| 23 | `home_remove(const char*)` | `home_remove(roll: str) -> bool` |
| 24 | `home_exists(const char*)` | `home_exists(roll: str) -> bool` |
| 25 | `home_get_all()` | `home_get_all() -> list[HomeRecord]` |
| 26 | `rejection_log_write(const char*, const uint8_t*, int)` | `rejection_log_write(date: str, scan: bytes) -> bool` |

> [!IMPORTANT]
> **Special handling needed for functions 6 and 18** — `student_get` and `log_get_entry` take output parameters by reference in C++. In the pybind11 wrapper, we need to write small lambda shims that return `std::optional<T>` or `py::none()` on failure instead of mutating a reference parameter. This makes them Pythonic (`result = student_get("26CSE001")` instead of `ok, result = student_get("26CSE001", out_ref)`).

---

#### [MODIFY] `cpp_engine/CMakeLists.txt` this i will do in the code base no need to worry about this

Add pybind11 integration to the existing build system:

```cmake
# --- Existing content stays unchanged ---

# NEW: pybind11 Python module
find_package(pybind11 REQUIRED)

pybind11_add_module(gate_engine_py bindings/pybind_module.cpp)
target_link_libraries(gate_engine_py PRIVATE gate_engine)
set_target_properties(gate_engine_py PROPERTIES OUTPUT_NAME "gate_engine")
```

After building, this produces `gate_engine.cpython-3XX-darwin.so` — a file that Python can directly `import gate_engine`.

---

### 4.4 Python Wrapper Layer

#### [NEW] `python_app/bridge/__init__.py`

Empty init file.

#### [NEW] `python_app/bridge/engine_wrapper.py`

A clean Pythonic interface over the raw C++ bindings. This is what the rest of the Python app imports — nobody imports `gate_engine` directly except this file.

**What it does:**

- Imports the compiled `gate_engine` module.
- Provides functions with Python type hints, docstrings, and proper error handling.
- Converts raw C++ struct objects into Python `dataclasses` for cleaner downstream use.
- Wraps exceptions so a C++ crash doesn't propagate an ugly error to the UI.

**Key design decisions:**

- Returns `Optional[StudentRecord]` instead of raising exceptions for "not found" cases.
- Automatically formats date strings to `DD_MM_YYYY` (the format the C++ engine expects).
- Provides a context manager for engine lifecycle (`with EngineSession(path) as engine:`).

```python
"""
engine_wrapper.py — Pythonic interface to the C++ gate_engine module.
all the funtions will be called from here 
"""
from __future__ import annotations
import sys
from dataclasses import dataclass
from typing import Optional
from datetime import datetime

# The compiled pybind11 module
import gate_engine as _engine


# ─── Python Dataclasses (mirrors of C++ structs) ───

@dataclass
class Student:
    roll_number: str
    name: str
    program: str
    batch: str
    year: int
    phone_number: str
    is_hosteller: bool
    fingerprint_template: bytes = b""

@dataclass
class LogRecord:
    roll_number: str
    name: str
    year: int
    reason: str
    gate_count: int
    status: str               # "IN" or "OUT"
    late_return: bool
    timestamps: list[str]     # up to 20

@dataclass
class HomeLeave:
    roll_number: str
    name: str
    year: int
    phone_number: str
    date_of_leaving: str
    time_of_leaving: str

@dataclass
class ScanResult:
    matched: bool
    roll_number: str = ""
    name: str = ""
    program: str = ""
    batch: str = ""
    year: int = 0
    phone_number: str = ""
    is_hosteller: bool = False
    confidence_score: float = 0.0


# ─── Conversion Helpers (C++ struct ↔ Python dataclass) ───

def _to_student(cpp_record) -> Student: ...
def _from_student(py_student: Student) -> _engine.StudentRecord: ...
def _to_log(cpp_entry) -> LogRecord: ...
def _from_log(py_record: LogRecord) -> _engine.LogEntry: ...
# ... similar for HomeLeave, ScanResult ...


# ─── Engine Lifecycle ───

def init(project_root: str) -> bool:
    """Initialize the C++ engine. Must be called once at startup."""
    return _engine.engine_init(project_root)

def shutdown() -> None:
    """Flush and release the C++ engine."""
    _engine.engine_shutdown()


# ─── Master Database ───

def add_student(student: Student) -> bool: ...
def remove_student(roll_number: str) -> bool: ...
def update_student(roll_number: str, student: Student) -> bool: ...
def get_student(roll_number: str) -> Optional[Student]: ...
def list_students_by_batch(batch: str) -> list[Student]: ...
def list_all_students() -> list[Student]: ...
def promote_batch(batch: str) -> int: ...
def promote_all_batches() -> int: ...
def delete_batch(batch: str) -> bool: ...


# ─── Fingerprint ───

def match_fingerprint(scan_bytes: bytes) -> ScanResult: ...
def enroll_fingerprint(roll_number: str, template_bytes: bytes) -> bool: ...


# ─── Daily Log ───

def create_day_log(date: str) -> bool: ...
def day_log_exists(date: str) -> bool: ...
def add_log_entry(date: str, record: LogRecord) -> bool: ...
def update_log_entry(date: str, roll_number: str, record: LogRecord) -> bool: ...
def get_log_entry(date: str, roll_number: str) -> Optional[LogRecord]: ...
def get_all_log_entries(date: str) -> list[LogRecord]: ...
def get_log_entries_in_range(start: str, end: str) -> list[LogRecord]: ...
def delete_day_log(date: str) -> bool: ...


# ─── Home Database ───

def add_to_home(record: HomeLeave) -> bool: ...
def remove_from_home(roll_number: str) -> bool: ...
def is_on_home_leave(roll_number: str) -> bool: ...
def get_all_home_leave() -> list[HomeLeave]: ...


# ─── Rejection Log ───

def log_rejection(date: str, failed_scan: bytes) -> bool: ...
```

> [!TIP]
> **Why the wrapper instead of calling `gate_engine` directly?**
>
> 1. The rest of the Python app only depends on clean Python types (`Student`, `LogRecord`, etc.), not on the C++ struct objects.
> 2. If you ever swap pybind11 for ctypes, or replace the C++ engine with SQLite, **only this one file changes**. Everything else stays the same.
> 3. Date formatting, error handling, and logging live here — not scattered across 10 modules.

---

### 4.5 Updated Folder Structure After Phase 4

```txt
project_root/
|
|-- cpp_engine/                             <-- EXISTING (Phases 1-3)
|   |-- include/
|   |   |-- engine.h
|   |   |-- serializer.h
|   |   |-- indexer.h
|   |   +-- touch_id.h
|   |-- src/
|   |   |-- master_db.cpp
|   |   |-- daily_log.cpp
|   |   |-- home_db.cpp
|   |   |-- serializer.cpp
|   |   |-- indexer.cpp
|   |   |-- fingerprint.cpp
|   |   |-- touch_id.mm
|   |   +-- main.cpp
|   |-- bindings/                           <-- NEW (Phase 4)
|   |   +-- pybind_module.cpp
|   +-- CMakeLists.txt                      <-- MODIFIED (Phase 4)
|
|-- python_app/                             <-- NEW (Phase 4 + 5)
|   |-- __init__.py
|   |-- main.py                             <-- NEW (Phase 5)
|   |-- config.py                           <-- NEW (Phase 5)
|   |-- bridge/
|   |   |-- __init__.py                     <-- NEW (Phase 4)
|   |   +-- engine_wrapper.py               <-- NEW (Phase 4)
|   +-- logic/
|       |-- __init__.py                     <-- NEW (Phase 5)
|       |-- parity.py                       <-- NEW (Phase 5)
|       |-- purpose.py                      <-- NEW (Phase 5)
|       |-- home_workflow.py                <-- NEW (Phase 5)
|       |-- curfew.py                       <-- NEW (Phase 5)
|       +-- session.py                      <-- NEW (Phase 5)
```

---

### 4.6 Phase 4 Verification

1. **Build the module:**

   ```bash
   cd cpp_engine/build
   cmake ..
   make
   ```

   Confirm output includes `gate_engine.cpython-3XX-darwin.so`.

2. **Python smoke test:**

   ```python
   import gate_engine

   # Init
   assert gate_engine.engine_init("test_db_root") == True

   # Create a student
   student = gate_engine.StudentRecord()
   student.roll_number = "26CSE001"
   student.name = "Devansh"
   student.program = "MSc"
   student.batch = "2026"
   student.year = 1
   student.phone_number = "+919876543210"
   student.is_hosteller = True
   student.fingerprint_template = bytes([ord('2')] * 512)  # mock template

   assert gate_engine.student_add(student) == True

   # Read it back
   result = gate_engine.student_get("26CSE001")
   assert result is not None
   assert result.name == "Devansh"
   assert result.is_hosteller == True

   print("Phase 4 verification: ALL PASSED")
   gate_engine.engine_shutdown()
   ```

3. **Wrapper test:**

   ```python
   from python_app.bridge import engine_wrapper as ew

   ew.init("test_db_root")
   student = ew.Student(
       roll_number="26CSE001", name="Devansh", program="MSc",
       batch="2026", year=1, phone_number="+919876543210",
       is_hosteller=True, fingerprint_template=bytes([42] * 512)
   )
   assert ew.add_student(student) == True
   fetched = ew.get_student("26CSE001")
   assert fetched is not None
   assert fetched.name == "Devansh"
   print("Wrapper verification: ALL PASSED")
   ew.shutdown()
   ```

---

## Phase 5 — Python Core Logic

Phase 5 builds the five logic modules that sit between the bridge layer and the UI. These modules contain **zero disk I/O** — they call `engine_wrapper` for all data operations.

---

### 5.1 `python_app/config.py` — Central Configuration

```python
"""
config.py — All tuneable parameters in one place.
"""
from pathlib import Path

# Paths
PROJECT_ROOT = Path(__file__).resolve().parent.parent
DB_ROOT = PROJECT_ROOT / "db_root"

# Curfew
CURFEW_HOUR = 18
CURFEW_MINUTE = 30

# Fingerprint matching
CONFIDENCE_THRESHOLD = 0.75    # must match C++ engine default currently its 0.75 so we keep it like that for now 

# Date format (matches C++ engine expectation)
DATE_FORMAT = "%d_%m_%Y"
TIMESTAMP_FORMAT = "%Y-%m-%d %H:%M:%S"
TIME_FORMAT = "%H:%M:%S"

# Purposes
HOSTELLER_PURPOSES = ["Market", "Medical", "Exam", "Home", "Others"]
DAY_SCHOLAR_PURPOSES = ["Class", "Others"]

# Admin
ADMIN_PASSWORD_HASH = None     # will be set in Phase 6

# Session
MAX_SCANS_PER_DAY = 20         # matches MAX_TIMESTAMPS in engine.h
```

---

### 5.2 `python_app/logic/parity.py` — IN/OUT Status Engine

This is the core intellectual logic of the system (Section 5 of the [README](file:///Users/devanshkhosla/Projects/Test%20folder/README.md)).

```python
"""
parity.py — Derives whether a student is IN or OUT from their gate count.

The parity model:
    Hosteller  + even count = INSIDE   (they start inside)
    Hosteller  + odd count  = OUTSIDE
    Day Scholar + odd count  = INSIDE   (they start outside)
    Day Scholar + even count = OUTSIDE
"""

def compute_status(has_scanned_today: bool, previous_status: str, is_hosteller: bool) -> str:
    """
    Returns "IN" or "OUT" based on the state transition model.

    Args:
        has_scanned_today: True if the student has scanned today.
        previous_status: The previous daily status ("IN" or "OUT") if scanned today.
        is_hosteller: True if hosteller, False if day scholar.

    Returns:
        "IN" or "OUT"
    """
    if not has_scanned_today:
        return "OUT" if is_hosteller else "IN"
    else:
        return "OUT" if previous_status == "IN" else "IN"


def is_going_out(has_scanned_today: bool, previous_status: str, is_hosteller: bool) -> bool:
    """Returns True if this scan means the student is now leaving campus."""
    return compute_status(has_scanned_today, previous_status, is_hosteller) == "OUT"


def needs_purpose_selection(has_scanned_today: bool, previous_status: str, is_hosteller: bool) -> bool:
    """
    Purpose is only asked when:
      - A hosteller is going OUT (leaving campus)
      - A day scholar is going IN (entering campus)
    """
    status = compute_status(has_scanned_today, previous_status, is_hosteller)
    if is_hosteller and status == "OUT":
        return True
    if not is_hosteller and status == "IN":
        return True
    return False
```

> [!NOTE]
> The parity model matches what's already proven in the C++ CLI (`main.cpp`, lines 245–253). We're just extracting it into a clean, testable Python function.

---

### 5.3 `python_app/logic/purpose.py` — Purpose Selection Handler

```python
"""
purpose.py — Handles the purpose selection flow after a scan.
Functions INcludes as follow:
  - get_purpose_options()   -> list of valid purposes for this student type
  - validate_purpose()      -> checks if a selected purpose string is valid
  - is_home_purpose()       -> True if the student selected HOME
"""
from python_app.config import HOSTELLER_PURPOSES, DAY_SCHOLAR_PURPOSES


def get_purpose_options(is_hosteller: bool) -> list[str]:
    """Returns the list of selectable exit/entry purposes."""
    if is_hosteller:
        return HOSTELLER_PURPOSES.copy()
    else:
        return DAY_SCHOLAR_PURPOSES.copy()


def validate_purpose(purpose: str, is_hosteller: bool) -> bool:
    """Validates whether a purpose string is acceptable."""
    valid = get_purpose_options(is_hosteller)
    # "Others" means any custom string is fine
    if "Others" in valid:
        return True
    return purpose in valid


def is_home_purpose(purpose: str) -> bool:
    """Returns True if this purpose requires the HOME approval workflow."""
    return purpose.strip().lower() == "home"
```

---

### 5.4 `python_app/logic/home_workflow.py` — HOME Approval Engine

This module manages the full HOME lifecycle:

1. Student selects HOME → request is queued.
2. Admin approves or rejects.
3. If approved → student added to Home DB, log updated.
4. If rejected → gate count rolled back, event discarded.
5. When student returns → detected on scan, removed from Home DB.

```python
from __future__ import annotations
from dataclasses import dataclass, field
from datetime import datetime
from typing import Optional, Callable
from enum import Enum

from python_app.bridge import engine_wrapper as ew
from python_app.config import DATE_FORMAT, TIME_FORMAT


class RequestStatus(Enum):
    PENDING = "pending"
    APPROVED = "approved"
    REJECTED = "rejected"


@dataclass
class HomeRequest:
    """A pending HOME leave request waiting for admin action."""
    roll_number: str
    name: str
    year: int
    phone_number: str
    is_hosteller: bool
    timestamp: str                         # when the request was made
    status: RequestStatus = RequestStatus.PENDING


class HomeWorkflowManager:

    def __init__(self):
        self._pending_requests: list[HomeRequest] = []
        self._on_request_callback: Optional[Callable] = None

    def submit_request(self, scan_result: ew.ScanResult, timestamp: str) -> HomeRequest:
        """
        Called when a student selects HOME as their exit purpose.
        Creates a pending request and notifies the admin panel.
        """
        request = HomeRequest(
            roll_number=scan_result.roll_number,
            name=scan_result.name,
            year=scan_result.year,
            phone_number=scan_result.phone_number,
            is_hosteller=scan_result.is_hosteller,
            timestamp=timestamp,
        )
        self._pending_requests.append(request)

        # Notify UI callback if registered (Phase 6 hooks into this)
        if self._on_request_callback:
            self._on_request_callback(request)

        return request

    def approve(self, request: HomeRequest) -> bool:
        """
        Admin approves the HOME request.
          - Writes HomeRecord to the C++ Home DB.
          - Updates request status.
        """
        now = datetime.now()
        home_record = ew.HomeLeave(
            roll_number=request.roll_number,
            name=request.name,
            year=request.year,
            phone_number=request.phone_number,
            date_of_leaving=now.strftime(DATE_FORMAT),
            time_of_leaving=now.strftime(TIME_FORMAT),
        )
        success = ew.add_to_home(home_record)
        if success:
            request.status = RequestStatus.APPROVED
            self._pending_requests.remove(request)
        return success

    def reject(self, request: HomeRequest) -> None:
        """
        Admin rejects the HOME request.
          - The caller (scan processor) is responsible for rolling back gate_count.
          - Updates request status.
        """
        request.status = RequestStatus.REJECTED
        self._pending_requests.remove(request)

    def get_pending_requests(self) -> list[HomeRequest]:
        """Returns all currently pending HOME requests."""
        return self._pending_requests.copy()

    def check_returning_from_home(self, roll_number: str) -> bool:
        """
        Called on every scan BEFORE normal log processing.
        Returns True if the student is currently on approved home leave.
        """
        return ew.is_on_home_leave(roll_number)

    def process_home_return(self, roll_number: str, date: str, timestamp: str) -> bool:
        """
        Handles a student returning from home:
          1. Remove from Home DB
          2. Update or create a log entry with status "IN" and reason "Home Return"
        """
        ew.remove_from_home(roll_number)

        # Get student details for the log entry
        student = ew.get_student(roll_number)
        if student is None:
            return False

        existing = ew.get_log_entry(date, roll_number)
        if existing is None:
            log_record = ew.LogRecord(
                roll_number=roll_number,
                name=student.name,
                year=student.year,
                reason="Home Return",
                gate_count=1,
                status="IN",
                late_return=False,
                timestamps=[timestamp],
            )
            return ew.add_log_entry(date, log_record)
        else:
            existing.gate_count += 1
            existing.reason = "Home Return"
            existing.status = "IN"
            existing.timestamps.append(timestamp)
            return ew.update_log_entry(date, roll_number, existing)

    def register_request_callback(self, callback: Callable[[HomeRequest], None]):
        """Register a callback for when a new request arrives (used by UI in Phase 6)."""
        self._on_request_callback = callback
```

---

### 5.5 `python_app/logic/curfew.py` — Curfew Compliance Checker

```python
"""
idk what this Curfew logic should be for now i just followed the same logic in the README/FinalDraft
"""
from __future__ import annotations
from dataclasses import dataclass
from datetime import datetime

from python_app.bridge import engine_wrapper as ew
from python_app.config import CURFEW_HOUR, CURFEW_MINUTE


@dataclass
class CurfewViolation:
    """One student who is not where they should be."""
    roll_number: str
    name: str
    student_type: str        # "Hosteller" or "Day Scholar"
    current_status: str      # "IN" or "OUT"
    phone_number: str
    violation_reason: str    # human-readable explanation


def is_past_curfew(now: datetime | None = None) -> bool:
    """Returns True if the current time is past the configured curfew."""
    if now is None:
        now = datetime.now()
    if now.hour > CURFEW_HOUR:
        return True
    if now.hour == CURFEW_HOUR and now.minute >= CURFEW_MINUTE:
        return True
    return False


def run_curfew_check(date: str) -> list[CurfewViolation]:
   
    # Fetch all data from C++ engine
    all_logs = ew.get_all_log_entries(date)
    home_leave_list = ew.get_all_home_leave()
    home_rolls = {h.roll_number for h in home_leave_list}

    violations: list[CurfewViolation] = []

    for log in all_logs:
        # Get the master record for student type and phone
        student = ew.get_student(log.roll_number)
        if student is None:
            continue

        is_hosteller = student.is_hosteller
        status = log.status

        violation = None

        if is_hosteller and status == "OUT":
            # Hosteller is outside — but if on approved home leave, that's fine
            if log.roll_number not in home_rolls:
                violation = CurfewViolation(
                    roll_number=log.roll_number,
                    name=log.name,
                    student_type="Hosteller",
                    current_status="OUT",
                    phone_number=student.phone_number,
                    violation_reason="Hosteller is outside campus without approved Home leave",
                )

        elif not is_hosteller and status == "IN":
            # Day scholar is still inside campus past curfew
            violation = CurfewViolation(
                roll_number=log.roll_number,
                name=log.name,
                student_type="Day Scholar",
                current_status="IN",
                phone_number=student.phone_number,
                violation_reason="Day Scholar is still inside campus after curfew",
            )

        if violation:
            violations.append(violation)

    return violations
```

---

### 5.6 `python_app/logic/session.py` — Session Lifecycle Manager

```python
"""
session.py — Manages the daily session lifecycle.
"""
from __future__ import annotations
from datetime import datetime
from typing import Optional, Callable
from enum import Enum

from python_app.bridge import engine_wrapper as ew
from python_app.logic import parity, purpose, curfew
from python_app.logic.home_workflow import HomeWorkflowManager, HomeRequest, RequestStatus
from python_app.config import (
    DATE_FORMAT, TIMESTAMP_FORMAT, TIME_FORMAT,
    CURFEW_HOUR, CURFEW_MINUTE, MAX_SCANS_PER_DAY,
)


class SessionState(Enum):
    NOT_STARTED = "not_started"
    ACTIVE = "active"
    ENDED = "ended"


@dataclass
class ScanOutcome:
    """The full result of processing one scan — returned to the UI."""
    success: bool
    student_name: str = ""
    roll_number: str = ""
    status: str = ""               # "IN" or "OUT"
    gate_count: int = 0
    purpose: str = ""
    is_late_return: bool = False
    is_home_return: bool = False
    requires_home_approval: bool = False
    home_request: Optional[HomeRequest] = None
    error_message: str = ""


class SessionManager:
    """
    Manages a single day's recording session.
    """

    def __init__(self):
        self.state = SessionState.NOT_STARTED
        self.today: str = ""
        self.home_manager = HomeWorkflowManager()

    def start_session(self, db_root: str) -> tuple[bool, str]:
        """
        Initializes the engine and prepares today's log.
        """
        if not ew.init(db_root):
            return False, "Failed to initialize C++ engine."

        self.today = datetime.now().strftime(DATE_FORMAT)

        if ew.day_log_exists(self.today):
            self.state = SessionState.ACTIVE
            return True, f"Resuming existing log for {self.today}."
        else:
            ew.create_day_log(self.today)
            self.state = SessionState.ACTIVE
            return True, f"New session started for {self.today}."

    def process_scan(
        self,
        fingerprint_bytes: bytes,
        selected_purpose: Optional[str] = None,
    ) -> ScanOutcome:
        if self.state != SessionState.ACTIVE:
            return ScanOutcome(success=False, error_message="Session not active.")

        now = datetime.now()
        timestamp = now.strftime(TIMESTAMP_FORMAT)

        # Step 1: Match fingerprint
        match = ew.match_fingerprint(fingerprint_bytes)
        if not match.matched:
            ew.log_rejection(self.today, fingerprint_bytes)
            return ScanOutcome(
                success=False,
                error_message="Fingerprint not recognised.",
            )

        # Step 2: Check home return
        if self.home_manager.check_returning_from_home(match.roll_number):
            self.home_manager.process_home_return(
                match.roll_number, self.today, timestamp
            )
            updated = ew.get_log_entry(self.today, match.roll_number)
            gate_count = updated.gate_count if updated else 1
            return ScanOutcome(
                success=True,
                student_name=match.name,
                roll_number=match.roll_number,
                status="IN",
                gate_count=gate_count,
                purpose="Home Return",
                is_home_return=True,
            )

        # Step 3: Load or create log entry
        existing = ew.get_log_entry(self.today, match.roll_number)
        if existing is None:
            gate_count = 1
            timestamps_list = [timestamp]
        else:
            if existing.gate_count >= MAX_SCANS_PER_DAY:
                return ScanOutcome(
                    success=False,
                    error_message=f"Max daily scans ({MAX_SCANS_PER_DAY}) reached.",
                )
            gate_count = existing.gate_count + 1
            timestamps_list = existing.timestamps + [timestamp]

        # Step 4: Compute status
        has_scanned_today = (existing is not None)
        previous_status = existing.status if existing else ""
        status = parity.compute_status(has_scanned_today, previous_status, match.is_hosteller)

        # Step 5: Check if purpose needed
        if parity.needs_purpose_selection(has_scanned_today, previous_status, match.is_hosteller):
            if selected_purpose is None:
                # Caller needs to ask for purpose and call again
                return ScanOutcome(
                    success=True,
                    student_name=match.name,
                    roll_number=match.roll_number,
                    status=status,
                    gate_count=gate_count,
                    error_message="AWAITING_PURPOSE",
                )
            chosen_purpose = selected_purpose
        else:
            chosen_purpose = "Entry" if status == "IN" else "Exit"

        # Step 6: HOME approval flow
        if purpose.is_home_purpose(chosen_purpose):
            request = self.home_manager.submit_request(match, timestamp)
            return ScanOutcome(
                success=True,
                student_name=match.name,
                roll_number=match.roll_number,
                status=status,
                gate_count=gate_count,
                purpose="Home",
                requires_home_approval=True,
                home_request=request,
            )

        # Step 7: Check late return
        is_late = False
        if curfew.is_past_curfew(now) and status == "IN" and match.is_hosteller:
            is_late = True

        # Step 8: Write log entry
        log_record = ew.LogRecord(
            roll_number=match.roll_number,
            name=match.name,
            year=match.year,
            reason=chosen_purpose,
            gate_count=gate_count,
            status=status,
            late_return=is_late,
            timestamps=timestamps_list,
        )

        if existing is None:
            ew.add_log_entry(self.today, log_record)
        else:
            ew.update_log_entry(self.today, match.roll_number, log_record)

        return ScanOutcome(
            success=True,
            student_name=match.name,
            roll_number=match.roll_number,
            status=status,
            gate_count=gate_count,
            purpose=chosen_purpose,
            is_late_return=is_late,
        )

    def finalize_home_approval(self, request: HomeRequest, approved: bool) -> ScanOutcome:
        """
        Called by the admin UI when they approve or reject a HOME request.
        """
        if approved:
            self.home_manager.approve(request)
            return ScanOutcome(
                success=True,
                student_name=request.name,
                roll_number=request.roll_number,
                status="OUT",
                purpose="Home",
            )
        else:
            self.home_manager.reject(request)
            return ScanOutcome(
                success=True,
                student_name=request.name,
                roll_number=request.roll_number,
                purpose="Home (Rejected)",
                error_message="Home leave denied. Transaction reverted.",
            )

    def end_session(self) -> list[curfew.CurfewViolation]:
        """
        Ends the day's session:
          1. Runs the curfew compliance check.
          2. Returns the list of violations.
          3. Marks the session as ended.
        """
        violations = curfew.run_curfew_check(self.today)
        self.state = SessionState.ENDED
        return violations

    def full_shutdown(self):
        """Shuts down the C++ engine."""
        ew.shutdown()
        self.state = SessionState.NOT_STARTED
```

---

### 5.7 `python_app/main.py` — Console Integration Test

A **terminal-based test harness** (no GUI) that proves the entire Python → C++ pipeline works end-to-end. This replaces `gate_cli` with the Python-driven version.

```python
"""
This is a consol based main.py

This will be replaced by the GUI in Phase 6.
"""
from python_app.logic.session import SessionManager


def main():
    sm = SessionManager()
    success, msg = sm.start_session("db_root")
    print(msg)

    # Interactive loop (mirrors gate_cli but in Python)
    while True:
        print("\n--- Campus Gate System (Python) ---")
        print("1. Enroll Student")
        print("2. Simulate Gate Scan")
        print("3. View Today's Log")
        print("4. View Gone Home List")
        print("5. Curfew Check")
        print("6. Batch Promotion")
        print("7. Exit")

        choice = input("Select: ").strip()
        # ... dispatch to engine_wrapper and session_manager ...

    sm.full_shutdown()


if __name__ == "__main__":
    main()
```

---

#### Integration Test (Console)

Run the `main.py` test harness:

```bash
cd project_root
python -m python_app.main
```

Perform the same test sequence as the [Phase 1-3 verification](file:///Users/devanshkhosla/Projects/Test%20folder/Phase%201-3.md):

1. Enroll Devansh (Hosteller) and Bacchi (Day Scholar) through the Python CLI.
2. Scan Devansh → status OUT (count 1), purpose: Market.
3. Scan Devansh → status IN (count 2), no purpose needed.
4. Scan Bacchi → status IN (count 1), purpose: Class.
5. Scan Devansh → purpose: Home → approve → added to Home DB.
6. Run curfew check → only Bacchi flagged (Devansh has approved leave).
7. Scan Devansh returning → removed from Home DB, status IN.
8. Run curfew check → all compliant.

---

## Security Roadmap -- Fingerprint Template Encryption

> [!IMPORTANT]
> **Must be implemented before production deployment.**

Currently, all fingerprint templates are stored in plaintext on disk (both `.dat` and `.fpt` files). This is acceptable for development but is a security risk in production.

**Why not hashing?** Hashing is one-way -- it destroys the original data. The matching engine requires the raw 512-byte template for byte-by-byte similarity comparison. Hashing would make fuzzy matching impossible since even one byte difference produces a completely different hash.

**Planned approach: AES-256 encryption at rest.**

| Step | What Happens |
| ---- | ------------- |
| Enrollment | Encrypt the 512-byte template with AES-256 before writing to `.dat` / `.fpt` files |
| Gate scan | Read encrypted template from disk, decrypt in memory, run `compare_templates()` |
| After match | Zero-wipe the decrypted buffer from memory immediately |
| Key storage | macOS Keychain, env variable, or hardware security module |

**New files needed:**

- `cpp_engine/src/crypto.cpp` + `cpp_engine/include/crypto.h` -- AES-256 encrypt/decrypt functions
- Modifications to `serializer.cpp`, `fingerprint.cpp`, and `engine.h`

**Note**: The FNV-1a hash in `master_index.dat` is NOT a security measure. It is a non-cryptographic 4-byte index used for fast candidate lookup. It is a table of contents, not a lock.

See [GateScanModifications.md Section 7](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/GateScanModifications.md) for full details.

---
