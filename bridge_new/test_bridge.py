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

''' import fingerprint_engine as fe
from datetime import datetime

# Initialize C++ engine
fe.engine_init("./db_root")

def enroll_student_interactive():
    print("\n--- STUDENT ENROLLMENT ---")
    student = fe.StudentRecord()
    student.roll_number = input("Enter Roll Number (e.g. 2025B001): ").strip()
    student.name = input("Enter Full Name: ").strip()
    student.program = input("Enter Program (e.g. BSc/MSc): ").strip()
    student.batch = input("Enter Batch (e.g. 2026): ").strip()
    student.year = int(input("Enter Academic Year (e.g. 1,2,3): "))
    student.phone_number = input("Enter Phone Number: ").strip()
    student.is_hosteller = input("Is Hosteller? (y/n): ").strip().lower() == 'y'

    # Save student profile
    fe.student_add(student)

    # CAPTURE FINGERPRINT
    print("\n>>> [ACTION] Place finger on the Biometric Scanner... <<<")
    
    # 1. Trigger hardware scanner (or Windows Hello / Simulation)
    fe.windows_biometric_authenticate("Enroll Student Fingerprint")
    
    # 2. Capture raw 512-byte template
    success, raw_template = fe.windows_capture_template(512)
    
    # 3. BioHash & Enroll into database
    if fe.fingerprint_enroll(student.roll_number, raw_template):
        print(f"Student {student.name} enrolled & fingerprint encrypted on disk!\n")
    else:
        print(" Failed to enroll fingerprint.\n")


def scan_gate_interactive():
    print("\n--- BIOMETRIC GATE SCANNER ACTIVE ---")
    print(">>> [ACTION] Place finger on the scanner to enter/exit... <<<")

    # 1. Capture live scan
    fe.windows_biometric_authenticate("Gate Access Authentication")
    success, live_scan = fe.windows_capture_template(512)

    # 2. Match in C++ database
    result = fe.fingerprint_match(live_scan)

    if not result.matched:
        print("ACCESS DENIED: Fingerprint not recognized!")
        today = datetime.now().strftime("%d_%m_%Y")
        fe.rejection_log_write(today, live_scan)
        return

    # 3. Identified Student!
    print(f"\n IDENTIFIED: {result.name} ({result.roll_number}) [{'Hosteller' if result.is_hosteller else 'Day Scholar'}]")

    # 4. Check Home Leave Database first
    if fe.home_exists(result.roll_number):
        print("Student returning from approved HOME leave. Clearing home permit.")
        fe.home_remove(result.roll_number)

    # 5. Compute IN / OUT Direction via Parity Model
    today = datetime.now().strftime("%d_%m_%Y")
    ok, entry = fe.log_get_entry(today, result.roll_number)

    if not ok:
        # First scan of the day
        entry = fe.LogEntry()
        entry.roll_number = result.roll_number
        entry.name = result.name
        entry.year = result.year
        entry.gate_count = 1
        # Hosteller starts inside (1st scan = OUT), Day Scholar starts outside (1st scan = IN)
        entry.status = "OUT" if result.is_hosteller else "IN"
        entry.reason = input(f"Enter Reason for {entry.status} (e.g. Class / Market / Home): ").strip()
        fe.log_add_entry(today, entry)
    else:
        # Subsequent scan today
        entry.gate_count += 1
        if result.is_hosteller:
            entry.status = "OUT" if (entry.gate_count % 2 != 0) else "IN"
        else:
            entry.status = "IN" if (entry.gate_count % 2 != 0) else "OUT"
        entry.reason = input(f"Enter Reason for {entry.status}: ").strip()
        fe.log_update_entry(today, result.roll_number, entry)

    print(f" GATE RECORDED: {result.name} is marked {entry.status} (Total scans today: {entry.gate_count})\n")


# Interactive Menu Loop
while True:
    print("========================================")
    print("   CAMPUS GATE BIOMETRIC CONTROLLER     ")
    print("========================================")
    print("1. Enroll New Student (Profile + Finger)")
    print("2. Gate Scan (Touch Fingerprint Scanner)")
    print("3. View All Registered Students")
    print("4. View Today's Gate Log")
    print("5. Exit")
    
    choice = input("\nSelect option (1-5): ").strip()
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
        fe.engine_shutdown()
        print("Engine shutdown. Goodbye!")
        break
    else:
        print("Invalid choice, try again.") '''