#ifndef SERIALIZER_V2_H
#define SERIALIZER_V2_H

#include "engine.h"
#include <string>
#include <vector>

// Binary Disk Serialization Utilities
bool serializer_write_student(const std::string& filepath, const StudentRecord& record);
bool serializer_read_student(const std::string& filepath, StudentRecord& record);

bool serializer_write_log_entry(const std::string& filepath, const LogEntry& entry);
bool serializer_read_log_entries(const std::string& filepath, std::vector<LogEntry>& entries);

bool serializer_write_home_record(const std::string& filepath, const HomeRecord& record);
bool serializer_read_home_records(const std::string& filepath, std::vector<HomeRecord>& records);

bool serializer_append_rejection(const std::string& filepath, const uint8_t* scan_data, int len);

#endif // SERIALIZER_V2_H
