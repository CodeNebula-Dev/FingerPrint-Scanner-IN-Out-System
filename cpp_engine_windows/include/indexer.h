#ifndef INDEXER_V2_H
#define INDEXER_V2_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

struct IndexEntry {
    char     roll_number[20];
    uint64_t template_hash;
};

void indexer_init();
void indexer_clear();

void indexer_insert(const char* roll_number, const uint8_t* encrypted_template, size_t len);
void indexer_remove(const char* roll_number);

uint64_t indexer_hash_template(const uint8_t* encrypted_template, size_t len);

std::vector<std::string> indexer_lookup_candidates(uint64_t template_hash);

#endif // INDEXER_V2_H
