#ifndef INDEXER_V2_H
#define INDEXER_V2_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// Fast In-Memory Index mapping template hashes to roll numbers
struct IndexEntry {
    char     roll_number[20];
    uint64_t template_hash; // FNV-1a hash of encrypted template payload
};

void indexer_init();
void indexer_clear();

// Add or update roll number in hash index
void indexer_insert(const char* roll_number, const uint8_t* encrypted_template, size_t len);
void indexer_remove(const char* roll_number);

// Calculate FNV-1a hash over encrypted template bytes
uint64_t indexer_hash_template(const uint8_t* encrypted_template, size_t len);

// Find candidate roll numbers matching template hash (Level-1 Coarse Filter)
std::vector<std::string> indexer_lookup_candidates(uint64_t template_hash);

#endif // INDEXER_V2_H
