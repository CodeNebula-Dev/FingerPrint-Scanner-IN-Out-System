#include "engine.h"
#include "indexer.h"
#include "crypto_placeholder.h"
#include "touch_id.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// CLI Color codes for a premium feel
#define RESET "\033[0m"
#define BOLD "\033[1m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"

// Global helpers for time
std::string get_current_date_string() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  char buf[12];
  std::strftime(buf, sizeof(buf), "%d_%m_%Y", std::localtime(&t));
  return std::string(buf);
}

std::string get_current_time_string() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  char buf[10];
  std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
  return std::string(buf);
}

std::string get_current_timestamp_string() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  char buf[25];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
  return std::string(buf);
}

bool is_after_curfew() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm *local = std::localtime(&t);
  int hour = local->tm_hour;
  int min = local->tm_min;
  if (hour > 18)
    return true;
  if (hour == 18 && min >= 30)
    return true;
  return false;
}

#include <random>

// Generate realistic deterministic 512-byte biometric feature vector based on roll number
void generate_mock_template(const std::string &roll, uint8_t *template_out) {
  std::hash<std::string> hasher;
  size_t seed = hasher(roll);
  std::mt19937 rng(static_cast<unsigned int>(seed));
  std::uniform_int_distribution<int> dist(0, 255);
  for (size_t i = 0; i < 512; ++i) {
    template_out[i] = static_cast<uint8_t>(dist(rng));
  }
}

// Visual layouts
void print_header(const std::string &title) {
  std::cout << BOLD << CYAN
            << "\n============================================================="
               "=========\n";
  std::cout << "  " << title << "\n";
  std::cout << "==============================================================="
               "======="
            << RESET << "\n";
}

void print_success(const std::string &msg) {
  std::cout << BOLD << GREEN << "  [SUCCESS] " << msg << RESET << "\n";
}

void print_warning(const std::string &msg) {
  std::cout << BOLD << YELLOW << "  [WARNING] " << msg << RESET << "\n";
}

void print_error(const std::string &msg) {
  std::cout << BOLD << RED << "  [ERROR] " << msg << RESET << "\n";
}

// Option Handlers
void do_enroll() {
  print_header("STUDENT BIOMETRIC ENROLLMENT");

  StudentRecord student;
  std::memset(&student, 0, sizeof(StudentRecord));

  std::cout << "  Enter Roll Number (e.g. 26CSE001): ";
  std::string roll;
  std::cin >> roll;
  std::strncpy(student.roll_number, roll.c_str(),
               sizeof(student.roll_number) - 1);

  // Check if roll number already exists
  StudentRecord existing;
  if (student_get(roll.c_str(), existing)) {
    print_error("A student with roll number '" + roll + "' already exists!");
    return;
  }

  std::cin.ignore(); // Clear newline
  std::cout << "  Enter Full Name: ";
  std::string name;
  std::getline(std::cin, name);
  std::strncpy(student.name, name.c_str(), sizeof(student.name) - 1);

  std::cout << "  Enter Program (e.g. BSc / MSc / PhD): ";
  std::string program;
  std::getline(std::cin, program);
  std::strncpy(student.program, program.c_str(), sizeof(student.program) - 1);

  std::cout << "  Enter Admission Batch Year (e.g. 2026): ";
  std::string batch;
  std::cin >> batch;
  std::strncpy(student.batch, batch.c_str(), sizeof(student.batch) - 1);

  std::cout << "  Enter Academic Year (1, 2, 3, etc.): ";
  std::cin >> student.year;

  std::cout << "  Enter Phone Number: ";
  std::string phone;
  std::cin >> phone;
  std::strncpy(student.phone_number, phone.c_str(),
               sizeof(student.phone_number) - 1);

  std::cout << "  Is Hosteller? (1 = Yes, 0 = No / Day Scholar): ";
  int hosteller;
  std::cin >> hosteller;
  student.is_hosteller = (hosteller == 1);

  std::cout << BOLD << YELLOW
            << "\n  >>> [ACTION REQUIRED] Please touch the MacBook Touch ID "
               "scanner to enroll... <<<\n"
            << RESET;
  std::string prompt_str = "Enroll fingerprint for student " + name;
  bool touch_ok = macos_touch_id_authenticate(prompt_str.c_str());

  if (!touch_ok) {
    print_warning("Touch ID not detected / cancelled. Falling back to simulation mode...");
  } else {
    print_success("MacBook Touch ID success!");
  }

  student_add(student);

  // Generate & BioHash template to disk
  uint8_t raw_template[512];
  generate_mock_template(roll, raw_template);

  if (fingerprint_enroll(student.roll_number, raw_template, 512)) {
    print_success("Student enrolled & BioHashed successfully on disk!");
  } else {
    print_error("Failed to enroll student biometric template.");
  }
}

void do_scan() {
  print_header("BIOMETRIC GATE SCANNER ACTIVE");

  std::vector<StudentRecord> students = student_list_all();
  if (students.empty()) {
    print_warning("No students currently enrolled in master database. Please "
                  "enroll students first!");
    return;
  }

  std::cout << BOLD << YELLOW
            << "\n  >>> [ACTION REQUIRED] Please touch the MacBook Touch ID "
               "scanner... <<<\n"
            << RESET;

  std::string prompt_str = "Authorize gate scan";
  bool touch_ok = macos_touch_id_authenticate(prompt_str.c_str());

  if (!touch_ok) {
    print_warning("Touch ID not detected / cancelled. Proceeding in simulation mode...");
  } else {
    print_success("MacBook Touch ID success!");
  }

  std::cout << BOLD << BLUE
            << "\n  [SIMULATION MODE] Simulating fingerprint scan...\n"
            << RESET;
  std::cout << "  Enter Roll Number to simulate scan (e.g. 26CSE001 or 'random' for imposter): ";
  std::string sim_roll;
  std::cin >> sim_roll;

  uint8_t live_scan[512];
  generate_mock_template(sim_roll, live_scan);

  std::cout << "  Processing BioHash matching in RAM...\n";

  MatchResult match = fingerprint_match(live_scan, 512);

  if (!match.matched) {
    print_error("Fingerprint match rejected by database engine!");
    std::cout << "  Student is not enrolled or fingerprint not recognized.\n";
    rejection_log_write(get_current_date_string().c_str(), live_scan, 512);
    return;
  }

  std::cout << BOLD << GREEN << "  [IDENTIFIED] " << match.name << " ("
            << match.roll_number << ") ["
            << (match.is_hosteller ? "Hosteller" : "Day Scholar")
            << "] (Confidence: " << std::fixed << std::setprecision(1) << (match.confidence_score * 100.0f) << "%)\n"
            << RESET;

  std::string today = get_current_date_string();
  std::string now_time = get_current_timestamp_string();

  // 1. Check Home leave database first
  if (home_exists(match.roll_number)) {
    std::cout
        << BOLD << GREEN
        << "  [RETURNING FROM HOME] Student detected on approved home leave.\n"
        << RESET;
    home_remove(match.roll_number);

    LogEntry entry;
    std::memset(&entry, 0, sizeof(LogEntry));
    std::strncpy(entry.roll_number, match.roll_number, sizeof(entry.roll_number) - 1);
    std::strncpy(entry.name, match.name, sizeof(entry.name) - 1);
    entry.year = match.year;
    std::strncpy(entry.reason, "Home Return", sizeof(entry.reason) - 1);
    entry.gate_count = 1;
    entry.late_return = false;
    std::strncpy(entry.timestamps[0], now_time.c_str(), sizeof(entry.timestamps[0]) - 1);
    entry.timestamp_count = 1;

    log_add_entry(today.c_str(), entry);
    print_success("Removed from gone-home registry. Gate status: IN");
    return;
  }

  // 2. Load daily log entry
  LogEntry log_entry;
  bool has_scanned_today = log_get_entry(today.c_str(), match.roll_number, log_entry);

  if (!has_scanned_today) {
    std::memset(&log_entry, 0, sizeof(LogEntry));
    std::strncpy(log_entry.roll_number, match.roll_number, sizeof(log_entry.roll_number) - 1);
    std::strncpy(log_entry.name, match.name, sizeof(log_entry.name) - 1);
    log_entry.year = match.year;
    log_entry.gate_count = 1;
    log_entry.timestamp_count = 1;
    std::strncpy(log_entry.timestamps[0], now_time.c_str(), sizeof(log_entry.timestamps[0]) - 1);
  } else {
    log_entry.gate_count++;
    if (log_entry.timestamp_count < MAX_TIMESTAMPS) {
      std::strncpy(log_entry.timestamps[log_entry.timestamp_count],
                   now_time.c_str(),
                   sizeof(log_entry.timestamps[log_entry.timestamp_count]) - 1);
      log_entry.timestamp_count++;
    }
  }

  // 3. Direction inference (Parity FSM Logic)
  bool is_in = false;
  if (match.is_hosteller) {
    is_in = (log_entry.gate_count % 2 == 0);
  } else {
    is_in = (log_entry.gate_count % 2 != 0);
  }

  std::string status_str = is_in ? "IN" : "OUT";
  std::strncpy(log_entry.status, status_str.c_str(), sizeof(log_entry.status) - 1);

  // Curfew check
  if (is_after_curfew() && is_in && match.is_hosteller) {
    log_entry.late_return = true;
    std::cout << BOLD << RED
              << "  [CURFEW WARNING] Late return detected! (Curfew is 18:30)\n"
              << RESET;
  }

  std::string purpose = "Regular";
  bool going_out = !is_in;

  if ((match.is_hosteller && going_out) || (!match.is_hosteller && !going_out)) {
    std::cout << "\n  Select purpose of gate crossing:\n";
    if (match.is_hosteller) {
      std::cout << "  [1] Market\n  [2] Medical\n  [3] Exam\n  [4] Home\n  [5] Others\n";
      std::cout << "  Enter choice: ";
      int choice;
      std::cin >> choice;
      if (choice == 1) purpose = "Market";
      else if (choice == 2) purpose = "Medical";
      else if (choice == 3) purpose = "Exam";
      else if (choice == 4) purpose = "Home";
      else {
        std::cout << "  Enter custom reason: ";
        std::cin.ignore();
        std::getline(std::cin, purpose);
      }
    } else {
      std::cout << "  [1] Classes\n  [2] Lab\n  [3] Library\n  [4] Others\n";
      std::cout << "  Enter choice: ";
      int choice;
      std::cin >> choice;
      if (choice == 1) purpose = "Classes";
      else if (choice == 2) purpose = "Lab";
      else if (choice == 3) purpose = "Library";
      else {
        std::cout << "  Enter custom reason: ";
        std::cin.ignore();
        std::getline(std::cin, purpose);
      }
    }
  }

  std::strncpy(log_entry.reason, purpose.c_str(), sizeof(log_entry.reason) - 1);

  // Handle Home leave workflow
  if (purpose == "Home" && match.is_hosteller && going_out) {
    std::cout << BOLD << YELLOW << "  [APPROVAL QUEUED] Home leave request submitted to admin.\n" << RESET;
    std::cout << "  Approve Home leave request? (y/n): ";
    char approval;
    std::cin >> approval;
    if (approval == 'y' || approval == 'Y') {
      print_success("Home leave approved!");
      HomeRecord hr;
      std::memset(&hr, 0, sizeof(HomeRecord));
      std::strncpy(hr.roll_number, match.roll_number, sizeof(hr.roll_number) - 1);
      std::strncpy(hr.name, match.name, sizeof(hr.name) - 1);
      hr.year = match.year;
      std::strncpy(hr.phone_number, match.phone_number, sizeof(hr.phone_number) - 1);
      std::string date_str = get_current_date_string();
      std::string time_str = get_current_time_string();
      std::strncpy(hr.date_of_leaving, date_str.c_str(), sizeof(hr.date_of_leaving) - 1);
      std::strncpy(hr.time_of_leaving, time_str.c_str(), sizeof(hr.time_of_leaving) - 1);
      home_add(hr);
    } else {
      print_warning("Home leave denied by administrator.");
      return;
    }
  }

  // Save to daily logs
  bool write_ok = false;
  if (!has_scanned_today) {
    write_ok = log_add_entry(today.c_str(), log_entry);
  } else {
    write_ok = log_update_entry(today.c_str(), match.roll_number, log_entry);
  }

  if (write_ok) {
    std::cout << BOLD << GREEN << "  [LOGGED] " << match.name << " is marked "
              << status_str << " (Gate count: " << log_entry.gate_count
              << ", Purpose: " << purpose << ")\n"
              << RESET;
  } else {
    print_error("Failed to write daily log.");
  }
}

void do_view_logs() {
  print_header("DAILY LOG GATE RECORDS");
  std::string today = get_current_date_string();
  std::vector<LogEntry> entries = log_get_all_entries(today.c_str());

  if (entries.empty()) {
    print_warning("No gate scans recorded today.");
    return;
  }

  std::cout << std::left << std::setw(12) << "Roll No" << std::setw(20)
            << "Name" << std::setw(8) << "Year" << std::setw(15) << "Purpose"
            << std::setw(8) << "Count" << std::setw(8) << "Status"
            << std::setw(8) << "Late?"
            << "Last Scan Time\n";
  std::cout << std::string(85, '=') << "\n";

  for (const auto &entry : entries) {
    std::string last_time =
        (entry.timestamp_count > 0)
            ? entry.timestamps[entry.timestamp_count - 1]
            : "N/A";
    std::cout << std::left << std::setw(12) << entry.roll_number
              << std::setw(20) << entry.name << std::setw(8) << entry.year
              << std::setw(15) << entry.reason << std::setw(8)
              << entry.gate_count << std::setw(8) << entry.status
              << std::setw(8) << (entry.late_return ? "YES" : "NO")
              << last_time << "\n";
  }
}

void do_view_home() {
  print_header("GONE HOME REGISTRY (ACTIVE LEAVES)");
  std::vector<HomeRecord> records = home_get_all();

  if (records.empty()) {
    print_warning("No students are currently on home leave.");
    return;
  }

  std::cout << std::left << std::setw(12) << "Roll No" << std::setw(20)
            << "Name" << std::setw(8) << "Year" << std::setw(15) << "Phone"
            << std::setw(15) << "Leaving Date"
            << "Leaving Time\n";
  std::cout << std::string(80, '=') << "\n";

  for (const auto &rec : records) {
    std::cout << std::left << std::setw(12) << rec.roll_number << std::setw(20)
              << rec.name << std::setw(8) << rec.year << std::setw(15)
              << rec.phone_number << std::setw(15) << rec.date_of_leaving
              << rec.time_of_leaving << "\n";
  }
}

void do_curfew_check() {
  print_header("CURFEW COMPLIANCE & HOSTEL AUDIT");

  std::string today = get_current_date_string();
  std::vector<LogEntry> logs = log_get_all_entries(today.c_str());
  std::vector<StudentRecord> all_students = student_list_all();

  std::cout << BOLD << "  Curfew Status at 18:30 (Hostellers must be IN):\n"
            << RESET;
  std::cout << std::string(80, '-') << "\n";

  int late_count = 0;
  int outside_count = 0;

  for (const auto &student : all_students) {
    if (!student.is_hosteller)
      continue;

    if (home_exists(student.roll_number))
      continue;

    LogEntry entry;
    bool has_entry = log_get_entry(today.c_str(), student.roll_number, entry);

    if (has_entry && entry.late_return) {
      late_count++;
      std::cout << BOLD << RED << "  [LATE RETURN]  " << student.roll_number
                << " - " << student.name << " (Returned late at "
                << entry.timestamps[entry.timestamp_count - 1] << ")\n"
                << RESET;
    } else if (has_entry && std::string(entry.status) == "OUT") {
      outside_count++;
      std::cout << BOLD << RED << "  [STILL OUT]    " << student.roll_number
                << " - " << student.name
                << " (Currently OUTSIDE campus after curfew!)\n"
                << RESET;
    }
  }

  if (late_count == 0 && outside_count == 0) {
    print_success("All hostellers are safe inside the hostel. No curfew violations!");
  } else {
    std::cout << BOLD << YELLOW << "\n  Summary: " << outside_count
              << " student(s) still outside, " << late_count
              << " student(s) logged late returns.\n"
              << RESET;
  }
}

void do_batch_promote() {
  print_header("BATCH PROMOTION UTILITIES");
  std::cout << "  [1] Promote Specific Batch\n";
  std::cout << "  [2] Promote ALL Batches\n";
  std::cout << "  [3] Delete Graduated Batch\n";
  std::cout << "  Enter choice: ";
  int ch;
  std::cin >> ch;

  if (ch == 1) {
    std::cout << "  Enter Batch Year to promote (e.g. 2026): ";
    std::string b;
    std::cin >> b;
    int count = batch_promote(b.c_str());
    print_success("Promoted " + std::to_string(count) + " students in batch " + b);
  } else if (ch == 2) {
    int count = batch_promote_all();
    print_success("Promoted all " + std::to_string(count) + " students in database!");
  } else if (ch == 3) {
    std::cout << "  Enter Graduated Batch to remove (e.g. 2022): ";
    std::string b;
    std::cin >> b;
    if (batch_delete(b.c_str())) {
      print_success("Batch " + b + " deleted successfully.");
    } else {
      print_error("Failed to delete batch " + b);
    }
  }
}

void do_view_database() {
  print_header("STUDENT MASTER DATABASE");
  std::vector<StudentRecord> students = student_list_all();

  if (students.empty()) {
    print_warning("No students found in master database.");
    return;
  }

  std::cout << std::left << std::setw(15) << "Roll No" << std::setw(20)
            << "Name" << std::setw(10) << "Program" << std::setw(8) << "Batch"
            << std::setw(6) << "Year" << std::setw(12) << "Type"
            << std::setw(15) << "Phone"
            << "BioHash Index (FNV-1a)\n";
  std::cout << std::string(105, '=') << "\n";

  for (const auto &student : students) {
    std::string role_str = student.is_hosteller ? "Hosteller" : "Day Scholar";
    uint64_t hash = indexer_hash_template(student.encrypted_template, 512);

    std::cout << std::left << std::setw(15) << student.roll_number
              << std::setw(20) << student.name << std::setw(10)
              << student.program << std::setw(8) << student.batch
              << std::setw(6) << student.year << std::setw(12) << role_str
              << std::setw(15) << student.phone_number << "0x" << std::hex
              << std::setw(16) << std::setfill('0') << hash << std::dec
              << std::setfill(' ') << "\n";
  }
}

void do_nuke_database() {
  print_header("[DEV ONLY] DATABASE WIPE");
  std::cout << BOLD << RED
            << "\n  !!! DANGER ZONE !!!\n"
            << "  This will permanently delete ALL data:\n"
            << "    - All enrolled students & BioHash keys\n"
            << "    - All daily logs and gone-home records\n"
            << "  Are you sure you want to proceed? (type 'YES' to confirm): "
            << RESET;
  std::string confirm;
  std::cin >> confirm;

  if (confirm == "YES") {
    if (engine_wipe_all_data()) {
      print_success("Database completely wiped. Clean state initialized.");
    } else {
      print_error("Failed to wipe database.");
    }
  } else {
    print_warning("Database wipe aborted.");
  }
}

int main() {
  std::cout << BOLD << CYAN
            << "\n╔════════════════════════════════════════════════════════════════╗"
            << "\n║   CAMPUS BIOMETRIC GATE ENTRY MANAGEMENT SYSTEM (ENGINE v2.0)  ║"
            << "\n║        Cancelleable Biometrics • BioHashing • ISO/IEC 24745    ║"
            << "\n╚════════════════════════════════════════════════════════════════╝"
            << RESET << "\n";

  if (!engine_init(".")) {
    print_error("Failed to initialize database engine directory structure!");
    return 1;
  }
  print_success("Engine v2.0 initialized successfully.");

  while (true) {
    std::cout << "\n"
              << BOLD << CYAN
              << "======================================================================\n"
              << "             CAMPUS BIOMETRIC GATE SYSTEM - CONSOLE CONTROLLER\n"
              << "======================================================================\n"
              << RESET
              << "  Current Date: " << get_current_date_string()
              << " | Current Time: " << get_current_time_string() << "\n\n"
              << "  1. Enroll Student (Master DB)\n"
              << "  2. Gate Scan (Touch ID Sensor)\n"
              << "  3. View Daily Logs\n"
              << "  4. View Gone Home Registry\n"
              << "  5. Curfew Compliance Check\n"
              << "  6. Batch Promotion Utilities\n"
              << "  7. View Student Master Database\n"
              << "  8. Shutdown & Exit\n"
              << "  9. [DEV ONLY] Wipe Entire Database\n\n"
              << BOLD << "  Please select an option: " << RESET;

    int choice;
    if (!(std::cin >> choice)) {
      std::cin.clear();
      std::string discard;
      std::cin >> discard;
      continue;
    }

    switch (choice) {
    case 1:
      do_enroll();
      break;
    case 2:
      do_scan();
      break;
    case 3:
      do_view_logs();
      break;
    case 4:
      do_view_home();
      break;
    case 5:
      do_curfew_check();
      break;
    case 6:
      do_batch_promote();
      break;
    case 7:
      do_view_database();
      break;
    case 8:
      print_success("Shutting down engine safely...");
      engine_shutdown();
      std::cout << "\nGoodbye!\n\n";
      return 0;
    case 9:
      do_nuke_database();
      break;
    default:
      print_warning("Invalid option. Please choose 1-9.");
      break;
    }
  }

  return 0;
}
