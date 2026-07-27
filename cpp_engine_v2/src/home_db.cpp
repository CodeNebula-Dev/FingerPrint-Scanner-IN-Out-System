#include "engine.h"
#include "serializer.h"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

extern std::string g_root_path;

static std::string get_home_filepath() {
    std::string dir = g_root_path + "/Home_data";
    fs::create_directories(dir);
    return dir + "/home_active.dat";
}

bool home_add(const HomeRecord& record) {
    std::string filepath = get_home_filepath();
    return serializer_write_home_record(filepath, record);
}

bool home_remove(const char* roll_number) {
    std::string filepath = get_home_filepath();
    std::vector<HomeRecord> records;
    if (!serializer_read_home_records(filepath, records)) return false;

    auto it = std::remove_if(records.begin(), records.end(), [roll_number](const HomeRecord& rec) {
        return std::string(rec.roll_number) == roll_number;
    });

    if (it == records.end()) return false;

    records.erase(it, records.end());

    // Rewrite file
    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    for (const auto& rec : records) {
        out.write(reinterpret_cast<const char*>(&rec), sizeof(HomeRecord));
    }
    return true;
}

bool home_exists(const char* roll_number) {
    std::string filepath = get_home_filepath();
    std::vector<HomeRecord> records;
    if (!serializer_read_home_records(filepath, records)) return false;

    for (const auto& rec : records) {
        if (std::string(rec.roll_number) == roll_number) {
            return true;
        }
    }
    return false;
}

std::vector<HomeRecord> home_get_all() {
    std::vector<HomeRecord> records;
    std::string filepath = get_home_filepath();
    serializer_read_home_records(filepath, records);
    return records;
}
