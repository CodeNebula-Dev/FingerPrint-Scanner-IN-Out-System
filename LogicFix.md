# Walkthrough of Changes

We resolved the issues in the Home logic and daily log status tracking.

## Changes Made

### 1. [main.cpp](file:///Users/devanshkhosla/Projects/Test%20folder/cpp_engine/src/main.cpp)

#### Fix Returning From Home
- Updated the database lookup logic in `do_scan()` when `home_exists()` is true.
- Previously, it created a fresh entry and called `log_add_entry()` which triggered a `[DailyLog] Warning: Duplicate log entry today` error if the student left earlier that day.
- Now, it uses `log_get_entry()` to fetch their record first. If they already scanned today, it increments their `gate_count`, updates the reason to `Home Return`, changes the status to `IN`, adds the timestamp, and calls `log_update_entry()` instead. Otherwise, it adds a new entry.

#### New Approach for Status
- Replaced the simple odd/even parity count calculation for determining `is_in` with a clean transition toggle logic.
- Previously, returning from home leave could invert the parity sequence for the rest of the day, making scan OUTs register as INs.
- Now, if the student has scanned already today, their status simply toggles: `IN` becomes `OUT` and `OUT` becomes `IN`. Default state transitions (Hostellers default OUT, Day Scholars default IN) are only used on the first scan of the day.

#### Gone Home Highlight
- Highlighted students whose status is `OUT` and purpose/reason is `Home` inside the daily log viewer (`do_view_logs()`) using the console `YELLOW` color.

## Verification

We compiled the project successfully and ran a comprehensive standalone test suite in C++ to verify the state transitions:
1. Checked student enrollment.
2. Verified going home today correctly sets status `OUT`, reason `Home`, and adds to the home registry.
3. Verified returning from home leave updates the existing daily log entry without duplicate warnings, setting status to `IN`, reason to `Home Return`, and incrementing the gate count.
4. Verified that subsequent scans today (e.g. going to Market) correctly toggle status back to `OUT` and count to 3.
5. Verified that returning from the Market toggles the status back to `IN` and count to 4.
6. The test completed with all assertions passing.
