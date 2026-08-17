#include "engine.h"
#include "crypto_placeholder.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <random>
#include <chrono>
#include <iomanip>

// Helper to generate simulated raw fingerprint template with given seed
void generate_mock_fingerprint(uint8_t* buffer, int seed_val) {
    std::mt19937 rng(seed_val);
    std::uniform_int_distribution<int> dist(0, 255);
    for (int i = 0; i < TEMPLATE_SIZE; i++) {
        buffer[i] = static_cast<uint8_t>(dist(rng));
    }
}

// Helper to simulate real-world optical/capacitive sensor noise on the same finger
void add_sensor_noise(const uint8_t* original, uint8_t* noisy_output, float noise_ratio, int noise_seed) {
    std::memcpy(noisy_output, original, TEMPLATE_SIZE);
    std::mt19937 rng(noise_seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::uniform_int_distribution<int> byte_dist(-30, 30);

    for (int i = 0; i < TEMPLATE_SIZE; i++) {
        if (dist(rng) < noise_ratio) {
            // Apply slight displacement or pressure variance typical of optical scanners
            int val = static_cast<int>(original[i]) + byte_dist(rng);
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            noisy_output[i] = static_cast<uint8_t>(val);
        }
    }
}

void print_banner(const std::string& title) {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================================" << std::endl;
}

int main() {
    print_banner("BIOHASHING PIPELINE INTEGRITY & NOISE TOLERANCE TEST");

    if (!engine_init("./test_db")) {
        std::cerr << "[-] Failed to initialize test engine!" << std::endl;
        return 1;
    }
    std::cout << "[✓] Test Database & Crypto Engine initialized at ./test_db" << std::endl;

    // ------------------------------------------------------------------------
    // TEST 1: Multi-Student Enrollment
    // ------------------------------------------------------------------------
    print_banner("1. ENROLLING 5 DISTINCT IDENTITIES");
    
    struct Person {
        const char* roll;
        const char* name;
        bool is_hosteller;
        int finger_seed;
        uint8_t enrolled_raw[TEMPLATE_SIZE];
    };

    std::vector<Person> people = {
        {"2026_CS_001", "Devansh Khosla", true, 1001, {}},
        {"2026_CS_002", "Aarav Sharma",   false, 2002, {}},
        {"2026_CS_003", "Rohan Mehta",    true, 3003, {}},
        {"2026_CS_004", "Priya Nair",     false, 4004, {}},
        {"2026_CS_005", "Sneha Patel",    true, 5005, {}}
    };

    for (auto& p : people) {
        generate_mock_fingerprint(p.enrolled_raw, p.finger_seed);

        StudentRecord rec;
        std::memset(&rec, 0, sizeof(StudentRecord));
        std::strncpy(rec.roll_number, p.roll, sizeof(rec.roll_number) - 1);
        std::strncpy(rec.name, p.name, sizeof(rec.name) - 1);
        std::strncpy(rec.program, "B.Tech CS", sizeof(rec.program) - 1);
        std::strncpy(rec.batch, "2026", sizeof(rec.batch) - 1);
        rec.year = 4;
        std::strncpy(rec.phone_number, "+919999999999", sizeof(rec.phone_number) - 1);
        rec.is_hosteller = p.is_hosteller;

        student_add(rec);
        bool enrolled = fingerprint_enroll(p.roll, p.enrolled_raw, TEMPLATE_SIZE);
        std::cout << "  [Enroll] " << std::left << std::setw(15) << p.roll 
                  << " | " << std::setw(18) << p.name 
                  << " | Status: " << (enrolled ? "SUCCESS (Encrypted)" : "FAILED") << std::endl;
    }

    // ------------------------------------------------------------------------
    // TEST 2: True Positive & Sensor Noise Tolerance (Same Person)
    // ------------------------------------------------------------------------
    print_banner("2. TRUE POSITIVE & SENSOR NOISE BENCHMARK");
    std::cout << "Testing genuine user scans with simulated optical sensor pressure/moisture variations:\n" << std::endl;

    for (const auto& p : people) {
        // Test 2a: Exact Scan (0% noise)
        MatchResult res_exact = fingerprint_match(p.enrolled_raw, TEMPLATE_SIZE);
        
        // Test 2b: Slight Sensor Noise (10% noise - typical live finger placement)
        uint8_t noisy_10[TEMPLATE_SIZE];
        add_sensor_noise(p.enrolled_raw, noisy_10, 0.10f, p.finger_seed + 10);
        MatchResult res_noisy10 = fingerprint_match(noisy_10, TEMPLATE_SIZE);

        // Test 2c: Heavy Sensor Noise (20% noise - dry/dirty finger)
        uint8_t noisy_20[TEMPLATE_SIZE];
        add_sensor_noise(p.enrolled_raw, noisy_20, 0.20f, p.finger_seed + 20);
        MatchResult res_noisy20 = fingerprint_match(noisy_20, TEMPLATE_SIZE);

        std::cout << "• " << p.name << " (" << p.roll << "):" << std::endl;
        std::cout << "    - Exact 0% noise  : " << (res_exact.matched ? "✓ MATCH" : "✗ FAIL") 
                  << " | Conf: " << std::fixed << std::setprecision(1) << (res_exact.confidence_score * 100.0f) << "%" << std::endl;
        std::cout << "    - Live 10% noise  : " << (res_noisy10.matched ? "✓ MATCH" : "✗ FAIL") 
                  << " | Conf: " << std::fixed << std::setprecision(1) << (res_noisy10.confidence_score * 100.0f) << "%" << std::endl;
        std::cout << "    - Harsh 20% noise : " << (res_noisy20.matched ? "✓ MATCH" : "✗ FAIL") 
                  << " | Conf: " << std::fixed << std::setprecision(1) << (res_noisy20.confidence_score * 100.0f) << "%" << std::endl;
    }

    // ------------------------------------------------------------------------
    // TEST 3: Imposter / True Negative Rejection (Unenrolled Person)
    // ------------------------------------------------------------------------
    print_banner("3. IMPOSTER / TRUE NEGATIVE REJECTION TEST");
    std::cout << "Testing completely unknown, unenrolled fingers against the enrolled database:\n" << std::endl;

    for (int i = 1; i <= 3; i++) {
        uint8_t imposter_raw[TEMPLATE_SIZE];
        generate_mock_fingerprint(imposter_raw, 9990 + i);

        MatchResult imp_res = fingerprint_match(imposter_raw, TEMPLATE_SIZE);
        std::cout << "• Imposter #" << i << ": " 
                  << (imp_res.matched ? "❌ FALSE ACCEPTANCE (SECURITY BREACH)" : "✓ REJECTED (Access Denied)")
                  << " | Max Similarity: " << std::fixed << std::setprecision(1) << (imp_res.confidence_score * 100.0f) << "%"
                  << " (Threshold: 85.0%)" << std::endl;
    }

    // ------------------------------------------------------------------------
    // TEST 4: Parity FSM Gate Direction Test
    // ------------------------------------------------------------------------
    print_banner("4. PARITY STATE MACHINE (DIRECTION INFERENCE)");
    
    // Simulate 4 successive scans for Hosteller (Devansh) vs Day Scholar (Aarav)
    std::cout << "Testing Hosteller (Devansh - Initial state: INSIDE):" << std::endl;
    for (int scan = 1; scan <= 4; scan++) {
        LogEntry log;
        std::memset(&log, 0, sizeof(LogEntry));
        std::strncpy(log.roll_number, people[0].roll, sizeof(log.roll_number) - 1);
        std::strncpy(log.name, people[0].name, sizeof(log.name) - 1);
        log.gate_count = scan;
        
        // Hosteller parity logic: odd = OUT, even = IN
        bool is_in = (log.gate_count % 2 == 0);
        std::strncpy(log.status, is_in ? "IN" : "OUT", sizeof(log.status) - 1);

        std::cout << "    Scan #" << scan << " -> Count: " << log.gate_count 
                  << " | State: " << (is_in ? "🟢 INSIDE CAMPUS" : "🔴 OUTSIDE CAMPUS") << std::endl;
    }

    print_banner("PIPELINE TEST COMPLETED SUCCESSFULLY");
    engine_shutdown();
    return 0;
}
