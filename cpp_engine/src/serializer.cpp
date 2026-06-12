#include "serializer.h"
#include <fstream>
#include <iostream>

bool serialize_student(const std::string& filepath, const StudentRecord& student) {
    std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[Serializer] Error: Cannot open student file for writing: " << filepath << std::endl;
        return false;
    }
    file.write(reinterpret_cast<const char*>(&student), sizeof(StudentRecord));
    return file.good();
}

bool deserialize_student(const std::string& filepath, StudentRecord& student) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    file.read(reinterpret_cast<char*>(&student), sizeof(StudentRecord));
    return file.good() || file.gcount() == sizeof(StudentRecord);
}

bool serialize_fingerprint(const std::string& filepath, const uint8_t* template_data, int length) {
    std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[Serializer] Error: Cannot open fingerprint file for writing: " << filepath << std::endl;
        return false;
    }
    int write_len = (length < 512) ? length : 512;
    file.write(reinterpret_cast<const char*>(template_data), write_len);
    return file.good();
}

bool deserialize_fingerprint(const std::string& filepath, uint8_t* template_data, int max_length) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    int read_len = (max_length < 512) ? max_length : 512;
    file.read(reinterpret_cast<char*>(template_data), read_len);
    return file.good() || file.gcount() == read_len;
}

bool serialize_log_entries(const std::string& filepath, const std::vector<LogEntry>& entries) {
    std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[Serializer] Error: Cannot open log file for writing: " << filepath << std::endl;
        return false;
    }
    for (const auto& entry : entries) {
        file.write(reinterpret_cast<const char*>(&entry), sizeof(LogEntry));
    }
    return file.good();
}

bool deserialize_log_entries(const std::string& filepath, std::vector<LogEntry>& entries) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    entries.clear();
    LogEntry entry;
    while (file.read(reinterpret_cast<char*>(&entry), sizeof(LogEntry))) {
        entries.push_back(entry);
    }
    return true; // Reaching EOF is normal
}

bool serialize_home_records(const std::string& filepath, const std::vector<HomeRecord>& records) {
    std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[Serializer] Error: Cannot open home records file for writing: " << filepath << std::endl;
        return false;
    }
    for (const auto& record : records) {
        file.write(reinterpret_cast<const char*>(&record), sizeof(HomeRecord));
    }
    return file.good();
}

bool deserialize_home_records(const std::string& filepath, std::vector<HomeRecord>& records) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    records.clear();
    HomeRecord record;
    while (file.read(reinterpret_cast<char*>(&record), sizeof(HomeRecord))) {
        records.push_back(record);
    }
    return true; // Reaching EOF is normal
}
