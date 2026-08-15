#ifndef HOME_DB_H
#define HOME_DB_H

#include <vector>
#include <string>
struct HomeRecord {
    char roll_number[32];
};

extern std::string g_project_root;

bool home_add(const HomeRecord& record);
bool home_remove(const char* roll_number);
bool home_exists(const char* roll_number);
std::vector<HomeRecord> home_get_all();
#endif