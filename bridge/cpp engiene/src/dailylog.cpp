#include "dailylog.h"
#include "engine.h"
#include "serializer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <iomanip>

namespace fs = std::filesystem;

extern std::string g_project_root;

// Helper to get month directory name
static std::string get_month_folder_name(const std::string& mm) {
    if (mm == "01") return "01_January";
    if (mm == "02") return "02_February";
    if (mm == "03") return "03_March";
    if (mm == "04") return "04_April";
    if (mm == "05") return "05_May";
    if (mm == "06") return "06_June";
    if (mm == "07") return "07_July";
    if (mm == "08") return "08_August";
    if (mm == "09") return "09_September";
    if (mm == "10") return "10_October";
    if (mm == "11") return "11_November";
    if (mm == "12") return "12_December";
    return mm + "_Unknown";
}

// Parses DD_MM_YYYY or DD-MM-YYYY or DD/MM/YYYY
static bool parse_date_string(const std::string& date_string, std::string& day, std::string& month, std::string& year) {
    std::string normalized = date_string;
    for (char& c : normalized) {
        if (c == '-' || c == '/') c = '_';
    }
    
    std::stringstream ss(normalized);
    std::string d, m, y;
    if (std::getline(ss, d, '_') && std::getline(ss, m, '_') && std::getline(ss, y, '_')) {
        day = d;
        month = m;
        year = y;
        return d.size() == 2 && m.size() == 2 && y.size() == 4;
    }
    return false;
}

// Returns full path to the daily log binary file
static std::string get_daily_log_filepath(const std::string& date_string) {
    std::string day, month, year;
    if (!parse_date_string(date_string, day, month, year)) {
        return (fs::path(g_project_root) / "Everyday_data" / (date_string + ".dat")).string();
    }
    std::string month_dir = get_month_folder_name(month);
    return (fs::path(g_project_root) / "Everyday_data" / year / month_dir / (date_string + ".dat")).string();
}

// Helper date struct for range calculation
struct Date {
    int day;
    int month;
    int year;
    
    bool operator<=(const Date& other) const {
        if (year != other.year) return year < other.year;
        if (month != other.month) return month < other.month;
        return day <= other.day;
    }
    
    std::string to_string() const {
        std::stringstream ss;
        ss << std::setw(2) << std::setfill('0') << day << "_"
           << std::setw(2) << std::setfill('0') << month << "_"
           << std::setw(4) << year;
        return ss.str();
    }
};

static Date parse_date(const std::string& date_str) {
    std::string d, m, y;
    parse_date_string(date_str, d, m, y);
    return Date{std::stoi(d), std::stoi(m), std::stoi(y)};
}

static void increment_date(Date& date) {
    int days_in_months[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (date.year % 4 == 0 && (date.year % 100 != 0 || date.year % 400 == 0)) {
        days_in_months[2] = 29;
    }
    
    date.day++;
    if (date.day > days_in_months[date.month]) {
        date.day = 1;
        date.month++;
        if (date.month > 12) {
            date.month = 1;
            date.year++;
        }
    }
}

// Implement APIs

bool log_create_day(const char* date_string) {
    std::string filepath = get_daily_log_filepath(date_string);
    if (fs::exists(filepath)) {
        return false;
    }
    
    try {
        fs::create_directories(fs::path(filepath).parent_path());
        std::ofstream file(filepath, std::ios::binary);
        return file.good();
    } catch (const std::exception& e) {
        std::cerr << "[DailyLog] Error creating day log: " << e.what() << std::endl;
        return false;
    }
}

bool log_day_exists(const char* date_string) {
    return fs::exists(get_daily_log_filepath(date_string));
}

bool log_add_entry(const char* date_string, const LogEntry& entry) {
    std::string filepath = get_daily_log_filepath(date_string);
    std::vector<LogEntry> entries;
    
    // Read existing entries if file exists
    if (fs::exists(filepath)) {
        deserialize_log_entries(filepath, entries);
    } else {
        // Create file if it doesn't exist
        log_create_day(date_string);
    }
    
    // Check if duplicate entry
    for (const auto& existing : entries) {
        if (std::strcmp(existing.roll_number, entry.roll_number) == 0) {
            std::cerr << "[DailyLog] Warning: Duplicate log entry today for student: " << entry.roll_number << std::endl;
            return false;
        }
    }
    
    entries.push_back(entry);
    return serialize_log_entries(filepath, entries);
}

bool log_update_entry(const char* date_string, const char* roll_number, const LogEntry& updated_entry) {
    std::string filepath = get_daily_log_filepath(date_string);
    std::vector<LogEntry> entries;
    if (!deserialize_log_entries(filepath, entries)) {
        return false;
    }
    
    bool found = false;
    for (auto& entry : entries) {
        if (std::strcmp(entry.roll_number, roll_number) == 0) {
            entry = updated_entry;
            found = true;
            break;
        }
    }
    
    if (!found) {
        std::cerr << "[DailyLog] Error: Cannot update entry, student " << roll_number << " has not scanned today yet." << std::endl;
        return false;
    }
    
    return serialize_log_entries(filepath, entries);
}

bool log_get_entry(const char* date_string, const char* roll_number, LogEntry& entry) {
    std::string filepath = get_daily_log_filepath(date_string);
    std::vector<LogEntry> entries;
    if (!deserialize_log_entries(filepath, entries)) {
        return false;
    }
    
    for (const auto& e : entries) {
        if (std::strcmp(e.roll_number, roll_number) == 0) {
            entry = e;
            return true;
        }
    }
    return false;
}

std::vector<LogEntry> log_get_all_entries(const char* date_string) {
    std::string filepath = get_daily_log_filepath(date_string);
    std::vector<LogEntry> entries;
    deserialize_log_entries(filepath, entries);
    return entries;
}

std::vector<LogEntry> log_get_entries_in_range(const char* start_date, const char* end_date) {
    std::vector<LogEntry> results;
    try {
        Date start = parse_date(start_date);
        Date end = parse_date(end_date);
        
        for (Date curr = start; curr <= end; increment_date(curr)) {
            std::string date_str = curr.to_string();
            std::string filepath = get_daily_log_filepath(date_str);
            if (fs::exists(filepath)) {
                std::vector<LogEntry> day_entries;
                if (deserialize_log_entries(filepath, day_entries)) {
                    results.insert(results.end(), day_entries.begin(), day_entries.end());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[DailyLog] Error parsing date range: " << e.what() << std::endl;
    }
    return results;
}

bool log_delete_day(const char* date_string) {
    std::string filepath = get_daily_log_filepath(date_string);
    try {
        if (fs::exists(filepath)) {
            fs::remove(filepath);
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DailyLog] Error deleting day log file: " << e.what() << std::endl;
    }
    return false;
}
