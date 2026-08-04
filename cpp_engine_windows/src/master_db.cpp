#include "engine.h"
#include "serializer.h"
#include "indexer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

// Helper to get the batch index file path
std::string get_batch_index_path(const std::string &batch)
{
    return (fs::path(g_project_root) / "Student_data" / (batch + "_batch") / "index.dat").string();
}

// Helper to get student data file path
std::string get_student_file_path(const std::string &batch, const std::string &roll_number)
{
    return (fs::path(g_project_root) / "Student_data" / (batch + "_batch") / (roll_number + ".dat")).string();
}

// Helper to get fingerprint file path (separate .fpt file)
std::string get_fingerprint_file_path(const std::string &batch, const std::string &roll_number)
{
    return (fs::path(g_project_root) / "Student_data" / (batch + "_batch") / (roll_number + ".fpt")).string();
}

// Helper to get student relative file path (for master index)
std::string get_student_relative_path(const std::string &batch, const std::string &roll_number)
{
    return (fs::path("Student_data") / (batch + "_batch") / (roll_number + ".dat")).string();
}

// Helper structure for batch index entry
struct BatchIndexEntry
{
    char roll_number[20];
};

// Initialize the database engine
bool engine_init(const char *project_root_path)
{
    g_project_root = project_root_path;
    try
    {
        fs::create_directories(fs::path(g_project_root) / "Student_data");
        fs::create_directories(fs::path(g_project_root) / "Everyday_data");
        fs::create_directories(fs::path(g_project_root) / "Home_data");
        fs::create_directories(fs::path(g_project_root) / "Rejection_log");

        // Load indexer cache
        return indexer_load();
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Engine] Init Error: " << e.what() << std::endl;
        return false;
    }
}

// Shutdown the database engine
void engine_shutdown()
{
    g_fingerprint_cache.clear();
    std::cout << "[Engine] Shutdown complete." << std::endl;
}

// Add a student to the master database
bool student_add(const StudentRecord &record)
{
    std::string batch = record.batch;
    std::string roll = record.roll_number;

    // Ensure the batch directory exists
    std::string batch_dir = (fs::path(g_project_root) / "Student_data" / (batch + "_batch")).string();
    try
    {
        fs::create_directories(batch_dir);
    }
    catch (const std::exception &e)
    {
        std::cerr << "[MasterDB] Error creating batch dir: " << e.what() << std::endl;
        return false;
    }

    // Check if student already exists by retrieving file path
    std::string filepath = get_student_file_path(batch, roll);
    if (fs::exists(filepath))
    {
        std::cerr << "[MasterDB] Error: Student with roll number " << roll << " already exists in batch " << batch << std::endl;
        return false;
    }

    // Serialize student record
    if (!serialize_student(filepath, record))
    {
        return false;
    }

    // Write separate fingerprint file (.fpt) for easier inspection
    std::string fpt_path = get_fingerprint_file_path(batch, roll);
    if (!serialize_fingerprint(fpt_path, record.fingerprint_template, 512))
    {
        std::cerr << "[MasterDB] Warning: Failed to write .fpt file for " << roll << std::endl;
    }

    // Update batch index
    std::string idx_path = get_batch_index_path(batch);
    std::vector<BatchIndexEntry> entries;
    std::ifstream idx_file_in(idx_path, std::ios::binary);
    if (idx_file_in.is_open())
    {
        BatchIndexEntry entry;
        while (idx_file_in.read(reinterpret_cast<char *>(&entry), sizeof(BatchIndexEntry)))
        {
            entries.push_back(entry);
        }
        idx_file_in.close();
    }

    // Add to batch index if not exists
    bool exists_in_idx = false;
    for (const auto &entry : entries)
    {
        if (std::strcmp(entry.roll_number, record.roll_number) == 0)
        {
            exists_in_idx = true;
            break;
        }
    }

    if (!exists_in_idx)
    {
        std::ofstream idx_file_out(idx_path, std::ios::binary | std::ios::app);
        if (idx_file_out.is_open())
        {
            BatchIndexEntry new_entry;
            std::strncpy(new_entry.roll_number, record.roll_number, sizeof(new_entry.roll_number) - 1);
            new_entry.roll_number[sizeof(new_entry.roll_number) - 1] = '\0';
            idx_file_out.write(reinterpret_cast<const char *>(&new_entry), sizeof(BatchIndexEntry));
            idx_file_out.close();
        }
        else
        {
            std::cerr << "[MasterDB] Error: Failed to update batch index file: " << idx_path << std::endl;
            return false;
        }
    }

    // Update master fingerprint index
    uint32_t hash = compute_fnv1a_hash(record.fingerprint_template, 512);
    std::string rel_path = get_student_relative_path(batch, roll);
    return indexer_add_or_update(record.roll_number, hash, rel_path);
}

// Helper to find student file path from roll number using global index
bool find_student_file_path(const char *roll_number, std::string &filepath, std::string &batch)
{
    for (const auto &entry : g_fingerprint_cache)
    {
        if (std::strcmp(entry.roll_number, roll_number) == 0)
        {
            filepath = (fs::path(g_project_root) / entry.file_path).string();
            // Extract batch from path e.g. Student_data/2025_batch/25CSE001.dat
            // We can parse the filename or load the record to check the batch.
            // Let's load the record later or parse the path.
            fs::path p(entry.file_path);
            std::string parent_dir = p.parent_path().filename().string(); // e.g. "2025_batch"
            size_t pos = parent_dir.find("_batch");
            if (pos != std::string::npos)
            {
                batch = parent_dir.substr(0, pos);
            }
            else
            {
                batch = "";
            }
            return true;
        }
    }
    return false;
}

// Remove a student from the master database
bool student_remove(const char *roll_number)
{
    std::string filepath;
    std::string batch;
    if (!find_student_file_path(roll_number, filepath, batch))
    {
        std::cerr << "[MasterDB] Error: Student with roll number " << roll_number << " not found in index." << std::endl;
        return false;
    }

    // Delete student .dat file and .fpt fingerprint file
    try
    {
        if (fs::exists(filepath))
        {
            fs::remove(filepath);
        }
        // Also delete the separate fingerprint file
        std::string fpt_path = get_fingerprint_file_path(batch, roll_number);
        if (fs::exists(fpt_path))
        {
            fs::remove(fpt_path);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "[MasterDB] Error removing student file: " << e.what() << std::endl;
        return false;
    }

    // Remove from batch index
    std::string idx_path = get_batch_index_path(batch);
    std::vector<BatchIndexEntry> entries;
    std::ifstream idx_file_in(idx_path, std::ios::binary);
    if (idx_file_in.is_open())
    {
        BatchIndexEntry entry;
        while (idx_file_in.read(reinterpret_cast<char *>(&entry), sizeof(BatchIndexEntry)))
        {
            if (std::strcmp(entry.roll_number, roll_number) != 0)
            {
                entries.push_back(entry);
            }
        }
        idx_file_in.close();
    }

    // Write back updated batch index
    std::ofstream idx_file_out(idx_path, std::ios::binary | std::ios::trunc);
    if (idx_file_out.is_open())
    {
        for (const auto &entry : entries)
        {
            idx_file_out.write(reinterpret_cast<const char *>(&entry), sizeof(BatchIndexEntry));
        }
        idx_file_out.close();
    }

    // Remove from global indexer
    return indexer_remove(roll_number);
}

// Update a student record
bool student_update(const char *roll_number, const StudentRecord &updated_record)
{
    std::string filepath;
    std::string batch;
    if (!find_student_file_path(roll_number, filepath, batch))
    {
        return false;
    }

    // If the student changed batch or roll number, we'd need to move files.
    // For simplicity, we assume roll_number and batch do not change during an update.
    // If they do change, it's safer to delete and add.
    if (std::strcmp(roll_number, updated_record.roll_number) != 0 || batch != updated_record.batch)
    {
        // Remove old and insert new
        StudentRecord old_record;
        if (!deserialize_student(filepath, old_record))
        {
            return false;
        }
        if (!student_remove(roll_number))
        {
            return false;
        }
        if (!student_add(updated_record))
        {
            // Rollback (try to restore old record)
            student_add(old_record);
            return false;
        }
        return true;
    }

    // Standard overwrite
    if (!serialize_student(filepath, updated_record))
    {
        return false;
    }

    // Also update the separate fingerprint file
    std::string fpt_path = get_fingerprint_file_path(batch, roll_number);
    if (!serialize_fingerprint(fpt_path, updated_record.fingerprint_template, 512))
    {
        std::cerr << "[MasterDB] Warning: Failed to update .fpt file for " << roll_number << std::endl;
    }

    // Recompute fingerprint hash and update index
    uint32_t hash = compute_fnv1a_hash(updated_record.fingerprint_template, 512);
    std::string rel_path = get_student_relative_path(batch, roll_number);
    return indexer_add_or_update(roll_number, hash, rel_path);
}

// Retrieve a student record
bool student_get(const char *roll_number, StudentRecord &record)
{
    std::string filepath;
    std::string batch;
    if (!find_student_file_path(roll_number, filepath, batch))
    {
        return false;
    }
    return deserialize_student(filepath, record);
}

// List all students in a batch
std::vector<StudentRecord> student_list_by_batch(const char *batch)
{
    std::vector<StudentRecord> students;
    std::string idx_path = get_batch_index_path(batch);
    std::ifstream idx_file(idx_path, std::ios::binary);
    if (!idx_file.is_open())
    {
        return students;
    }

    BatchIndexEntry entry;
    while (idx_file.read(reinterpret_cast<char *>(&entry), sizeof(BatchIndexEntry)))
    {
        std::string s_filepath = get_student_file_path(batch, entry.roll_number);
        StudentRecord record;
        if (deserialize_student(s_filepath, record))
        {
            students.push_back(record);
        }
    }
    return students;
}

// List all students in the entire database
std::vector<StudentRecord> student_list_all()
{
    std::vector<StudentRecord> students;
    // We can load all students in cache
    for (const auto &entry : g_fingerprint_cache)
    {
        std::string filepath = (fs::path(g_project_root) / entry.file_path).string();
        StudentRecord record;
        if (deserialize_student(filepath, record))
        {
            students.push_back(record);
        }
    }
    return students;
}

// Promote a batch (increment year by 1)
int batch_promote(const char *batch)
{
    std::string idx_path = get_batch_index_path(batch);
    std::ifstream idx_file(idx_path, std::ios::binary);
    if (!idx_file.is_open())
    {
        return 0;
    }

    int count = 0;
    BatchIndexEntry entry;
    std::vector<std::string> rolls;
    while (idx_file.read(reinterpret_cast<char *>(&entry), sizeof(BatchIndexEntry)))
    {
        rolls.push_back(entry.roll_number);
    }
    idx_file.close();

    for (const auto &roll : rolls)
    {
        std::string s_filepath = get_student_file_path(batch, roll);
        StudentRecord record;
        if (deserialize_student(s_filepath, record))
        {
            record.year += 1;
            if (serialize_student(s_filepath, record))
            {
                count++;
            }
        }
    }
    return count;
}

// Promote all batches
int batch_promote_all()
{
    int count = 0;
    // We can iterate over the cache to find all students and update them
    for (const auto &entry : g_fingerprint_cache)
    {
        std::string filepath = (fs::path(g_project_root) / entry.file_path).string();
        StudentRecord record;
        if (deserialize_student(filepath, record))
        {
            record.year += 1;
            if (serialize_student(filepath, record))
            {
                count++;
            }
        }
    }
    return count;
}

// Delete an entire batch
bool batch_delete(const char *batch)
{
    std::string idx_path = get_batch_index_path(batch);
    std::ifstream idx_file(idx_path, std::ios::binary);
    if (idx_file.is_open())
    {
        BatchIndexEntry entry;
        while (idx_file.read(reinterpret_cast<char *>(&entry), sizeof(BatchIndexEntry)))
        {
            // Remove from global indexer
            indexer_remove(entry.roll_number);
        }
        idx_file.close();
    }

    // Delete the directory
    std::string batch_dir = (fs::path(g_project_root) / "Student_data" / (std::string(batch) + "_batch")).string();
    try
    {
        if (fs::exists(batch_dir))
        {
            fs::remove_all(batch_dir);
            return true;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "[MasterDB] Error deleting batch folder: " << e.what() << std::endl;
    }
    return false;
}

// [DEV ONLY] Wipe all data from the database for a fresh start
bool engine_wipe_all_data()
{
    try
    {
        // Delete all data directories
        std::string dirs[] = {"Student_data", "Everyday_data", "Home_data", "Rejection_log"};
        for (const auto &dir : dirs)
        {
            fs::path dir_path = fs::path(g_project_root) / dir;
            if (fs::exists(dir_path))
            {
                fs::remove_all(dir_path);
                std::cout << "[Engine] Deleted: " << dir_path.string() << std::endl;
            }
        }

        // Clear in-memory fingerprint cache
        g_fingerprint_cache.clear();

        // Re-create empty directories so the engine is ready
        for (const auto &dir : dirs)
        {
            fs::create_directories(fs::path(g_project_root) / dir);
        }

        std::cout << "[Engine] All data wiped. Fresh directories created." << std::endl;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Engine] Error during data wipe: " << e.what() << std::endl;
        return false;
    }
}
