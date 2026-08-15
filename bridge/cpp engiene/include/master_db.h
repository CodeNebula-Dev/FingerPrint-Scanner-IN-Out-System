#ifndef MASTER_DB_H
#define MASTER_DB_H

#include "engine.h"
#include "indexer.h"
#include "serializer.h"

struct BatchIndexEntry {
    char roll_number[20];
};

bool engine_init(const char* project_root_path);
void engine_shutdown();
bool student_add(const StudentRecord& record);
bool find_student_file_path(const char* roll_number, std::string& filepath, std::string& batch);
bool student_remove(const char* roll_number);
bool student_update(const char* roll_number, const StudentRecord& updated_record);
bool student_get(const char* roll_number, StudentRecord& record);
int batch_promote(const char* batch);
int batch_promote_all();
bool batch_delete(const char* batch);
bool engine_wipe_all_data();

#endif
