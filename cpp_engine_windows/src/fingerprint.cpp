#include "engine.h"
#include "crypto_placeholder.h"
#include "indexer.h"
#include "serializer.h"
#include <cstring>
#include <vector>
#include <iostream>

bool fingerprint_enroll(const char* roll_number, const uint8_t* raw_template, int length) {
    if (!roll_number || !raw_template || length <= 0) return false;

    // Fetch existing student record or get fields
    StudentRecord student;
    if (!student_get(roll_number, student)) {
        std::cerr << "Fingerprint Enroll Error: Student profile not found for " << roll_number << std::endl;
        return false;
    }

    // 1. Pass raw template through modular encryption/transformation interface
    if (!crypto_enroll_transform(raw_template, static_cast<size_t>(length), student.encrypted_template, TEMPLATE_SIZE)) {
        std::cerr << "Fingerprint Enroll Error: Crypto transformation failed." << std::endl;
        return false;
    }

    // 2. Persist updated student record with encrypted template payload
    return student_update(roll_number, student);
}

MatchResult fingerprint_match(const uint8_t* live_scan, int scan_length) {
    MatchResult result;
    std::memset(&result, 0, sizeof(MatchResult));
    result.matched = false;
    result.confidence_score = 0.0f;
    result.match_count = 0;

    if (!live_scan || scan_length <= 0) return result;

    // 1. Transform incoming live scan using active encryption scheme
    uint8_t live_transformed[TEMPLATE_SIZE];
    if (!crypto_enroll_transform(live_scan, static_cast<size_t>(scan_length), live_transformed, TEMPLATE_SIZE)) {
        return result;
    }

    // 2. Compute Level-1 Index Hash
    uint64_t live_hash = indexer_hash_template(live_transformed, TEMPLATE_SIZE);

    // 3. Retrieve all students for Level-2 evaluation
    auto all_students = student_list_all();
    const float MATCH_THRESHOLD = 0.85f;

    for (const auto& candidate : all_students) {
        // Evaluate similarity in transformed domain via crypto_placeholder slot
        MatchScoreResult eval = crypto_match_evaluate(
            live_transformed,
            TEMPLATE_SIZE,
            candidate.encrypted_template,
            MATCH_THRESHOLD
        );

        if (eval.is_matched) {
            result.match_count++;

            if (!result.matched || eval.confidence_score > result.confidence_score) {
                result.matched = true;
                result.confidence_score = eval.confidence_score;
                std::strncpy(result.roll_number, candidate.roll_number, sizeof(result.roll_number) - 1);
                std::strncpy(result.name, candidate.name, sizeof(result.name) - 1);
                std::strncpy(result.program, candidate.program, sizeof(result.program) - 1);
                std::strncpy(result.batch, candidate.batch, sizeof(result.batch) - 1);
                result.year = candidate.year;
                std::strncpy(result.phone_number, candidate.phone_number, sizeof(result.phone_number) - 1);
                result.is_hosteller = candidate.is_hosteller;
            }
        }
    }

    return result;
}
