#include "engine.h"
#include "crypto_placeholder.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>

void print_separator() {
    std::cout << "--------------------------------------------------------" << std::endl;
}

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "  C++ BIOMETRIC MATCHING ENGINE v2.0 (CANCELABLE CRYPTO)" << std::endl;
    std::cout << "========================================================" << std::endl;

    if (!engine_init(".")) {
        std::cerr << "Failed to initialize Engine v2.0" << std::endl;
        return 1;
    }

    std::cout << "[INIT] Engine v2.0 initialized successfully." << std::endl;
    std::cout << "[INIT] Active Crypto Scheme: " << static_cast<int>(crypto_get_active_scheme()) << std::endl;

    print_separator();

    // 1. Enroll Test Student (Hosteller)
    StudentRecord student1;
    std::memset(&student1, 0, sizeof(StudentRecord));
    std::strncpy(student1.roll_number, "2026_CS_001", sizeof(student1.roll_number) - 1);
    std::strncpy(student1.name, "Devansh Khosla", sizeof(student1.name) - 1);
    std::strncpy(student1.program, "B.Tech CS", sizeof(student1.program) - 1);
    std::strncpy(student1.batch, "2026", sizeof(student1.batch) - 1);
    student1.year = 4;
    std::strncpy(student1.phone_number, "+919876543210", sizeof(student1.phone_number) - 1);
    student1.is_hosteller = true;

    // Simulate mock raw 512-byte fingerprint template
    uint8_t mock_raw_template[TEMPLATE_SIZE];
    for (int i = 0; i < TEMPLATE_SIZE; i++) {
        mock_raw_template[i] = static_cast<uint8_t>((i * 7 + 13) % 256);
    }

    if (student_add(student1)) {
        std::cout << "[DB] Added Student: " << student1.name << " (" << student1.roll_number << ")" << std::endl;
    }

    if (fingerprint_enroll(student1.roll_number, mock_raw_template, TEMPLATE_SIZE)) {
        std::cout << "[CRYPTO] Fingerprint enrolled and transformed successfully." << std::endl;
    }

    print_separator();

    // 2. Perform Biometric Verification Test
    std::cout << "[MATCH] Testing live scan verification..." << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();
    MatchResult result = fingerprint_match(mock_raw_template, TEMPLATE_SIZE);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    if (result.matched) {
        std::cout << "  ✓ MATCH SUCCESSFUL!" << std::endl;
        std::cout << "  Student Name  : " << result.name << std::endl;
        std::cout << "  Roll Number   : " << result.roll_number << std::endl;
        std::cout << "  Confidence    : " << (result.confidence_score * 100.0f) << "%" << std::endl;
        std::cout << "  Latency       : " << elapsed_us << " us (" << (elapsed_us / 1000.0f) << " ms)" << std::endl;
    } else {
        std::cout << "  ❌ MATCH FAILED!" << std::endl;
    }

    print_separator();

    // 3. Test Daily Log Entry
    LogEntry entry;
    std::memset(&entry, 0, sizeof(LogEntry));
    std::strncpy(entry.roll_number, student1.roll_number, sizeof(entry.roll_number) - 1);
    std::strncpy(entry.name, student1.name, sizeof(entry.name) - 1);
    entry.year = student1.year;
    std::strncpy(entry.reason, "Campus Gate Exit", sizeof(entry.reason) - 1);
    entry.gate_count = 1;
    std::strncpy(entry.status, "OUT", sizeof(entry.status) - 1);
    entry.late_return = false;
    std::strncpy(entry.timestamps[0], "2026-07-28 08:30:00", sizeof(entry.timestamps[0]) - 1);
    entry.timestamp_count = 1;

    if (log_add_entry("2026-07-28", entry)) {
        std::cout << "[LOG] Gate entry logged for " << student1.roll_number << std::endl;
    }

    engine_shutdown();
    std::cout << "[SHUTDOWN] Engine v2.0 shut down cleanly." << std::endl;
    return 0;
}
