#include "serializer.h"
#include <fstream>
#include <iostream>

bool serializer_write_student(const std::string& filepath, const StudentRecord& record) {
    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(&record), sizeof(StudentRecord));
    return out.good();
}

bool serializer_read_student(const std::string& filepath, StudentRecord& record) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) return false;
    in.read(reinterpret_cast<char*>(&record), sizeof(StudentRecord));
    return in.good();
}

bool serializer_write_log_entry(const std::string& filepath, const LogEntry& entry) {
    std::ofstream out(filepath, std::ios::binary | std::ios::app);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(&entry), sizeof(LogEntry));
    return out.good();
}

bool serializer_read_log_entries(const std::string& filepath, std::vector<LogEntry>& entries) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) return false;

    LogEntry entry;
    while (in.read(reinterpret_cast<char*>(&entry), sizeof(LogEntry))) {
        entries.push_back(entry);
    }
    return true;
}

bool serializer_write_home_record(const std::string& filepath, const HomeRecord& record) {
    std::ofstream out(filepath, std::ios::binary | std::ios::app);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(&record), sizeof(HomeRecord));
    return out.good();
}

bool serializer_read_home_records(const std::string& filepath, std::vector<HomeRecord>& records) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) return false;

    HomeRecord record;
    while (in.read(reinterpret_cast<char*>(&record), sizeof(HomeRecord))) {
        records.push_back(record);
    }
    return true;
}

bool serializer_append_rejection(const std::string& filepath, const uint8_t* scan_data, int len) {
    std::ofstream out(filepath, std::ios::binary | std::ios::app);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(scan_data), len);
    return out.good();
}
