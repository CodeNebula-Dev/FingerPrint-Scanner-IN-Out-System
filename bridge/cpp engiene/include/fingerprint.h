#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <cstdint>
#include "engine.h"

struct RejectionEntry {
    char timestamp[25];
    uint8_t failed_template[512];
};

MatchResult fingerprint_match(const uint8_t* live_scan, int scan_length);
bool rejection_log_write(const char* date_string, const uint8_t* failed_scan, int scan_length);

#endif