#include "indexer.h"
#include <unordered_map>
#include <cstring>
#include <algorithm>

static std::unordered_map<std::string, IndexEntry> g_index_map;

void indexer_init() {
    g_index_map.clear();
}

void indexer_clear() {
    g_index_map.clear();
}

uint64_t indexer_hash_template(const uint8_t* encrypted_template, size_t len) {
    if (!encrypted_template || len == 0) return 0;

    // FNV-1a 64-bit Hash over encrypted template payload
    uint64_t hash = 14695981039346656037ULL;
    const uint64_t FNV_prime = 1099511628211ULL;

    for (size_t i = 0; i < len; ++i) {
        hash ^= encrypted_template[i];
        hash *= FNV_prime;
    }
    return hash;
}

void indexer_insert(const char* roll_number, const uint8_t* encrypted_template, size_t len) {
    if (!roll_number || !encrypted_template) return;

    IndexEntry entry;
    std::strncpy(entry.roll_number, roll_number, sizeof(entry.roll_number) - 1);
    entry.roll_number[sizeof(entry.roll_number) - 1] = '\0';
    entry.template_hash = indexer_hash_template(encrypted_template, len);

    g_index_map[std::string(roll_number)] = entry;
}

void indexer_remove(const char* roll_number) {
    if (!roll_number) return;
    g_index_map.erase(std::string(roll_number));
}

std::vector<std::string> indexer_lookup_candidates(uint64_t template_hash) {
    std::vector<std::string> candidates;
    for (const auto& kv : g_index_map) {
        if (kv.second.template_hash == template_hash) {
            candidates.push_back(kv.first);
        }
    }
    return candidates;
}
