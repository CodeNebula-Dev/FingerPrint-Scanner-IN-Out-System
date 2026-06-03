#ifndef SERIALIZER_H
#define SERIALIZER_H

#include "engine.h"
#include <string>
#include <vector>

// Student Record Serialization
bool serialize_student(const std::string& filepath, const StudentRecord& student);
bool deserialize_student(const std::string& filepath, StudentRecord& student);

// Daily Log Entries Serialization (sequential records in a single file)
bool serialize_log_entries(const std::string& filepath, const std::vector<LogEntry>& entries);
bool deserialize_log_entries(const std::string& filepath, std::vector<LogEntry>& entries);

// Home Records Serialization (sequential records in a single file)
bool serialize_home_records(const std::string& filepath, const std::vector<HomeRecord>& records);
bool deserialize_home_records(const std::string& filepath, std::vector<HomeRecord>& records);

#endif // SERIALIZER_H
