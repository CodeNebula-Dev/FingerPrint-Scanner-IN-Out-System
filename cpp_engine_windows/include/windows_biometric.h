#ifndef WINDOWS_BIOMETRIC_H
#define WINDOWS_BIOMETRIC_H

#include <cstdint>
#include <vector>
#include <string>

/**
 * @file windows_biometric.h
 * @brief Windows Biometric Driver & Hardware Scanner Interface
 * 
 * Supports:
 *   1. External Hardware Serial Fingerprint Scanners (R307 / R503 / Pico / ESP32 on COM1..COM32)
 *   2. Native Windows Hello Biometric API (WinBio fallback if configured)
 *   3. Interactive Simulation Fallback (Ensures CLI never crashes when no hardware is plugged in)
 */

// Authenticates via active scanner (Hardware COM -> WinBio -> Simulation fallback)
bool windows_biometric_authenticate(const char *prompt_reason);

// Captures a raw 512-byte template from hardware COM scanner or simulation
bool windows_capture_template(uint8_t *template_out, int length);

// Connect / configure active COM port (e.g. "COM3")
bool windows_set_com_port(const char *port_name, int baudrate = 57600);

// List available COM ports on Windows
std::vector<std::string> windows_list_com_ports();

#endif // WINDOWS_BIOMETRIC_H