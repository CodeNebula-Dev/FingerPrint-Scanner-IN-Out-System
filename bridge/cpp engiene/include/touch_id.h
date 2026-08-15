#ifndef TOUCH_ID_H
#define TOUCH_ID_H

// Triggers the native macOS Touch ID biometric scanner.
// Returns true if the user authenticated successfully, false otherwise.
bool macos_touch_id_authenticate(const char* prompt_reason);

#endif // TOUCH_ID_H
