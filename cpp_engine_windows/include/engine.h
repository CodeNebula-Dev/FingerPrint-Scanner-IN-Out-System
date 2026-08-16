#ifndef ENGINE_V2_H
#define ENGINE_V2_H

#include <cstdint>
#include <vector>
#include <string>

// Global System Constants
const int MAX_TIMESTAMPS = 20;
const int TEMPLATE_SIZE = 512;

// Master Student Record (Engine v2.0 - Security Hardened)
struct StudentRecord {
    char roll_number[20];               // Unique student identifier (Primary Key)
    char name[100];                     // Full name of student
    char program[20];                   // Program ("BSc", "MSc", "PhD", etc.)
    char batch[10];                     // Batch year (e.g. "2026")
    int  year;                          // Academic year
    char phone_number[15];              // Contact details
    bool is_hosteller;                  // true = hosteller, false = day scholar
    uint8_t encrypted_template[TEMPLATE_SIZE]; // Transformed/encrypted biometric payload
};

// Daily Gate Activity Log Entry
struct LogEntry {
    char roll_number[20];
    char name[100];
    int  year;
    char reason[50];                    // Reason for exit/entry
    int  gate_count;                    // Scans today
    char status[10];                    // "IN" or "OUT"
    bool late_return;                   // Flagged if entry after curfew
    char timestamps[MAX_TIMESTAMPS][25]; // Format: "YYYY-MM-DD HH:MM:SS"
    int  timestamp_count;
};

// Approved Home Leave Entry
struct HomeRecord {
    char roll_number[20];
    char name[100];
    int  year;
    char phone_number[15];
    char date_of_leaving[12];           // "DD-MM-YYYY"
    char time_of_leaving[10];           // "HH:MM:SS"
};

// Biometric Verification Match Result
struct MatchResult {
    bool   matched;
    char   roll_number[20];
    char   name[100];
    char   program[20];
    char   batch[10];
    int    year;
    char   phone_number[15];
    bool   is_hosteller;
    float  confidence_score;
    int    match_count;                 // Count of matching records above threshold
};

// Engine Lifecycle API
bool engine_init(const char* project_root_path);
void engine_shutdown();
bool engine_wipe_all_data();

// Student Profile CRUD (Master DB)
bool student_add(const StudentRecord& record);
bool student_remove(const char* roll_number);
bool student_update(const char* roll_number, const StudentRecord& updated_record);
bool student_get(const char* roll_number, StudentRecord& record);
std::vector<StudentRecord> student_list_by_batch(const char* batch);
std::vector<StudentRecord> student_list_all();
int  batch_promote(const char* batch);
int  batch_promote_all();
bool batch_delete(const char* batch);

// Biometric API (Engine v2.0 Pluggable Architecture)
MatchResult fingerprint_match(const uint8_t* live_scan, int scan_length);
bool fingerprint_enroll(const char* roll_number, const uint8_t* raw_template, int length);

// Daily Logs Management API
bool log_create_day(const char* date_string);
bool log_day_exists(const char* date_string);
bool log_add_entry(const char* date_string, const LogEntry& entry);
bool log_update_entry(const char* date_string, const char* roll_number, const LogEntry& updated_entry);
bool log_get_entry(const char* date_string, const char* roll_number, LogEntry& entry);
std::vector<LogEntry> log_get_all_entries(const char* date_string);
std::vector<LogEntry> log_get_entries_in_range(const char* start_date, const char* end_date);
bool log_delete_day(const char* date_string);

// Home Leaves Registry API
bool home_add(const HomeRecord& record);
bool home_remove(const char* roll_number);
bool home_exists(const char* roll_number);
std::vector<HomeRecord> home_get_all();

// Rejections Logging API
bool rejection_log_write(const char* date_string, const uint8_t* failed_scan, int scan_length);

#endif // ENGINE_V2_H
