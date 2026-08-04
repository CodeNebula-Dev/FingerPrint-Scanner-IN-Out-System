#ifndef WINDOWS_BIOMETRIC_H
#define WINDOWS_BIOMETRIC_H

// Triggers the native Windows fingerprint biometric scanner (via WinBio).
// Returns true if the user authenticated successfully, false otherwise.
//
// Drop-in replacement for macos_touch_id_authenticate() — same signature,
// same semantics, so calling code doesn't need to change.
bool windows_biometric_authenticate(const char *prompt_reason);

#endif // WINDOWS_BIOMETRIC_H