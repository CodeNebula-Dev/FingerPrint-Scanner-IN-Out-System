# Fingerprint Entry System — Flowchart

> College gate management system that replaces manual registers with fingerprint scanning, automatic status tracking, and daily Excel exports.

---

## Full System Flow

```
                        ┌─────────────────────────┐
                        │   🖐 FINGERPRINT SCANNED  │
                        │   Student touches scanner │
                        └────────────┬────────────┘
                                     │
                                     ▼
                        ┌─────────────────────────┐
                        │  Match in Main Database? │
                        │  name · roll no · type   │
                        └────────────┬────────────┘
                                     │
               ┌─────────────────────┴──────────────────────┐
             NO│                                          YES│
               ▼                                            ▼
  ┌────────────────────────┐              ┌─────────────────────────┐
  │  ⚠  FAIL SAFE          │              │  ✓ Record Fetched        │
  │  Flag unknown entry    │              │  name · roll no · count  │
  │  Alert admin           │              └──────────────┬──────────┘
  │  Block gate — STOP     │                             │
  └────────────────────────┘                             ▼
                                          ┌─────────────────────────┐
                                          │  Log Entry in Daily Table│
                                          │  Increment crossing count│
                                          │  (new table if day 1)    │
                                          └──────────────┬──────────┘
```

---

## Step 2 — Status Calculation

```
                                          ┌──────────────┴──────────┐
                                          │     Student Type?        │
                                          └──────────────┬──────────┘
                                                         │
                            ┌────────────────────────────┴──────────────────────────┐
                       HOSTELITE                                               DAY SCHOLAR
                            │                                                        │
                            ▼                                                        ▼
             ┌──────────────────────────┐                          ┌──────────────────────────┐
             │  Hostel Parity Rule      │                          │  Day Scholar Parity Rule  │
             │  (starts day INSIDE)     │                          │  (starts day OUTSIDE)     │
             │                          │                          │                           │
             │  Even count  →  IN       │                          │  Odd  count  →  IN        │
             │  Odd  count  →  OUT      │                          │  Even count  →  OUT       │
             └──────────────┬───────────┘                          └─────────────┬─────────────┘
                            │                                                    │
                            └─────────────────────┬──────────────────────────────┘
                                                  │
                                                  ▼
                                   ┌──────────────────────────┐
                                   │  Status Written to Log   │
                                   │      IN  or  OUT         │
                                   └──────────────┬───────────┘
```

---

## Step 3 — Purpose Selection

```
                                                  │
                                                  ▼
                                   ┌──────────────────────────┐
                                   │    Select Exit Purpose   │
                                   │  Market · Exam · Class   │
                                   │         · Home           │
                                   └──────────────┬───────────┘
                                                  │
                        ┌─────────────────────────┴────────────────────────┐
                  OTHER │                                              HOME │
          (Market/Exam/Class)                                               │
                        │                                                   ▼
                        ▼                                    ┌──────────────────────────┐
          ┌──────────────────────────┐                       │  🔔 Awaits Admin Approval │
          │  ⏱ Timestamp Saved       │                       │     Request queued        │
          │  Purpose + time logged   │                       └──────────────┬───────────┘
          │  Record complete ✓       │                                      │
          └──────────────┬───────────┘                        ┌─────────────┴─────────────┐
                         │                              APPROVE│                       DENY│
                         │                                     ▼                          ▼
                         │                   ┌─────────────────────────┐  ┌──────────────────────────┐
                         │                   │  ✅ HOME_APPROVED flag   │  │  ❌ Flag not set          │
                         │                   │  Status  →  OUT          │  │  Status reverts to IN    │
                         │                   │  Timestamp saved         │  │  Student stays inside    │
                         │                   └──────────────┬──────────┘  └──────────────┬───────────┘
                         │                                  │                             │
                         └──────────────────────────────────┴─────────────────────────────┘
                                                            │
                                                            ▼
                                             ┌──────────────────────────┐
                                             │  📝 Daily Record Updated  │
                                             │  count · status · purpose │
                                             │  timestamp — all saved    │
                                             └──────────────┬───────────┘
```

---

## Step 4 — Late Returns

```
                                                            │
                                                            ▼
                                             ┌──────────────────────────┐
                                             │  Scan after 18:30?       │
                                             └──────────────┬───────────┘
                                                            │
                                       ┌────────────────────┴──────────────────┐
                                   YES │                                    NO  │
                                       ▼                                        ▼
                        ┌──────────────────────────┐          ┌──────────────────────────┐
                        │  🌙 Late Return Flow      │          │  ✓ Normal — on time      │
                        │  1. Remove from outside  │          │  Awaits end-of-day check │
                        │     list                  │          └──────────────┬───────────┘
                        │  2. Log late timestamp   │                         │
                        │  3. No purpose needed    │                         │
                        └──────────────┬───────────┘                         │
                                       └─────────────────────┬────────────────┘
                                                             │
```

---

## Step 5 — End-of-Day Session

```
                                                             │
                                                             ▼
                                              ┌──────────────────────────┐
                                              │  ⏰ Admin Ends Session    │
                                              │     Default: 18:30       │
                                              └──────────────┬───────────┘
                                                             │
                                                             ▼
                                              ┌──────────────────────────┐
                                              │  5a. Build Outside List  │
                                              │  All students with odd   │
                                              │  final crossing count    │
                                              └──────────────┬───────────┘
                                                             │
                                                             ▼
                                              ┌──────────────────────────┐
                                              │  5b. Remove Approved     │
                                              │  Drop HOME_APPROVED      │
                                              │  students from list      │
                                              └──────────────┬───────────┘
                                                             │
                                                             ▼
                                              ┌──────────────────────────┐
                                              │  5c. Unaccounted Report  │
                                              │  Remaining = missing     │
                                              │  Shown to admin          │
                                              └──────────────┬───────────┘
                                                             │
                                                             ▼
                                              ┌──────────────────────────┐
                                              │  5d. Admin Shuts Down    │
                                              │  Reviews report          │
                                              │  Locks day's data        │
                                              └──────────────┬───────────┘
```

---

## Step 6 — Export & Archive

```
                                                             │
                                                             ▼
                                              ┌──────────────────────────┐
                                              │  📊 Export → Excel       │
                                              │  One .xlsx file per day  │
                                              │  Saved to archive folder │
                                              │  DB entry cleared        │
                                              └──────────────┬───────────┘
                                                             │
                                                             ▼
                                              ┌──────────────────────────┐
                                              │  ✨ Ready for Next Day   │
                                              │  New table on first scan │
                                              └──────────────────────────┘
```

---

## Parity Rule Reference

| Student Type  | Count | Status |
|---------------|-------|--------|
| Hostelite     | Even  | IN     |
| Hostelite     | Odd   | OUT    |
| Day Scholar   | Odd   | IN     |
| Day Scholar   | Even  | OUT    |

> **Why opposite rules?** A hostelite starts the day *inside*, so their first scan (count = 1, odd) takes them OUT.
> A day scholar starts the day *outside*, so their first scan (count = 1, odd) brings them IN.

---

## Edge Cases

| Situation | Behaviour |
|-----------|-----------|
| Unrecognised fingerprint | Flagged unknown · admin alerted · gate blocked |
| Admin denies home request | Flag not set · status reverts to IN |
| Student returns after 18:30 | Removed from outside list · late return logged |
| Student never returns (odd count, no approval) | Appears on unaccounted report |
| New batch joins | Main database updated once a year by admin |

---

## Data Models

### `students` — Main Database

| Field | Type | Description |
|-------|------|-------------|
| `roll_no` | string (PK) | Unique student ID |
| `name` | string | Full name |
| `fingerprint_hash` | binary | Fingerprint template |
| `student_type` | enum | `HOSTELITE` or `DAY_SCHOLAR` |
| `contact` | string | Phone / emergency contact |

### `log_YYYY-MM-DD` — Daily Log

| Field | Type | Description |
|-------|------|-------------|
| `roll_no` | string (FK) | Links to main database |
| `name` | string | Denormalised for quick access |
| `crossing_count` | integer | Incremented on each scan |
| `status` | enum | `IN` or `OUT` |
| `purpose` | string | Reason for last exit |
| `last_scan_time` | datetime | Timestamp of most recent scan |
| `home_approved` | boolean | Set by admin for home exits |
| `late_return` | boolean | True if scanned after 18:30 |

---

*System designed for college campus gate management. All daily logs are archived in Excel for record-keeping and audit purposes.*