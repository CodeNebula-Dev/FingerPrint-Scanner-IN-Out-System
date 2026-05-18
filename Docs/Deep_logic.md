# Fingerprint Attendance System — How It Works

This document explains what the program does, step by step, in plain language.

---

## What does this system do?

When a student puts their finger on the scanner at the gate, the program:
- Figures out **who they are**
- Records whether they are **going out or coming in**
- Asks **why they are going out**
- Saves everything to a **daily file**

The admin can view, correct, and manage all records from a separate admin panel.

---

## The 6 Phases

---

### Phase 1 — Registering Students (Done Once)

> Before the system can work, every student's details must be saved.

**Steps:**
1. Admin opens the student registration screen.
2. Fills in the student's details — name, roll number, year, phone number.
3. The student places their finger on the scanner to save their fingerprint.
4. The program saves this as a file, for example `ROLL001.csv`.
5. The student's filename is added to a batch list, for example `2025_batch.txt`.

**Result:**
- One file per student inside the `student_data/` folder.
- One batch list file that points to all student files of that year.

**Watch out for:**
- If a roll number already exists → the program will reject it, not overwrite.
- If the fingerprint scan fails → the program retries 3 times, then stops.

---

### Phase 2 — Starting the Program Each Day

> Every morning, the admin starts the program. The program decides what to do based on the date.

**Steps:**
1. Admin starts the program.
2. The program reads today's date from the computer clock (e.g. `01_01_2026`).
3. It checks: does a file for today already exist?

   - **Yes →** it asks the admin:
     - *"Continue from where you left off?"* → program opens the existing file and continues.
     - *"Stop?"* → program closes and goes back to the admin panel.
   - **No →** it creates a new file for today, inside the correct month and year folder automatically.

4. The file is now locked — no one can edit it manually while the program is running.
5. The gate scan loop begins (Phase 3).

**Result:**
- A daily record file is ready, e.g. `everyday_data/2026/January/01_01_2026.csv`.

**Watch out for:**
- If the computer clock is wrong → admin must fix it before starting.
- Missing folders → the program creates them automatically.

---

### Phase 3 — The Gate Scan Loop

> This is the main part that runs all day. Every time a student scans their finger, this runs.

**Steps:**
1. Program waits for a finger to be placed on the scanner.
2. It reads the fingerprint and searches for a match in the student files.
   - **No match →** shows "Unrecognised fingerprint", waits for next scan.
   - **Match found →** gets the student's name, roll number, and year.
3. It checks how many times the student has crossed the gate today (the **gate count**).

   - **Count is even (0, 2, 4…) → student is coming IN:**
     - Records the current time as their entry time.
     - Adds 1 to their gate count.
     - No reason needed (they are just returning).

   - **Count is odd (1, 3, 5…) → student is going OUT:**
     - Records the current time as their exit time.
     - Adds 1 to their gate count.
     - Asks the student to pick a reason (see below).

4. **Reason for going out:**

   | Choice | What happens |
   |--------|-------------|
   | Market | Saved directly, shown in the list. |
   | Medical | Saved directly, shown in the list. |
   | Exam | Saved directly, shown in the list. |
   | Home | Needs admin approval first → goes to Phase 4. |
   | Others | Student types their own reason on the keyboard. |

5. The student's row is saved to today's file and shown on screen:
   ```
   Name | Roll | Year | Entry Time | Exit Time | Reason
   ```
6. Program goes back to step 1 and waits for the next student.

**This loop keeps running until:** 18:30 is reached, or the admin ends the session.

**Watch out for:**
- If a student scans twice by accident within 5 seconds → the second scan is ignored.
- If the scanner has a hardware problem → an error is shown, but the program keeps running.

---

### Phase 4 — Home Leave Approval

> When a student wants to go home, the admin must approve it first.

**Steps:**
1. Student selects "Home" at the gate.
2. A request appears on the admin panel showing the student's name, roll, and year.
3. The admin makes a decision:

   - **Approved:**
     - The student is recorded as gone home.
     - They will NOT appear in the "still outside" list at 18:30.

   - **Denied:**
     - The screen shows: *"Leave form not submitted"*.
     - The student's gate count is reversed (as if they never scanned).
     - The student stays inside.

4. The gate goes back to waiting for the next scan.

**Watch out for:**
- If the admin does not respond → the student waits. The request stays open.

---

### Phase 5 — End of Day

> At 18:30 (or when the admin ends the session), the program checks who is still outside.

**Steps:**
1. At 18:30 the program triggers automatically, or the admin clicks "End session".
2. It looks through today's file for students whose gate count is **odd** (still outside).
3. It removes students who went home with approved leave — they are expected to be out.
4. The remaining students are shown in the admin panel as a **"still outside" list**:
   ```
   Name | Roll | Year | Exit Time | Reason | Phone Number
   ```
   The phone number is pulled from the student's profile file.
5. As late students return and scan their finger:
   - They are removed from the list automatically.
   - Their original exit time is kept as-is (admin can change it manually if needed).
6. Admin shuts down the program when done.
7. All files are saved and unlocked.

**Watch out for:**
- If a student never returns → their record stays odd; admin fixes it manually in Phase 6.

---

### Phase 6 — Admin Panel (When Program Is Off)

> When the gate program is not running, the admin can view and fix everything.

---

#### Add a new student
1. Fill in the student's details.
2. Scan their fingerprint.
3. Their file is created and added to the batch list.

---

#### Delete a student
1. Select the student by roll number.
2. Confirm the deletion.
3. Their file and their entry in the batch list are both removed.

---

#### Edit a student's details
1. Select the student.
2. Change any detail — name, year, phone, or even fingerprint.
3. Save.

---

#### Edit an attendance record
1. Go to the date you want to fix (e.g. `01_01_2026.csv`).
2. Change any field — entry time, exit time, reason, etc.
3. Save.

---

#### Promote a whole batch (move everyone up one year)
1. Select the batch (e.g. `2025_batch.txt`).
2. The program adds 1 to the year of every student in that batch.
3. The batch file can be renamed to `2026_batch.txt`.

> The confirmation dialog prevents this from being run twice by accident.

---

## How Files Are Organised

```
project_root/
│
├── student_data/
│   ├── 2025_batch.txt       ← list of all student files for this batch
│   ├── ROLL001.csv          ← one file per student
│   ├── ROLL002.csv
│   └── ...
│
└── everyday_data/
    └── 2026/
        └── January/
            ├── 01_01_2026.csv   ← one file per day
            ├── 02_01_2026.csv
            └── ...
```

**Each daily file stores:**
```
Roll | Name | Year | Entry Time | Exit Time | Reason | Gate Count
```

**Each student file stores:**
```
Roll Number | Name | Year | Phone | Fingerprint
```

---

## The Gate Count — Simple Explanation

Every student starts the day with a count of **0** (inside).

| Count | Means | Next scan will… |
|-------|-------|-----------------|
| 0 | Inside | Record going OUT, count → 1 |
| 1 | Outside | Record coming IN, count → 2 |
| 2 | Inside | Record going OUT, count → 3 |
| 3 | Outside | Record coming IN, count → 4 |

**Even = inside. Odd = outside.** That's the whole rule.

---

## Libraries Used — Plain English

| Library | What it does in this project |
|---------|------------------------------|
| `<cstdint>` | Lets us store fingerprint data in the right format (uint8_t) |
| `<chrono>` | Gets the current time from the computer clock |
| `<ctime>` | Turns that time into a readable format like "08:45:00" |
| `<filesystem>` | Creates folders, checks if files exist, moves files |
| `<fstream>` | Reads from and writes to `.csv` and `.txt` files |
| `<bits/stdc++.h>` | A shortcut header that includes most standard tools at once |