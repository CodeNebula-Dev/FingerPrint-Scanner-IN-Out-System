#include "engine.h"
#include "serializer.h"
#include "indexer.h"
#include "crypto_placeholder.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;

std::string g_root_path = "./db_root";

bool engine_init(const char* project_root_path) {
    if (project_root_path && std::strlen(project_root_path) > 0) {
        g_root_path = std::string(project_root_path) + "/db_root";
    }

    try {
        fs::create_directories(g_root_path + "/Student_data");
        fs::create_directories(g_root_path + "/Everyday_data");
        fs::create_directories(g_root_path + "/Home_data");
        fs::create_directories(g_root_path + "/Rejection_log");
    } catch (const std::exception& e) {
        std::cerr << "Engine Init Directory Error: " << e.what() << std::endl;
        return false;
    }

    indexer_init();

    // Initialize BioHash encryption key subsystem
    if (!crypto_init_key(g_root_path)) {
        std::cerr << "Engine Init Warning: BioHash key initialization failed." << std::endl;
        // Non-fatal: engine can still operate, but enrollment/matching will fail gracefully
    }

    // Populate RAM indexer from existing student files on disk
    std::string student_dir = g_root_path + "/Student_data";
    if (fs::exists(student_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(student_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".dat") {
                StudentRecord rec;
                if (serializer_read_student(entry.path().string(), rec)) {
                    indexer_insert(rec.roll_number, rec.encrypted_template, TEMPLATE_SIZE);
                }
            }
        }
    }

    return true;
}

void engine_shutdown() {
    indexer_clear();
}

bool engine_wipe_all_data() {
    try {
        fs::remove_all(g_root_path);
        return engine_init(nullptr);
    } catch (...) {
        return false;
    }
}

static std::string get_student_filepath(const char* batch, const char* roll_number) {
    std::string batch_dir = g_root_path + "/Student_data/" + std::string(batch);
    fs::create_directories(batch_dir);
    return batch_dir + "/" + std::string(roll_number) + ".dat";
}

bool student_add(const StudentRecord& record) {
    std::string filepath = get_student_filepath(record.batch, record.roll_number);
    if (!serializer_write_student(filepath, record)) {
        return false;
    }
    indexer_insert(record.roll_number, record.encrypted_template, TEMPLATE_SIZE);
    return true;
}

bool student_remove(const char* roll_number) {
    StudentRecord rec;
    if (!student_get(roll_number, rec)) return false;

    std::string filepath = get_student_filepath(rec.batch, roll_number);
    indexer_remove(roll_number);
    return fs::remove(filepath);
}

bool student_update(const char* roll_number, const StudentRecord& updated_record) {
    student_remove(roll_number);
    return student_add(updated_record);
}

bool student_get(const char* roll_number, StudentRecord& record) {
    std::string student_dir = g_root_path + "/Student_data";
    if (!fs::exists(student_dir)) return false;

    for (const auto& entry : fs::recursive_directory_iterator(student_dir)) {
        if (entry.is_regular_file() && entry.path().stem().string() == roll_number) {
            return serializer_read_student(entry.path().string(), record);
        }
    }
    return false;
}

std::vector<StudentRecord> student_list_by_batch(const char* batch) {
    std::vector<StudentRecord> list;
    std::string batch_dir = g_root_path + "/Student_data/" + std::string(batch);

    if (fs::exists(batch_dir)) {
        for (const auto& entry : fs::directory_iterator(batch_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".dat") {
                StudentRecord rec;
                if (serializer_read_student(entry.path().string(), rec)) {
                    list.push_back(rec);
                }
            }
        }
    }
    return list;
}

std::vector<StudentRecord> student_list_all() {
    std::vector<StudentRecord> list;
    std::string student_dir = g_root_path + "/Student_data";

    if (fs::exists(student_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(student_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".dat") {
                StudentRecord rec;
                if (serializer_read_student(entry.path().string(), rec)) {
                    list.push_back(rec);
                }
            }
        }
    }
    return list;
}

int batch_promote(const char* batch) {
    auto students = student_list_by_batch(batch);
    int count = 0;
    for (auto& s : students) {
        s.year++;
        student_update(s.roll_number, s);
        count++;
    }
    return count;
}

int batch_promote_all() {
    auto students = student_list_all();
    int count = 0;
    for (auto& s : students) {
        s.year++;
        student_update(s.roll_number, s);
        count++;
    }
    return count;
}

bool batch_delete(const char* batch) {
    std::string batch_dir = g_root_path + "/Student_data/" + std::string(batch);
    if (!fs::exists(batch_dir)) return false;

    auto students = student_list_by_batch(batch);
    for (const auto& s : students) {
        indexer_remove(s.roll_number);
    }
    return fs::remove_all(batch_dir) > 0;
}
