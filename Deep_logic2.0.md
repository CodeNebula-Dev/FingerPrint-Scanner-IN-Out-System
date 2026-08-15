# Campus Biometric Entry Management System — Technical Design

## Core Problem Being Solved

A college gate has two kinds of people moving through it — students who live inside the campus (hostelers) and students who only come for classes (day scholars). The gate needs to know, at any moment, who is where. The system must derive this automatically from scan events alone, without any manual toggle or button press.

## Database Architecture

### Why Two Separate Databases 

The master student data and the daily movement logs have fundamentally different lifecycles. Student records change once a year. Gate logs change hundreds of times a day. Mixing them into one database would mean running heavy write operations against a table that also needs to serve fast identity lookups on every scan. Keeping them separate means each database can be optimised independently — the master database stays largely read-only, and the log database is optimised for rapid sequential writes.

### Master Database

Stores everything that defines a student's identity in the system. The fingerprint template is the primary lookup key — every other field exists to be retrieved once a match is confirmed. The `residency_type` field (hosteler or day scholar) is stored here because it is a stable property of the student and is needed on every single gate event to determine status direction. Fetching it from the master at the time of each scan avoids storing it redundantly in every log row.

The database is updated in bulk at the start of each academic year when a new batch enrolls. Individual record corrections can be made through a restricted admin operation.

### Daily Log Database

The purpose of this database is to represent the state of the gate for a single day. Each student gets one row per day. That row accumulates every scan they make throughout the day as a sequence of timestamps. The row does not distinguish between entry and exit explicitly — that distinction is derived mathematically from the position of the scan in the sequence (explained below).

## Identity Resolution at the Gate

When a student places their finger on the scanner, the biometric engine runs a one-to-many comparison — the live scan against every enrolled template in the master database. This produces a match score. If the score crosses the acceptance threshold, the system retrieves that student's record. If no template scores above the threshold, the scan is treated as unrecognised.

An unrecognised scan must never silently fail. It is written to a separate rejection log with the timestamp and terminal ID so the admin can investigate — whether it is a damaged finger, a worn sensor, or an unenrolled person attempting entry.

## The Parity Model for Status Derivation

This is the core logic of the system. Rather than asking the student to press "IN" or "OUT", the system counts how many times they have scanned today and uses that count to determine their current location.

The reasoning is based on a simple physical truth: a person starts in one place and alternates every time they cross the gate. The starting position is different depending on residency type.

A hosteler lives inside the campus. Their default position at the start of the day is INSIDE. Their first scan means they are leaving — so they become OUTSIDE. Their second scan means they returned — so they become INSIDE. Every odd-numbered scan places them OUTSIDE. Every even-numbered scan places them INSIDE.

A day scholar lives outside the campus. Their default position at the start of the day is OUTSIDE. Their first scan means they arrived — so they become INSIDE. Their second scan means they left — so they become OUTSIDE. Every odd-numbered scan places them INSIDE. Every even-numbered scan places them OUTSIDE.

scan_count = total timestamps recorded for this student today

HOSTELER  →  odd count  = OUTSIDE  |  even count = INSIDE
DAY SCHOLAR →  odd count  = INSIDE   |  even count = OUTSIDE

This eliminates any need for direction input at the terminal. The count alone is sufficient.


## Purpose Selection and What It Represents

After identity is resolved and before the timestamp is written, the student selects a purpose. This is not just a label — it affects downstream processing in one specific case.

For purposes like MARKET, EXAM, CLASS, or any non-HOME reason, the timestamp is written immediately and the status flag is updated via the parity model.

For HOME, the system cannot proceed automatically. Going home implies an extended, potentially overnight absence. This requires the admin to be informed and to explicitly authorise it. Until that authorisation arrives, no timestamp is written, no status is updated, and the student is held at the terminal in a pending state. If the admin approves, the timestamp is committed and the student's status updates to OUTSIDE. If the admin rejects, the entire event is discarded and the student remains INSIDE.

## Curfew Monitoring

At the curfew time, the system does not create any new data — it queries what already exists in the day's log. The query isolates two groups:

- Hostelers whose current scan count is odd (they are outside and have not returned)
- Day scholars whose current scan count is odd (they are inside and have not left)

Both groups are anomalous at curfew time. The admin sees these lists on a live dashboard. As students scan in, their count increments, their status flips, and they drop off the list in real time.

## Archival and Storage Management

The daily log database will accumulate data continuously. Leaving it to grow indefinitely creates two problems: query performance degrades over time, and storage costs increase without bound.

The solution is periodic export to Excel followed by cleanup of the source data. Excel is chosen because the records, once a day is over, are historical and read-only — they do not need to be queried relationally. A spreadsheet is sufficient for audits, attendance reviews, and administrative reporting.

Two export approaches are viable:

The first exports each day's table at the end of that day, immediately freeing the space. This keeps the log database consistently small but produces many individual files.

The second lets the log database accumulate for the full academic year, then exports all of it at year-end into a single workbook with one sheet per day. This allows in-database queries across the year (useful for patterns, attendance analytics) at the cost of carrying the full year's data in live storage until export.

Which approach is chosen depends on the institute's storage constraints and whether cross-day queries are a priority.

## Sequence of Events for a Single Gate Interaction

---

Student scans finger
        │
        ▼
Biometric engine runs 1:N match against master DB
        │
        ├── No match above threshold
        │         └── Write to rejection log → deny gate → alert admin
        │
        └── Match found
                  │
                  ▼
          Has this student scanned today?
                  │
                  ├── NO → Create new row in today's log table
                  │
                  └── YES → Locate existing row
                  │
                  ▼
          Student selects purpose
                  │
                  ├── HOME → Send approval request to admin
                  │              │
                  │         Approved → write timestamp, update status
                  │         Rejected → discard event, no change
                  │
                  └── Any other purpose
                              │
                              ▼
                    Write timestamp to next available slot
                              │
                              ▼
                    Recompute status via parity + residency type
                              │
                              ▼
                    Update status field in row
```