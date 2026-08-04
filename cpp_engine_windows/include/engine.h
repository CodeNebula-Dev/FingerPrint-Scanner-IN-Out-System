#ifndef ENGINE_H
#define ENGINE_H

#include <cstdint>
#include <vector>
#include <string>

// Maximum limits for lists
const int MAX_TIMESTAMPS = 20;

// Student Record (Master Database)
struct StudentRecord
{
    char roll_number[20];              // Unique student identifier (Primary Key)
    char name[100];                    // Full name of the student
    char program[20];                  // "BSc", "MSc", "PhD", etc.
    char batch[10];                    // "2025", "2024", etc.
    int year;                          // Current academic year (1, 2, 3...)
    char phone_number[15];             // Contact number (stored as string to avoid overflow & keep leading 0s/+)
    bool is_hosteller;                 // true = hosteler, false = day scholar
    uint8_t fingerprint_template[512]; // Biometric template
};

// Daily Log Entry (Everyday Database)
struct LogEntry
{
    char roll_number[20];                // Unique student identifier
    char name[100];                      // Full name (fetched from master for de-normalization)
    int year;                            // Academic year when scanned
    char reason[50];                     // Purpose ("Market", "Medical", "Exam", "Home", etc.)
    int gate_count;                      // Incremented on each scan today
    char status[10];                     // "IN" or "OUT"
    bool late_return;                    // true if scanned in after curfew (18:30)
    char timestamps[MAX_TIMESTAMPS][25]; // Timestamps today, "YYYY-MM-DD HH:MM:SS"
    int timestamp_count;                 // Number of timestamps recorded today
};

// Home Record (Home Database)
struct HomeRecord
{
    char roll_number[20];
    char name[100];
    int year;
    char phone_number[15];    // Contact number
    char date_of_leaving[12]; // "DD-MM-YYYY"
    char time_of_leaving[10]; // "HH:MM:SS"
};

// Match Result
struct MatchResult
{
    bool matched; // true if match found above threshold
    char roll_number[20];
    char name[100];
    char program[20];
    char batch[10];
    int year;
    char phone_number[15];
    bool is_hosteller;
    float confidence_score; // Match score from 0.0 to 1.0
    int match_count;        // Number of students matching above threshold
};

// ==========================================
// C++ Engine API Declarations
// ==========================================

// Utility/Initialization Functions
bool engine_init(const char *project_root_path);
void engine_shutdown();
bool engine_wipe_all_data(); // [DEV ONLY] Deletes all data for a fresh start

// Master Database (Student_data) Operations
bool student_add(const StudentRecord &record);
bool student_remove(const char *roll_number);
bool student_update(const char *roll_number, const StudentRecord &updated_record);
bool student_get(const char *roll_number, StudentRecord &record);
std::vector<StudentRecord> student_list_by_batch(const char *batch);
std::vector<StudentRecord> student_list_all();
int batch_promote(const char *batch);
int batch_promote_all();
bool batch_delete(const char *batch);

// Fingerprint Operations
MatchResult fingerprint_match(const uint8_t *live_scan, int scan_length);
bool fingerprint_enroll(const char *roll_number, const uint8_t *template_data, int length);

// Daily Log Operations
bool log_create_day(const char *date_string);
bool log_day_exists(const char *date_string);
bool log_add_entry(const char *date_string, const LogEntry &entry);
bool log_update_entry(const char *date_string, const char *roll_number, const LogEntry &updated_entry);
bool log_get_entry(const char *date_string, const char *roll_number, LogEntry &entry);
std::vector<LogEntry> log_get_all_entries(const char *date_string);
std::vector<LogEntry> log_get_entries_in_range(const char *start_date, const char *end_date);
bool log_delete_day(const char *date_string);

// Home Database Operations
bool home_add(const HomeRecord &record);
bool home_remove(const char *roll_number);
bool home_exists(const char *roll_number);
std::vector<HomeRecord> home_get_all();

// Rejection Logging
bool rejection_log_write(const char *date_string, const uint8_t *failed_scan, int scan_length);

#endif // ENGINE_H
