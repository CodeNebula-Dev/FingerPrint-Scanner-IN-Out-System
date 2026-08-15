# Fingerprint Entry System — Logic & Flow

A digitised gate-entry system for colleges that replaces manual registers with fingerprint scanning, automatic status tracking, and daily Excel exports.

---

## Overview

| Component | Description |
|-----------|-------------|
| Main database | Permanent student records (name, roll no, fingerprint, type, contact) |
| Daily log database | Temporary per-day entry tables, one table per date |
| Excel archive | Daily logs exported at end of day to free up database space |

---

## Step 1 — Fingerprint Scan

Every gate crossing begins here, whether it is morning entry, an exit during the day, or a late return after session close.

1. Student places finger on scanner.
2. System queries the **main database** for a matching fingerprint.
3. **If no match is found** → entry is flagged as unknown, admin is alerted (fail-safe), and the flow stops.
4. **If a match is found** → student record is fetched (name, roll no, student type, current count).

---

## Step 2 — Log Entry & Status Update

### 2a. Increment count

Every scan increments the student's crossing count for the day in the **daily log table**.  
A new table is created automatically on the first scan of each day, named by date (e.g. `2025-07-14`).

### 2b. Determine IN / OUT status

Status is calculated from the **parity of the crossing count**, and the rule differs by student type:

| Student type | Even count | Odd count |
|--------------|------------|-----------|
| **Hostelite** (lives on campus) | IN | OUT |
| **Day scholar** (commutes daily) | OUT | IN |

> **Why the rules are opposite:** A hostelite starts the day inside, so their first scan (count = 1, odd) takes them OUT. A day scholar starts the day outside, so their first scan (count = 1, odd) brings them IN.

Status is written to the daily log immediately after the count is updated.

---

## Step 3 — Purpose Selection

After status is updated, the student selects the reason for crossing the gate:

- **Market** — going to the market
- **Exam** — attending an external exam
- **Class** — attending an off-campus class
- **Home** — going home (requires admin approval)
- *(additional purposes can be configured by admin)*

### 3a. Purpose is Market / Exam / Class (or any non-Home purpose)

- Timestamp of the entry is saved alongside the purpose in the daily log.
- Flow ends here; record is complete.

### 3b. Purpose is Home

- Request is queued for admin review.
- Student's record is **held** until the admin acts.

**If admin approves:**
- Student is flagged as `HOME_APPROVED` in the daily log.
- Status is set to OUT.
- Timestamp is saved.

**If admin denies:**
- Flag is not set.
- Student's status reverts to the previous value (remains IN).
- Student must re-select a purpose or return inside.

---

## Step 4 — Late Returns (After Session Close)

If a student scans their fingerprint **after the session close time (default 18:30)**:

1. System detects the scan is post-session.
2. Student is **removed from the still-outside list**.
3. A late-entry timestamp is logged against their record.
4. No purpose selection is required for late returns.

---

## Step 5 — End-of-Day Session

Admin triggers end-of-day at **18:30** (configurable).

### 5a. Build still-outside list

System scans the daily log and collects every student whose final crossing count is **odd** (meaning they are currently outside by their type's rule).

### 5b. Remove home-approved students

Students with the `HOME_APPROVED` flag are excluded from the still-outside list — their absence is accounted for.

### 5c. Generate outside report

The remaining students on the list are flagged as **unaccounted for**.  
This report is presented to the admin before shutdown.

### 5d. Admin review & shutdown

Admin reviews the report, takes any necessary action, then shuts down the session.  
The daily log table is **locked** — no further edits are possible after shutdown.

---

## Step 6 — Export & Archive

After session shutdown:

1. The locked daily log is **exported to Excel** (`.xlsx`), one file per day.
2. The exported file is saved to the archive folder.
3. The daily log entry in the database is cleared to free storage.
4. The system resets and is **ready for the next day**.

---

## Data Models (Summary)

### Main database — `students` table

| Field | Type | Description |
|-------|------|-------------|
| `roll_no` | string (PK) | Unique student identifier |
| `name` | string | Full name |
| `fingerprint_hash` | binary | Fingerprint template |
| `student_type` | enum | `HOSTELITE` or `DAY_SCHOLAR` |
| `contact` | string | Phone / emergency contact |

### Daily log — `log_YYYY-MM-DD` table

| Field | Type | Description |
|-------|------|-------------|
| `roll_no` | string (FK) | Links to main database |
| `name` | string | Denormalised for quick access |
| `crossing_count` | integer | Incremented on each scan |
| `status` | enum | `IN` or `OUT` |
| `purpose` | string | Selected purpose for last exit |
| `last_scan_time` | datetime | Timestamp of most recent scan |
| `home_approved` | boolean | Set by admin for home exits |
| `late_return` | boolean | True if scanned after 18:30 |

---

## Edge Cases & Rules

| Situation | Behaviour |
|-----------|-----------|
| Unrecognised fingerprint | Logged as unknown, admin alerted, gate action blocked |
| Admin denies home request | Status reverts to IN, student must re-select purpose |
| Student returns after 18:30 | Removed from outside list, late return logged |
| Student never returns (odd count at EOD, no approval) | Appears on unaccounted-for report |
| New batch joins | Main database updated once a year by admin |

---

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| Session close time | `18:30` | Time at which EOD is triggered |
| Purposes list | Market, Exam, Class, Home | Configurable by admin |
| Archive folder path | `/exports/` | Where Excel files are saved |

---

*System designed for college campus gate management. All daily logs are archived in Excel for record-keeping and audit purposes.*