# Resources, Credits, and Learning Materials

This document compiles the external APIs, libraries, standards, and algorithms utilized to build the core database and biometric matching C++ engine (`cpp_engine`). It serves as a central registry for credits and references.

---

## 1. Biometrics & Core Hashing Algorithms

### Fowler-Noll-Vo (FNV-1a) Hashing
* **Description:** FNV-1a is a high-performance, non-cryptographic hash function selected for mapping raw 512-byte fingerprint templates to compact 32-bit identifiers. This allows the in-memory cache indexer to perform Level-1 candidate searches without initiating expensive filesystem hits.
* **Learning Reference:** [Landon Curt Noll's FNV Hash Overview](http://www.isthe.com/chongo/tech/comp/fnv/)
* **Code Application:** Located in `indexer.cpp` and `fingerprint.cpp`.
* **Snippet Example:**
  ```cpp
  // From indexer.cpp
  uint32_t compute_fnv1a_hash(const uint8_t* data, int len) {
      uint32_t hash = 2166136261u; // FNV-1a 32-bit offset basis
      for (int i = 0; i < len; ++i) {
          hash ^= data[i];
          hash *= 16777619u;      // FNV-1a 32-bit prime
      }
      return hash;
  }
  ```

---

## 2. Platform Biometrics (macOS Touch ID Integration)

### macOS LocalAuthentication Framework
* **Description:** The macOS native biometric authorization framework. It provides access to secure enclaves (Touch ID) via Objective-C. Used to simulate physical biometric readers during testing phases.
* **Official Documentation:** [Apple Developer: LocalAuthentication](https://developer.apple.com/documentation/localauthentication)
* **Code Application:** Exposed in `touch_id.h` and implemented in `touch_id.mm`.
* **Snippet Example:**
  ```objc
  // From touch_id.mm
  LAContext *myContext = [[LAContext alloc] init];
  NSError *authError = nil;
  
  if ([myContext canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:&authError]) {
      [myContext evaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                localizedReason:reasonNS
                          reply:^(BOOL success, NSError *error) {
          // Callback execution handler
      }];
  }
  ```

### Grand Central Dispatch (GCD) Semaphores
* **Description:** A thread-synchronization system used to bridge the asynchronous Objective-C block execution callback with the synchronous C++ library execution.
* **Official Documentation:** [Apple Developer: DispatchSemaphore](https://developer.apple.com/documentation/dispatch/dispatchsemaphore)
* **Code Application:** Located in `touch_id.mm` to block the C++ main loop thread until user touch input resolves.
* **Snippet Example:**
  ```objc
  // From touch_id.mm
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  
  // Inside local authentication callback block:
  dispatch_semaphore_signal(semaphore);
  
  // On the calling thread, wait synchronously:
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  ```

---

## 3. C++ Standard Libraries

### Standard File System Library (`<filesystem>`)
* **Description:** Standardized directory hierarchy navigation, verification, and creation introduced in C++17. Used to organize batched student files and everyday chronological folders.
* **Technical Reference:** [cppreference: std::filesystem](https://en.cppreference.com/w/cpp/filesystem)
* **Code Application:** Located in `master_db.cpp`, `daily_log.cpp`, and `home_db.cpp`.
* **Snippet Example:**
  ```cpp
  // From master_db.cpp
  namespace fs = std::filesystem;
  
  bool engine_init(const char* project_root_path) {
      g_project_root = project_root_path;
      try {
          fs::create_directories(fs::path(g_project_root) / "Student_data");
          fs::create_directories(fs::path(g_project_root) / "Everyday_data");
          fs::create_directories(fs::path(g_project_root) / "Home_data");
          fs::create_directories(fs::path(g_project_root) / "Rejection_log");
          return indexer_load();
      } catch (...) {
          return false;
      }
  }
  ```

### Standard Stream IO Binary Serialization (`<fstream>`)
* **Description:** Direct binary dumping and retrieval of data structures to disk, which provides faster read/write times compared to CSV or JSON parsers.
* **Technical Reference:** [cppreference: std::basic_ostream::write](https://en.cppreference.com/w/cpp/io/basic_ostream/write)
* **Code Application:** Located in `serializer.cpp` to write and read `StudentRecord` and `LogEntry` structures.
* **Snippet Example:**
  ```cpp
  // From serializer.cpp
  bool serialize_student(const std::string& filepath, const StudentRecord& student) {
      std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
      if (!file.is_open()) return false;
      file.write(reinterpret_cast<const char*>(&student), sizeof(StudentRecord));
      return file.good();
  }
  ```

### Chrono & C-Style Time Manipulation (`<chrono>`, `<ctime>`)
* **Description:** System clock queries and parsing methods used to calculate active curfews and output readable timestamps for the database log entries.
* **Technical Reference:** [cppreference: Date & Time Utilities](https://en.cppreference.com/w/cpp/chrono)
* **Code Application:** Located in `main.cpp` and `daily_log.cpp`.
* **Snippet Example:**
  ```cpp
  // From main.cpp
  #include <chrono>
  #include <ctime>
  
  std::string get_current_timestamp_string() {
      auto now = std::chrono::system_clock::now();
      std::time_t t = std::chrono::system_clock::to_time_t(now);
      char buf[25];
      std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
      return std::string(buf);
  }
  ```

---

## 4. UI Elements & Build Orchestration

### ANSI Escape Terminal Codes
* **Description:** Virtual terminal formatting parameters that supply custom interface colors and styling flags directly within command-line prints.
* **Technical Reference:** [Wikipedia: ANSI Escape Code - Colors](https://en.wikipedia.org/wiki/ANSI_escape_code#Colors)
* **Code Application:** Located in `main.cpp` for console themes.
* **Snippet Example:**
  ```cpp
  // From main.cpp
  #define BOLD "\033[1m"
  #define GREEN "\033[32m"
  #define RESET "\033[0m"
  
  void print_success(const std::string &msg) {
      std::cout << BOLD << GREEN << "  [SUCCESS] " << msg << RESET << "\n";
  }
  ```

### CMake Build System
* **Description:** The industry-standard toolchain configurations utilized to organize build environments and target compilations (shared libraries, binaries).
* **Official Documentation:** [CMake Guide & Documentation](https://cmake.org/documentation/)
* **Code Application:** Configured in `CMakeLists.txt`.
* **Snippet Example:**
  ```cmake
  # From CMakeLists.txt
  cmake_minimum_required(VERSION 3.12)
  project(GateEngine C CXX)
  
  set(CMAKE_CXX_STANDARD 17)
  add_library(gate_engine SHARED ${SRC_FILES})
  ```

---

## 5. Upcoming Security & Biometric Encryption Logic (Planned)

Biometric credentials are mathematically derived signatures of human physical characteristics. Unlike passwords, compromised biometric templates cannot be changed or reset, making the protection of stored fingerprint templates a critical requirement.

### 5.1 Why Fingerprint Templates Must Be Secured (Concepts & Examples)

A biometric fingerprint template is a digital representation of a person's physical finger. It does not store the full visual image of the fingerprint; instead, it extracts a **feature vector** (typically 512 bytes of coordinates and orientations of ridge endings and bifurcations, known as *minutiae points*).

If these templates are stored as plain-text binary files on disk (e.g., `26CSE001.fpt`), the system is vulnerable to two major categories of attacks:

---

#### Attack Vector 1: Reconstitution and Identity Theft
* **The Vulnerability:** An attacker who gains read access to the database directory can extract the plain 512-byte minutiae templates.
* **The Attack Scenario:** Although a template is not an image, it is a precise spatial map of the fingerprint. Modern reconstruction algorithms can reverse-engineer a minutiae map to generate a high-fidelity synthetic image of the original fingerprint. The attacker can print this synthetic image onto conductive paper or cast it into a silicone mold (dummy finger). This physical fake can then be used to log in at the gate or compromise the user’s personal biometric locks (like Touch ID on their phone or laptop).
* **The Impact:** Unlike a compromised password or roll number, a human finger cannot be replaced. The user's biometric security is permanently compromised across all systems.

---

#### Attack Vector 2: Replay Attack (Spoofing the Matcher)
* **The Vulnerability:** The C++ engine’s biometric identification function `fingerprint_match` takes a 512-byte array representing a live scan and matches it against templates.
* **The Attack Scenario:** An attacker intercepts the plain-text template file for a student. Instead of placing their physical finger on the scanner at the gate, they write a script that sends the stolen 512 bytes directly to the C++ shared library API or Python bridge. Since the matcher operates purely on template byte comparison, it resolves the identity as the victim student, logging them "IN" or "OUT" without physical presence.

---

### 5.2 What Encryption Actually Does (Step-by-Step Example)

Encryption uses a mathematical algorithm (cipher) and a secret key to transform readable data (**plaintext**) into unreadable scrambled noise (**ciphertext**). Only users possessing the secret key can decrypt the ciphertext back into plaintext.

Here is a step-by-step example of how encryption secures the 512-byte biometric template:

#### Step 1: Plaintext Template (In Memory/Raw Sensor Capture)
The scanner captures the finger and outputs a structured pattern of bytes:
```text
Plaintext:  [ 0x8A, 0xC4, 0x12, 0x00, 0xFF, 0x3E, 0x77, 0x9B, ... (512 bytes) ]
```
*   **Status:** High Risk. An observer can read these minutiae coordinates, run FNV-1a hashes to identify duplicates, or reconstruct the print geometry.

#### Step 2: The Cryptographic Mixing (The Cipher)
When writing this template to disk, the C++ engine feeds the plaintext into the symmetric cipher (e.g., AES-256):
$$\text{Ciphertext} = \text{Encrypt}(\text{Plaintext}, \text{Secret Key}, \text{Initialization Vector})$$
*   **The Secret Key:** A 32-byte master key securely stored outside the database directory (e.g., in a TPM/Secure Enclave or injected via environment configuration).
*   **The Initialization Vector (IV) / Salt:** A unique value derived from the student's `roll_number` (e.g., `26CSE001`). 

#### Step 3: Ciphertext Output (On Disk)
The cipher performs 14 rounds of byte substitution, row shifting, column mixing, and round key addition. The resulting file saved to disk looks like random, uncorrelated noise:
```text
Ciphertext: [ 0xD3, 0x8E, 0x1A, 0x92, 0x05, 0xFC, 0x47, 0xB1, ... (512 bytes) ]
```
*   **Status:** Secure. An attacker stealing this file gets only scrambled bytes. Without the Secret Key, brute-forcing AES-256 would take $2^{256}$ operations (longer than the age of the universe).
*   **The Power of the Salt:** If two students have identical templates (or the same student registers twice), using the `roll_number` as a salt ensures their encrypted files look completely different on disk. Attackers cannot compare files to see if the same finger was registered to multiple roll numbers.

#### Step 4: Secure Decryption during Scans
When the system needs to run a gate scan:
1. The student scans their finger (generating a **live plaintext template** in RAM).
2. The engine loads each candidate's **encrypted template** from disk.
3. The engine decrypts the candidate's template in volatile memory (RAM) only:
$$\text{Plaintext} = \text{Decrypt}(\text{Ciphertext}, \text{Secret Key}, \text{Initialization Vector})$$
4. The matching algorithm compares the templates in RAM. Once the match finishes, the plaintext templates are wiped from RAM, leaving only the encrypted data on disk.

---

### 5.3 Proposed Cryptographic Architecture
We plan to integrate a dependency-free symmetric encryption implementation in standard C++:
* **Algorithm Choice:** AES-256 (Advanced Encryption Standard) in CBC or GCM mode, or the ChaCha20 stream cipher, providing high-performance cryptography on low-resource hardware.
* **Salting Mechanics:** Combining a system-wide database key with a dynamic salt derived from the student's unique `roll_number` (e.g., FNV-1a hash of the roll number). This ensures identical fingerprints generate distinct ciphertexts on disk.
* **Separated Key Storage:** Ensuring encryption keys are isolated from the data directory and managed via environment parameters or secure hardware modules (like TPM/Secure Enclaves).

### 5.3 Cryptography Learning Resources
* **ISO/IEC 24745 Standard:** [Biometric Information Protection Guidelines](https://www.iso.org/standard/52946.html)
* **NIST Cryptographic Standards:** [NIST Block Cipher Modes of Operation](https://csrc.nist.gov/publications/detail/sp/800-38a/final)
* **Practical C++ Encryption Guide:** [Lightweight AES Implementation in C++ (Byte-Oriented)](https://github.com/kokke/tiny-AES-c)
* **Crypto101 Introduction:** [An Introductory Course on Cryptography](https://www.crypto101.io/)

