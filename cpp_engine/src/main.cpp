#include "engine.h"
#include "indexer.h"
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

// Generate mock template data based on roll number
void generate_mock_template(const std::string &roll, uint8_t *template_out) {
  std::memset(template_out, 0, 512);
  // Simple reproducible mock template: fill with ascii values of the roll
  // number repeating
  for (size_t i = 0; i < 512; ++i) {
    template_out[i] = static_cast<uint8_t>(roll[i % roll.length()]);
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

// Interactive CLI actions
void do_enroll() {
  print_header("STUDENT BIOMETRIC ENROLLMENT");
  StudentRecord student;
  std::memset(&student, 0, sizeof(StudentRecord));

  std::cout << "  Enter Roll Number (e.g. 26CSE001): ";
  std::string roll;
  std::cin >> roll;
  std::strncpy(student.roll_number, roll.c_str(),
               sizeof(student.roll_number) - 1);

  std::cout << "  Enter Full Name: ";
  std::cin.ignore();
  std::string name;
  std::getline(std::cin, name);
  std::strncpy(student.name, name.c_str(), sizeof(student.name) - 1);

  std::cout << "  Enter Program (e.g. BSc / MSc / PhD): ";
  std::string prog;
  std::cin >> prog;
  std::strncpy(student.program, prog.c_str(), sizeof(student.program) - 1);

  std::cout << "  Enter Admission Batch Year (e.g. 2026): ";
  std::cin >> student.batch;

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
    print_error("MacBook Touch ID enrollment failed or was cancelled. "
                "Enrollment aborted.");
    return;
  }

  print_success("MacBook Touch ID success!");

  // Generate mock template
  generate_mock_template(roll, student.fingerprint_template);

  if (student_add(student)) {
    print_success("Student enrolled successfully on disk!");
  } else {
    print_error("Failed to enroll student.");
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

  std::cout << "  Available students to simulate:\n";
  for (size_t i = 0; i < students.size(); ++i) {
    std::cout << "  [" << i + 1 << "] " << students[i].name << " ("
              << students[i].roll_number << ") ["
              << (students[i].is_hosteller ? "Hosteller" : "Day Scholar")
              << "]\n";
  }
  std::cout << "  Select student index to simulate: ";
  size_t selection;
  std::cin >> selection;
  if (selection < 1 || selection > students.size()) {
    print_error("Invalid selection.");
    return;
  }

  const auto &selected_student = students[selection - 1];

  std::cout << BOLD << YELLOW
            << "\n  >>> [ACTION REQUIRED] Please touch the MacBook Touch ID "
               "scanner... <<<\n"
            << RESET;

  std::string prompt_str =
      "Authorize gate scan for student " + std::string(selected_student.name);
  bool touch_ok = macos_touch_id_authenticate(prompt_str.c_str());

  if (!touch_ok) {
    print_error("MacBook Touch ID authentication failed or was cancelled.");
    // Write to rejection log
    uint8_t failed_scan[512] = {0};
    rejection_log_write(get_current_date_string().c_str(), failed_scan, 512);
    return;
  }

  print_success("MacBook Touch ID success!");
  std::cout << "  Processing scan for student: " << selected_student.name
            << "...\n";

  // Simulate template scan
  uint8_t live_scan[512];
  generate_mock_template(selected_student.roll_number, live_scan);

  // Run database matching
  MatchResult match = fingerprint_match(live_scan, 512);
  if (!match.matched) {
    print_error("Fingerprint match rejected by database engine!");
    rejection_log_write(get_current_date_string().c_str(), live_scan, 512);
    return;
  }

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
    bool has_scanned = log_get_entry(today.c_str(), match.roll_number, entry);
    bool write_ok = false;

    if (!has_scanned) {
      std::memset(&entry, 0, sizeof(LogEntry));
      std::strncpy(entry.roll_number, match.roll_number,
                   sizeof(entry.roll_number) - 1);
      std::strncpy(entry.name, match.name, sizeof(entry.name) - 1);
      entry.year = match.year;
      std::strncpy(entry.reason, "Home Return", sizeof(entry.reason) - 1);
      entry.gate_count = 1;
      std::strncpy(entry.status, "IN", sizeof(entry.status) - 1);
      entry.late_return = false;
      std::strncpy(entry.timestamps[0], now_time.c_str(),
                   sizeof(entry.timestamps[0]) - 1);
      entry.timestamp_count = 1;

      write_ok = log_add_entry(today.c_str(), entry);
    } else {
      if (entry.timestamp_count >= MAX_TIMESTAMPS) {
        print_error("Daily scan count limit reached for this student.");
        return;
      }
      entry.gate_count++;
      std::strncpy(entry.reason, "Home Return", sizeof(entry.reason) - 1);
      std::strncpy(entry.status, "IN", sizeof(entry.status) - 1);
      std::strncpy(entry.timestamps[entry.timestamp_count], now_time.c_str(),
                   sizeof(entry.timestamps[entry.timestamp_count]) - 1);
      entry.timestamp_count++;

      write_ok = log_update_entry(today.c_str(), match.roll_number, entry);
    }

    if (write_ok) {
      print_success("Removed from gone-home registry. Gate status: IN");
    } else {
      print_error("Failed to update daily log.");
    }
    return;
  }

  // 2. Load daily log entry
  LogEntry log_entry;
  bool has_scanned_today =
      log_get_entry(today.c_str(), match.roll_number, log_entry);

  if (!has_scanned_today) {
    std::memset(&log_entry, 0, sizeof(LogEntry));
    std::strncpy(log_entry.roll_number, match.roll_number,
                 sizeof(log_entry.roll_number) - 1);
    std::strncpy(log_entry.name, match.name, sizeof(log_entry.name) - 1);
    log_entry.year = match.year;
    log_entry.gate_count = 1;
    log_entry.timestamp_count = 1;
    std::strncpy(log_entry.timestamps[0], now_time.c_str(),
                 sizeof(log_entry.timestamps[0]) - 1);
  } else {
    if (log_entry.timestamp_count >= MAX_TIMESTAMPS) {
      print_error("Daily scan count limit reached for this student.");
      return;
    }
    log_entry.gate_count++;
    std::strncpy(log_entry.timestamps[log_entry.timestamp_count],
                 now_time.c_str(),
                 sizeof(log_entry.timestamps[log_entry.timestamp_count]) - 1);
    log_entry.timestamp_count++;
  }

  // Compute status by toggling or using default
  bool is_in = false;
  if (!has_scanned_today) {
    if (match.is_hosteller) {
      is_in = false; // Hosteller default INSIDE, first scan goes OUT
    } else {
      is_in = true; // Day Scholar default OUTSIDE, first scan comes IN
    }
  } else {
    // Toggle the previous status
    is_in = (std::strcmp(log_entry.status, "IN") != 0);
  }

  std::string status_str = is_in ? "IN" : "OUT";
  std::strncpy(log_entry.status, status_str.c_str(),
               sizeof(log_entry.status) - 1);

  // Curfew check
  if (is_after_curfew() && is_in && match.is_hosteller) {
    log_entry.late_return = true;
    std::cout << BOLD << RED
              << "  [CURFEW WARNING] Late return detected! (Curfew is 18:30)\n"
              << RESET;
  }

  // Purpose selection flow
  std::string purpose = "Regular";
  bool going_out = !is_in;

  // For Hostellers going OUT or Day Scholars going IN
  if ((match.is_hosteller && going_out) ||
      (!match.is_hosteller && !going_out)) {
    std::cout << "\n  Select purpose of gate crossing:\n";
    if (match.is_hosteller) {
      std::cout << "  [1] Market\n  [2] Medical\n  [3] Exam\n  [4] Home\n  [5] "
                   "Others\n";
      std::cout << "  Enter choice: ";
      int choice;
      std::cin >> choice;
      if (choice == 1)
        purpose = "Market";
      else if (choice == 2)
        purpose = "Medical";
      else if (choice == 3)
        purpose = "Exam";
      else if (choice == 4)
        purpose = "Home";
      else {
        std::cout << "  Enter custom reason: ";
        std::cin.ignore();
        std::getline(std::cin, purpose);
      }
    } else {
      std::cout << "  [1] Class\n  [2] Others\n";
      std::cout << "  Enter choice: ";
      int choice;
      std::cin >> choice;
      if (choice == 1)
        purpose = "Class";
      else {
        std::cout << "  Enter custom reason: ";
        std::cin.ignore();
        std::getline(std::cin, purpose);
      }
    }
  } else {
    purpose = is_in ? "Entry" : "Exit";
  }

  std::strncpy(log_entry.reason, purpose.c_str(), sizeof(log_entry.reason) - 1);

  // Special HOME workflow
  if (purpose == "Home") {
    std::cout << BOLD << YELLOW
              << "  [APPROVAL QUEUED] Home leave request submitted to admin.\n"
              << RESET;
    std::cout << "  Approve Home leave request? (y/n): ";
    char approval;
    std::cin >> approval;
    if (approval == 'y' || approval == 'Y') {
      print_success("Home leave approved!");

      // Add student to gone home active database
      HomeRecord hr;
      std::memset(&hr, 0, sizeof(HomeRecord));
      std::strncpy(hr.roll_number, match.roll_number,
                   sizeof(hr.roll_number) - 1);
      std::strncpy(hr.name, match.name, sizeof(hr.name) - 1);
      hr.year = match.year;
      std::strncpy(hr.phone_number, match.phone_number,
                   sizeof(hr.phone_number) - 1);

      // Date and time of leaving
      std::string date_str = get_current_date_string();
      std::string time_str = get_current_time_string();
      std::strncpy(hr.date_of_leaving, date_str.c_str(),
                   sizeof(hr.date_of_leaving) - 1);
      std::strncpy(hr.time_of_leaving, time_str.c_str(),
                   sizeof(hr.time_of_leaving) - 1);

      home_add(hr);
    } else {
      print_warning("Home leave denied by administrator.");
      std::cout << "  The leave form is not submitted. Transaction reverted.\n";
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

  // Print Table Header
  std::cout << std::left << std::setw(12) << "Roll No" << std::setw(20)
            << "Name" << std::setw(8) << "Year" << std::setw(15) << "Purpose"
            << std::setw(8) << "Count" << std::setw(8) << "Status"
            << std::setw(8) << "Late?"
            << "Last Scan Time\n";
  std::cout << std::string(85, '=') << "\n";

  for (const auto &entry : entries) {
    std::string last_time = (entry.timestamp_count > 0)
                                ? entry.timestamps[entry.timestamp_count - 1]
                                : "N/A";
    bool highlight = (std::strcmp(entry.reason, "Home") == 0 &&
                      std::strcmp(entry.status, "OUT") == 0);
    if (highlight) {
      std::cout << GREEN;
    }
    std::cout << std::left << std::setw(12) << entry.roll_number
              << std::setw(20) << entry.name << std::setw(8) << entry.year
              << std::setw(15) << entry.reason << std::setw(8)
              << entry.gate_count << std::setw(8) << entry.status
              << std::setw(8) << (entry.late_return ? "YES" : "NO")
              << last_time;
    if (highlight) {
      std::cout << RESET;
    }
    std::cout << "\n";
  }
}

void do_curfew_check() {
  print_header("CURFEW COMPLIANCE REPORT (REAL-TIME)");
  std::string today = get_current_date_string();
  std::vector<LogEntry> entries = log_get_all_entries(today.c_str());
  std::vector<HomeRecord> gone_home = home_get_all();

  std::cout << "  Curfew threshold: 18:30\n";
  std::cout << "  Current active home-leaves: " << gone_home.size() << "\n\n";

  bool anomaly_found = false;

  // Print anomalous students
  std::cout << BOLD << RED << "  STUDENTS OUTSIDE COMPLIANCE:\n" << RESET;
  std::cout << std::left << std::setw(12) << "Roll No" << std::setw(20)
            << "Name" << std::setw(12) << "Role" << std::setw(10) << "Status"
            << std::setw(15) << "Phone Number\n";
  std::cout << std::string(70, '-') << "\n";

  for (const auto &log : entries) {
    // Fetch student master record to confirm type and phone number
    StudentRecord student;
    if (!student_get(log.roll_number, student))
      continue;

    bool is_anomaly = false;
    std::string role_str = student.is_hosteller ? "Hosteller" : "Day Scholar";

    if (student.is_hosteller && std::strcmp(log.status, "OUT") == 0) {
      // Hosteller is outside - check if approved for home leave
      bool on_home_leave = false;
      for (const auto &hr : gone_home) {
        if (std::strcmp(hr.roll_number, student.roll_number) == 0) {
          on_home_leave = true;
          break;
        }
      }
      if (!on_home_leave) {
        is_anomaly = true;
      }
    } else if (!student.is_hosteller && std::strcmp(log.status, "IN") == 0) {
      // Day scholar is still inside campus after curfew
      is_anomaly = true;
    }

    if (is_anomaly) {
      anomaly_found = true;
      std::cout << std::left << std::setw(12) << student.roll_number
                << std::setw(20) << student.name << std::setw(12) << role_str
                << std::setw(10) << log.status << student.phone_number << "\n";
    }
  }

  if (!anomaly_found) {
    print_success(
        "All scanned students are fully accounted for and compliant!");
  }
}

void do_home_list() {
  print_header("STUDENTS AWAY ON APPROVED HOME LEAVE");
  std::vector<HomeRecord> list = home_get_all();
  if (list.empty()) {
    print_warning("No students are currently away on home leave.");
    return;
  }

  std::cout << std::left << std::setw(12) << "Roll No" << std::setw(20)
            << "Name" << std::setw(8) << "Year" << std::setw(15) << "Date Left"
            << std::setw(15) << "Time Left"
            << "Contact Info\n";
  std::cout << std::string(80, '=') << "\n";

  for (const auto &r : list) {
    std::cout << std::left << std::setw(12) << r.roll_number << std::setw(20)
              << r.name << std::setw(8) << r.year << std::setw(15)
              << r.date_of_leaving << std::setw(15) << r.time_of_leaving
              << r.phone_number << "\n";
  }
}

void do_batch_promote() {
  print_header("BATCH PROMOTION PANEL");
  std::cout << "  [1] Promote a specific batch\n  [2] Promote ALL students\n  "
               "Select option: ";
  int choice;
  std::cin >> choice;

  if (choice == 1) {
    std::cout << "  Enter batch name to promote (e.g. 2026): ";
    std::string batch;
    std::cin >> batch;
    int promoted = batch_promote(batch.c_str());
    std::cout << BOLD << GREEN << "\n  Successfully promoted " << promoted
              << " students in batch " << batch << "!" << RESET << "\n";
  } else if (choice == 2) {
    int promoted = batch_promote_all();
    std::cout << BOLD << GREEN << "\n  Successfully promoted " << promoted
              << " students globally!" << RESET << "\n";
  } else {
    print_error("Invalid selection.");
  }
}

void do_view_master_db() {
  print_header("STUDENT MASTER DATABASE");
  std::vector<StudentRecord> students = student_list_all();

  if (students.empty()) {
    print_warning("No students currently enrolled in master database.");
    return;
  }

  std::cout << std::left << std::setw(12) << "Roll No" << std::setw(20)
            << "Name" << std::setw(10) << "Program" << std::setw(8) << "Batch"
            << std::setw(6) << "Year" << std::setw(12) << "Type"
            << std::setw(15) << "Phone"
            << "Fingerprint Hash (FNV-1a)\n";
  std::cout << std::string(105, '=') << "\n";

  for (const auto &student : students) {
    std::string role_str = student.is_hosteller ? "Hosteller" : "Day Scholar";
    uint32_t hash = compute_fnv1a_hash(student.fingerprint_template, 512);

    std::cout << std::left << std::setw(12) << student.roll_number
              << std::setw(20) << student.name << std::setw(10)
              << student.program << std::setw(8) << student.batch
              << std::setw(6) << student.year << std::setw(12) << role_str
              << std::setw(15) << student.phone_number << "0x" << std::hex
              << std::setw(8) << std::setfill('0') << hash << std::dec
              << std::setfill(' ') << "\n";
  }
}

int main() {
  // Determine target database path relative to workspace or absolute path
  // For local testing in the build workspace, we create a db_root directory
  std::string db_root = "db_root";

  if (!engine_init(db_root.c_str())) {
    std::cerr << "CRITICAL ERROR: Failed to initialize C++ Database Engine!"
              << std::endl;
    return 1;
  }

  while (true) {
    std::cout << BOLD << MAGENTA
              << "\n==========================================================="
                 "===========\n";
    std::cout
        << "             CAMPUS BIOMETRIC GATE SYSTEM - CONSOLE CONTROLLER\n";
    std::cout << "============================================================="
                 "========="
              << RESET << "\n";
    std::cout << "  Current Date: " << get_current_date_string()
              << " | Current Time: " << get_current_time_string() << "\n\n";
    std::cout << "  1. Enroll Student (Master DB)\n";
    std::cout << "  2. Gate Scan (Touch ID Sensor)\n";
    std::cout << "  3. View Daily Logs\n";
    std::cout << "  4. View Gone Home Registry\n";
    std::cout << "  5. Curfew compliance Check\n";
    std::cout << "  6. Batch Promotion Utilities\n";
    std::cout << "  7. View Student Master Database\n";
    std::cout << "  8. Shutdown & Exit\n\n";
    std::cout << "  Please select an option: ";

    int option;
    std::cin >> option;
    if (std::cin.fail()) {
      std::cin.clear();
      std::cin.ignore(10000, '\n');
      print_error("Invalid option entered. Please choose 1-8.");
      continue;
    }

    if (option == 1) {
      do_enroll();
    } else if (option == 2) {
      do_scan();
    } else if (option == 3) {
      do_view_logs();
    } else if (option == 4) {
      do_home_list();
    } else if (option == 5) {
      do_curfew_check();
    } else if (option == 6) {
      do_batch_promote();
    } else if (option == 7) {
      do_view_master_db();
    } else if (option == 8) {
      engine_shutdown();
      std::cout << "\n  Exiting. Goodbye!\n";
      break;
    } else {
      print_error("Invalid selection. Try again.");
    }
  }

  return 0;
}