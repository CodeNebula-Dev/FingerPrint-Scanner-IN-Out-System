#include "engine.h"
#include "serializer.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

extern std::string g_root_path;

static std::string get_log_filepath(const char* date_string) {
    std::string date_str(date_string);
    std::string year = date_str.substr(0, 4);
    std::string month = date_str.substr(5, 2);
    
    std::string dir = g_root_path + "/Everyday_data/" + year + "/" + month;
    fs::create_directories(dir);
    return dir + "/" + date_str + ".dat";
}

bool log_create_day(const char* date_string) {
    std::string filepath = get_log_filepath(date_string);
    if (!fs::exists(filepath)) {
        std::ofstream out(filepath, std::ios::binary);
        return out.good();
    }
    return true;
}

bool log_day_exists(const char* date_string) {
    return fs::exists(get_log_filepath(date_string));
}

bool log_add_entry(const char* date_string, const LogEntry& entry) {
    std::string filepath = get_log_filepath(date_string);
    return serializer_write_log_entry(filepath, entry);
}

bool log_update_entry(const char* date_string, const char* roll_number, const LogEntry& updated_entry) {
    std::string filepath = get_log_filepath(date_string);
    std::vector<LogEntry> entries;
    if (!serializer_read_log_entries(filepath, entries)) return false;

    bool updated = false;
    for (auto& entry : entries) {
        if (std::string(entry.roll_number) == roll_number) {
            entry = updated_entry;
            updated = true;
            break;
        }
    }

    if (!updated) return false;

    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    for (const auto& entry : entries) {
        out.write(reinterpret_cast<const char*>(&entry), sizeof(LogEntry));
    }
    return out.good();
}

bool log_get_entry(const char* date_string, const char* roll_number, LogEntry& entry) {
    std::string filepath = get_log_filepath(date_string);
    std::vector<LogEntry> entries;
    if (!serializer_read_log_entries(filepath, entries)) return false;

    for (const auto& item : entries) {
        if (std::string(item.roll_number) == roll_number) {
            entry = item;
            return true;
        }
    }
    return false;
}

std::vector<LogEntry> log_get_all_entries(const char* date_string) {
    std::vector<LogEntry> entries;
    std::string filepath = get_log_filepath(date_string);
    serializer_read_log_entries(filepath, entries);
    return entries;
}

std::vector<LogEntry> log_get_entries_in_range(const char* start_date, const char* end_date) {
    std::vector<LogEntry> all_entries;
    std::string base_dir = g_root_path + "/Everyday_data";

    if (!fs::exists(base_dir)) return all_entries;

    std::string start_s(start_date);
    std::string end_s(end_date);

    for (const auto& entry : fs::recursive_directory_iterator(base_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".dat") {
            std::string filename = entry.path().stem().string();
            if (filename >= start_s && filename <= end_s) {
                std::vector<LogEntry> day_entries;
                serializer_read_log_entries(entry.path().string(), day_entries);
                all_entries.insert(all_entries.end(), day_entries.begin(), day_entries.end());
            }
        }
    }
    return all_entries;
}

bool log_delete_day(const char* date_string) {
    std::string filepath = get_log_filepath(date_string);
    if (fs::exists(filepath)) {
        return fs::remove(filepath);
    }
    return false;
}

bool rejection_log_write(const char* date_string, const uint8_t* failed_scan, int scan_length) {
    std::string dir = g_root_path + "/Rejection_log";
    fs::create_directories(dir);
    std::string filepath = dir + "/rejections_" + std::string(date_string) + ".dat";
    return serializer_append_rejection(filepath, failed_scan, scan_length);
}
