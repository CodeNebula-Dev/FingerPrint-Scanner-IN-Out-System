#ifndef INDEXER_H
#define INDEXER_H

#include "engine.h"
#include <vector>
#include <string>

// In-memory cache entry for fingerprint matching
struct CachedFingerprint {
    uint32_t hash;
    char roll_number[20];
    char file_path[256]; // Relative path to project root
};

// Global in-memory cache of fingerprint templates/hashes
extern std::vector<CachedFingerprint> g_fingerprint_cache;
extern std::string g_project_root;

// Hashing helper (FNV-1a 32-bit hash)
uint32_t compute_fnv1a_hash(const uint8_t* data, size_t length);

// Load all entries from master_index.dat into memory
bool indexer_load();

// Add or update an entry in the master index file and cache
bool indexer_add_or_update(const char* roll_number, uint32_t hash, const std::string& relative_path);

// Remove an entry from the master index file and cache
bool indexer_remove(const char* roll_number);

#endif // INDEXER_H
