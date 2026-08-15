#include "indexer.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

std::vector<CachedFingerprint> g_fingerprint_cache;
std::string g_project_root;

uint32_t compute_fnv1a_hash(const uint8_t* data, size_t length) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

std::string get_master_index_path() {
    return (fs::path(g_project_root) / "Student_data" / "master_index.dat").string();
}

bool indexer_load() {
    g_fingerprint_cache.clear();
    std::string path = get_master_index_path();
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        // It's fine if the file doesn't exist yet, we just start with an empty cache
        return true;
    }
    
    CachedFingerprint entry;
    while (file.read(reinterpret_cast<char*>(&entry), sizeof(CachedFingerprint))) {
        g_fingerprint_cache.push_back(entry);
    }
    
    std::cout << "[Indexer] Loaded " << g_fingerprint_cache.size() << " student records into fingerprint cache." << std::endl;
    return true;
}

bool save_cache_to_disk() {
    std::string path = get_master_index_path();
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[Indexer] Error: Failed to open master index for writing: " << path << std::endl;
        return false;
    }
    
    for (const auto& entry : g_fingerprint_cache) {
        file.write(reinterpret_cast<const char*>(&entry), sizeof(CachedFingerprint));
    }
    return file.good();
}

bool indexer_add_or_update(const char* roll_number, uint32_t hash, const std::string& relative_path) {
    // Check if it already exists in the cache
    bool found = false;
    for (auto& entry : g_fingerprint_cache) {
        if (std::strcmp(entry.roll_number, roll_number) == 0) {
            entry.hash = hash;
            std::strncpy(entry.file_path, relative_path.c_str(), sizeof(entry.file_path) - 1);
            entry.file_path[sizeof(entry.file_path) - 1] = '\0';
            found = true;
            break;
        }
    }
    
    if (!found) {
        CachedFingerprint new_entry;
        new_entry.hash = hash;
        std::strncpy(new_entry.roll_number, roll_number, sizeof(new_entry.roll_number) - 1);
        new_entry.roll_number[sizeof(new_entry.roll_number) - 1] = '\0';
        std::strncpy(new_entry.file_path, relative_path.c_str(), sizeof(new_entry.file_path) - 1);
        new_entry.file_path[sizeof(new_entry.file_path) - 1] = '\0';
        g_fingerprint_cache.push_back(new_entry);
    }
    
    return save_cache_to_disk();
}

bool indexer_remove(const char* roll_number) {
    for (auto it = g_fingerprint_cache.begin(); it != g_fingerprint_cache.end(); ++it) {
        if (std::strcmp(it->roll_number, roll_number) == 0) {
            g_fingerprint_cache.erase(it);
            return save_cache_to_disk();
        }
    }
    return false; // Not found in index
}
