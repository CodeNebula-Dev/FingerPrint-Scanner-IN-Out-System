#ifndef DAILYLOG_H
#define DAILYLOG_H

#include <string>
#include <vector>
#include "engine.h"

bool log_create_day(const char* date_string);
bool log_day_exists(const char* date_string);
bool log_add_entry(const char* date_string, const LogEntry& entry);
bool log_update_entry(const char* date_string, const char* roll_number, const LogEntry& updated_entry);
bool log_get_entry(const char* date_string, const char* roll_number, LogEntry& entry);
std::vector<LogEntry> log_get_all_entries(const char* date_string);
std::vector<LogEntry> log_get_entries_in_range(const char* start_date, const char* end_date);
bool log_delete_day(const char* date_string);

#endif