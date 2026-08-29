# ONLY TO TEST IF PYTHON CAN CALL ALL THE C++ FUNCTIONS

'''import fingerprint_engine as fe

fe.engine_init("./db_root")

student = fe.StudentRecord()
student.roll_number = input("Enter Roll Number: ")
student.name = input("Enter Full Name: ")
student.program = input("Enter Program (BSc/MSc/etc): ")
student.batch = input("Enter Batch (e.g. 2026): ")
student.year = int(input("Enter Academic Year (e.g. 1,2 etc.): "))
student.is_hosteller = input("Is Hosteller? (y/n): ").lower() == 'y'

fe.student_add(student)
print(f"Student {student.name} successfully registered in C++ database!")'''

'''import fingerprint_engine as fe
from datetime import datetime

# Initialize C++ engine
fe.engine_init("./db_root")

def enroll_student_interactive():
    print("\n--- STUDENT ENROLLMENT ---")
    student = fe.StudentRecord()
    student.roll_number = input("Enter Roll Number (e.g. 26CSE001): ").strip()
    student.name = input("Enter Full Name: ").strip()
    student.program = input("Enter Program (BSc/MSc): ").strip()
    student.batch = input("Enter Batch (e.g. 2026): ").strip()
    student.year = int(input("Enter Academic Year (1-3): "))
    student.phone_number = input("Enter Phone Number: ").strip()
    student.is_hosteller = input("Is Hosteller? (y/n): ").strip().lower() == 'y'

    # Save student profile
    fe.student_add(student)

    # CAPTURE FINGERPRINT
    print("\n>>> [ACTION] Place finger on the Biometric Scanner... <<<")
    fe.windows_biometric_authenticate("Enroll Student Fingerprint")
    success, raw_template = fe.windows_capture_template(512)
    
    if fe.fingerprint_enroll(student.roll_number, raw_template):
        print(f"Student {student.name} enrolled & fingerprint encrypted on disk!\n")
    else:
        print("Failed to enroll fingerprint.\n")

def select_purpose(status: str) -> str:
    """Displays a numbered menu for selecting the gate crossing reason."""
    if status == "IN":
        return "Campus Return / Arrival"

    # Standard OUT options per system specification
    print("\n  Select Purpose for Exiting:")
    print("    [1] Market ")
    print("    [2] Medical")
    print("    [3] Exam ")
    print("    [4] Class")
    print("    [5] HOME")
    print("    [6] Other (Type reason)")

    choice = input("  Enter option (1-6) [Default: 1]: ").strip()

    options = {
        "1": "Market",
        "2": "Medical",
        "3": "Exam",
        "4": "Class",
        "5": "HOME",
    }

    if choice == "6":
        custom = input("  Please enter custom reason: ").strip()
        return custom if custom else "Other"
    
    return options.get(choice, "Market")


def scan_gate_interactive():
    print("\n--- BIOMETRIC GATE SCANNER ACTIVE ---")
    print(">>> [ACTION] Place finger on the scanner... <<<")

    fe.windows_biometric_authenticate("Gate Access Authentication")
    success, live_scan = fe.windows_capture_template(512)

    result = fe.fingerprint_match(live_scan)
    if not result.matched:
        print("ACCESS DENIED: Fingerprint not recognized!")
        today = datetime.now().strftime("%d_%m_%Y")
        fe.rejection_log_write(today, live_scan)
        return

    print(f"\nIDENTIFIED: {result.name} ({result.roll_number}) [{'Hosteller' if result.is_hosteller else 'Day Scholar'}]")

    # 1. Check if returning from approved Home Leave
    if fe.home_exists(result.roll_number):
        print("Student returning from approved HOME leave. Clearing home permit.")
        fe.home_remove(result.roll_number)

    # 2. Determine State (IN vs OUT)
    today = datetime.now().strftime("%d_%m_%Y")
    ok, entry = fe.log_get_entry(today, result.roll_number)

    if not ok:
        entry = fe.LogEntry()
        entry.roll_number = result.roll_number
        entry.name = result.name
        entry.year = result.year
        entry.gate_count = 1
        entry.status = "OUT" if result.is_hosteller else "IN"
        
        # PROMPT PURPOSE VIA NUMBERED SELECTION
        reason = select_purpose(entry.status)
        entry.reason = reason
        
        # If student selected HOME, add to Home Database
        if reason == "HOME":
            home_rec = fe.HomeRecord()
            home_rec.roll_number = result.roll_number
            home_rec.name = result.name
            home_rec.year = result.year
            home_rec.phone_number = result.phone_number
            home_rec.date_of_leaving = datetime.now().strftime("%d-%m-%Y")
            home_rec.time_of_leaving = datetime.now().strftime("%H:%M:%S")
            fe.home_add(home_rec)
            print(f"[HOME LEAVE ACTIVATED] {result.name} logged into Approved Home Registry.")

        fe.log_add_entry(today, entry)
    else:
        entry.gate_count += 1
        if result.is_hosteller:
            entry.status = "OUT" if (entry.gate_count % 2 != 0) else "IN"
        else:
            entry.status = "IN" if (entry.gate_count % 2 != 0) else "OUT"
            
        reason = select_purpose(entry.status)
        entry.reason = reason
        fe.log_update_entry(today, result.roll_number, entry)

    print(f"GATE RECORDED: {result.name} is marked {entry.status} ({entry.reason}) [Total scans today: {entry.gate_count}]\n")


def wipe_database_interactive():
    print("\n WARNING: This will permanently DELETE all student data, logs, and home permits!")
    confirm = input("Are you sure you want to NUKE the entire database? (type 'yes' to confirm): ").strip()
    if confirm.lower() == 'yes':
        if fe.engine_wipe_all_data():
            print("[NUKE SUCCESS] All database files, student profiles, and logs have been wiped clean!\n")
        else:
            print("Failed to wipe database.\n")
    else:
        print("Wipe operation cancelled.\n")


# Interactive Menu Loop
while True:
    print("========================================")
    print("   CAMPUS GATE BIOMETRIC CONTROLLER     ")
    print("========================================")
    print("1. Enroll New Student (Profile + Finger)")
    print("2. Gate Scan (Touch Fingerprint Scanner)")
    print("3. View All Registered Students")
    print("4. View Today's Gate Log")
    print("5. [DEV ONLY] Wipe Entire Database")
    print("6. Exit")
    
    choice = input("\nSelect option (1-6): ").strip()
    if choice == '1':
        enroll_student_interactive()
    elif choice == '2':
        scan_gate_interactive()
    elif choice == '3':
        students = fe.student_list_all()
        print(f"\nTotal Students: {len(students)}")
        for s in students:
            print(f"  • {s.roll_number} | {s.name} | Batch: {s.batch} | {'Hosteller' if s.is_hosteller else 'Day Scholar'}")
        print()
    elif choice == '4':
        today = datetime.now().strftime("%d_%m_%Y")
        logs = fe.log_get_all_entries(today)
        print(f"\nGate Logs for {today}:")
        for l in logs:
            print(f"  • {l.roll_number} | {l.name} | Scans: {l.gate_count} | Status: {l.status} | Reason: {l.reason}")
        print()
    elif choice == '5':
        wipe_database_interactive()
    elif choice == '6':
        fe.engine_shutdown()
        print("Engine shutdown. Goodbye!")
        break
    else:
        print("Invalid choice, try again.")'''