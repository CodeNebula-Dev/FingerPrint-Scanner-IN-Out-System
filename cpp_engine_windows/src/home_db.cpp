#include "engine.h"
#include "serializer.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <iostream>

namespace fs = std::filesystem;

extern std::string g_project_root;

// Helper to get active home file path
static std::string get_home_active_filepath()
{
    return (fs::path(g_project_root) / "Home_data" / "home_active.dat").string();
}

bool home_add(const HomeRecord &record)
{
    std::string filepath = get_home_active_filepath();
    std::vector<HomeRecord> records;

    // Read current records
    if (fs::exists(filepath))
    {
        deserialize_home_records(filepath, records);
    }

    // Check if student is already marked as gone home
    for (const auto &r : records)
    {
        if (std::strcmp(r.roll_number, record.roll_number) == 0)
        {
            std::cout << "[HomeDB] Student " << record.roll_number << " is already in active home database." << std::endl;
            return true;
        }
    }

    records.push_back(record);
    return serialize_home_records(filepath, records);
}

bool home_remove(const char *roll_number)
{
    std::string filepath = get_home_active_filepath();
    std::vector<HomeRecord> records;
    if (!fs::exists(filepath))
    {
        return false;
    }

    if (!deserialize_home_records(filepath, records))
    {
        return false;
    }

    bool found = false;
    for (auto it = records.begin(); it != records.end(); ++it)
    {
        if (std::strcmp(it->roll_number, roll_number) == 0)
        {
            records.erase(it);
            found = true;
            break;
        }
    }

    if (!found)
    {
        return false; // Not in list
    }

    return serialize_home_records(filepath, records);
}

bool home_exists(const char *roll_number)
{
    std::string filepath = get_home_active_filepath();
    std::vector<HomeRecord> records;
    if (!fs::exists(filepath))
    {
        return false;
    }

    if (!deserialize_home_records(filepath, records))
    {
        return false;
    }

    for (const auto &r : records)
    {
        if (std::strcmp(r.roll_number, roll_number) == 0)
        {
            return true;
        }
    }
    return false;
}

std::vector<HomeRecord> home_get_all()
{
    std::string filepath = get_home_active_filepath();
    std::vector<HomeRecord> records;
    if (fs::exists(filepath))
    {
        deserialize_home_records(filepath, records);
    }
    return records;
}
