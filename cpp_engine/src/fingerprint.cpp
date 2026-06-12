#include "engine.h"
#include "indexer.h"
#include "serializer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <chrono>
#include <ctime>

namespace fs = std::filesystem;

extern std::string g_project_root;

// Rejection entry struct
struct RejectionEntry {
    char timestamp[25];
    uint8_t failed_template[512];
};

// Configurable confidence threshold for biometric matching
const float MATCH_THRESHOLD = 0.75f;

// Helper to compute a spatial/energy hash (sum of template bytes) for Level 1 coarse filter
static uint32_t compute_spatial_hash(const uint8_t* template_data, int len) {
    uint32_t sum = 0;
    for (int i = 0; i < len; ++i) {
        sum += template_data[i];
    }
    return sum;
}

// Helper to compare two templates byte-by-byte (Level 2 full comparison)
static float compare_templates(const uint8_t* t1, const uint8_t* t2) {
    int matching_bytes = 0;
    for (int i = 0; i < 512; ++i) {
        if (t1[i] == t2[i]) {
            matching_bytes++;
        }
    }
    return static_cast<float>(matching_bytes) / 512.0f;
}

MatchResult fingerprint_match(const uint8_t* live_scan, int scan_length) {
    MatchResult result;
    std::memset(&result, 0, sizeof(MatchResult));
    result.matched = false;
    result.match_count = 0;
    
    if (live_scan == nullptr || scan_length <= 0) {
        return result;
    }
    
    // Compute FNV-1a hash for logging/indexing
    uint32_t live_hash = compute_fnv1a_hash(live_scan, scan_length);
    
    std::string best_roll = "";
    float highest_score = 0.0f;
    StudentRecord best_student;
    int above_threshold_count = 0;
    
    // Level 1: Coarse Filtering — load all candidates from in-memory cache
    // For small databases (<1000), we check all students directly
    std::vector<CachedFingerprint> candidates;
    for (const auto& entry : g_fingerprint_cache) {
        candidates.push_back(entry);
    }
    
    // Level 2: Full Template Comparison against all candidates
    for (const auto& candidate : candidates) {
        std::string filepath = (fs::path(g_project_root) / candidate.file_path).string();
        StudentRecord student;
        if (deserialize_student(filepath, student)) {
            float score = compare_templates(live_scan, student.fingerprint_template);
            
            // Count ALL students that match above threshold
            if (score >= MATCH_THRESHOLD) {
                above_threshold_count++;
                std::cout << "[Matcher] Candidate match: " << student.name 
                          << " (" << student.roll_number << ") Score: " << score << std::endl;
            }
            
            // Track the best (highest confidence) match
            if (score > highest_score) {
                highest_score = score;
                best_roll = student.roll_number;
                best_student = student;
            }
        }
    }
    
    // Store total number of matches above threshold
    result.match_count = above_threshold_count;
    
    // Check if best match exceeds confidence threshold
    if (highest_score >= MATCH_THRESHOLD) {
        result.matched = true;
        std::strncpy(result.roll_number, best_student.roll_number, sizeof(result.roll_number) - 1);
        std::strncpy(result.name, best_student.name, sizeof(result.name) - 1);
        std::strncpy(result.program, best_student.program, sizeof(result.program) - 1);
        std::strncpy(result.batch, best_student.batch, sizeof(result.batch) - 1);
        result.year = best_student.year;
        std::strncpy(result.phone_number, best_student.phone_number, sizeof(result.phone_number) - 1);
        result.is_hosteller = best_student.is_hosteller;
        result.confidence_score = highest_score;
        
        if (above_threshold_count > 1) {
            std::cout << "[Matcher] WARNING: Multiple entries detected! " 
                      << above_threshold_count << " students share this fingerprint." << std::endl;
            std::cout << "[Matcher] Best match: " << result.name << " (Confidence: " << highest_score << ")" << std::endl;
        } else {
            std::cout << "[Matcher] Match found! Name: " << result.name << ", Confidence: " << highest_score << std::endl;
        }
    } else {
        std::cout << "[Matcher] No match found. Best score: " << highest_score << " (Threshold: " << MATCH_THRESHOLD << ")" << std::endl;
    }
    
    return result;
}

bool fingerprint_enroll(const char* roll_number, const uint8_t* template_data, int length) {
    StudentRecord record;
    if (!student_get(roll_number, record)) {
        std::cerr << "[Enroll] Error: Student not found for enrollment: " << roll_number << std::endl;
        return false;
    }
    
    int copy_len = (length < 512) ? length : 512;
    std::memcpy(record.fingerprint_template, template_data, copy_len);
    if (copy_len < 512) {
        std::memset(record.fingerprint_template + copy_len, 0, 512 - copy_len);
    }
    
    return student_update(roll_number, record);
}

bool rejection_log_write(const char* date_string, const uint8_t* failed_scan, int scan_length) {
    std::string path = (fs::path(g_project_root) / "Rejection_log" / ("rejections_" + std::string(date_string) + ".dat")).string();
    
    RejectionEntry entry;
    
    // Get timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::strftime(entry.timestamp, sizeof(entry.timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    
    // Copy failed scan data
    int copy_len = (scan_length < 512) ? scan_length : 512;
    std::memcpy(entry.failed_template, failed_scan, copy_len);
    if (copy_len < 512) {
        std::memset(entry.failed_template + copy_len, 0, 512 - copy_len);
    }
    
    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        std::cerr << "[RejectionLog] Error: Failed to open rejection log for writing: " << path << std::endl;
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(&entry), sizeof(RejectionEntry));
    return file.good();
}
