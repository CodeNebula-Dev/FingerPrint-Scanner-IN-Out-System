# Engine Upgrade Specifications: What We Are Changing in Version 2.0

> **Target Directory**: `cpp_engine_v2/`  
> **Base Version**: `cpp_engine/` / `Final_cpp_engine/`  
> **Document Purpose**: Complete technical breakdown of architectural changes, security diffs, and the modular encryption placeholder interface.

---

## 1. Executive Summary of Changes

Version 2.0 transforms our C++ biometric matching engine from a **plaintext baseline model** into a **Cancelable Encryption Standard Architecture**. 

While Engine Version 1.0 achieved ultra-fast lookup times ($< 0.5\,\text{ms}$), it relied on storing and comparing 512-byte raw fingerprint minutiae arrays in plaintext. Version 2.0 eliminates raw template exposure across storage and RAM while keeping the database CRUD structure, daily logging, and Python bridge API contract identical.

---

## 2. Comprehensive Change Matrix

| Component | Version 1.0 (Legacy Baseline) | Version 2.0 (Cancelable Architecture) | Rationale for Change |
| :--- | :--- | :--- | :--- |
| **Biometric Struct Field** | `uint8_t fingerprint_template[512]` (Raw minutiae) | `uint8_t encrypted_template[512]` (Transformed Payload) | Removes raw minutiae from binary disk storage and engine RAM. |
| **Cryptographic Layer** | Direct byte similarity calculation in `fingerprint.cpp` | Abstracted `crypto_placeholder.h` / `crypto_placeholder.cpp` interface | Decouples cryptographic transformation logic from core database and indexing logic. |
| **Enrollment Pipeline** | Writes raw 512-byte array directly to disk `.dat` file | Passes raw template through `crypto_enroll_transform()` before disk write | Ensures unencrypted biometric data never touches disk storage. |
| **Verification Pipeline** | Loads raw bytes into RAM and executes direct similarity score | Passes live scan through `crypto_match_evaluate()` in transformed space | Eliminates cold boot RAM scraping vulnerabilities. |
| **Template Protection** | None (Static raw template) | Cancelable / Revocable via key rotation (`crypto_rekey()`) | Fulfills ISO/IEC 24745 requirements (Irreversibility, Unlinkability, Renewability). |
| **API Contract** | C++ / Python C-ABI Function Signatures | Preserved Exactly (`fingerprint_match`, `fingerprint_enroll`) | Prevents breaking changes for the Python application bridge layer. |

---

## 3. Detailed Breakdown of the Modular Encryption Placeholder

To allow flexibility in deciding which specific Cancelable Encryption algorithm to deploy (e.g., BioHashing, Non-Invertible Matrix Projection, or PolyProtect), Version 2.0 introduces a dedicated abstraction interface: `include/crypto_placeholder.h`.

```
                        Modular Encryption Architecture

  ┌────────────────────────────────────────────────────────────────────────┐
  │                    CORE C++ ENGINE (master_db, indexer)                │
  └───────────────────────────────────┬────────────────────────────────────┘
                                      │
                                      ▼
  ┌────────────────────────────────────────────────────────────────────────┐
  │              MODULAR ENCRYPTION INTERFACE (crypto_placeholder.h)       │
  ├────────────────────────────────────────────────────────────────────────┤
  │ + crypto_enroll_transform(raw_in, length, enc_out, out_len)            │
  │ + crypto_match_evaluate(live_scan, live_len, stored_enc, threshold)   │
  │ + crypto_rekey(old_enc, new_enc, old_key, new_key)                     │
  └───────────────────────────────────┬────────────────────────────────────┘
                                      │
                  ┌───────────────────┼───────────────────┐
                  ▼                   ▼                   ▼
          ┌───────────────┐   ┌───────────────┐   ┌───────────────┐
          │  BioHashing   │   │ Matrix Proj.  │   │  PolyProtect  │
          │   Standard    │   │  Cancelable   │   │   Standard    │
          └───────────────┘   └───────────────┘   └───────────────┘
                               (Pluggable Slots)
```

### 3.1 Function Contracts in `crypto_placeholder.h`

1. **`crypto_enroll_transform`**:
   - **Input**: Raw 512-byte template captured from scanner.
   - **Output**: 512-byte transformed/encrypted template payload ready for binary disk storage.
   - **Role**: Applies non-invertible transformation during student registration.

2. **`crypto_match_evaluate`**:
   - **Input**: Incoming live scan payload and stored candidate encrypted template payload.
   - **Output**: `MatchScoreResult` struct containing `is_matched` (boolean) and `confidence_score` (float).
   - **Role**: Executes distance evaluation strictly in the transformed domain without decrypting back to raw minutiae.

3. **`crypto_rekey`**:
   - **Input**: Existing encrypted template, old transformation key, new transformation key.
   - **Output**: Newly transformed template payload under updated key $K'$.
   - **Role**: Implements ISO/IEC 24745 template renewability if keys are revoked.

---

## 4. Code Struct & API Signatures Diff

### 4.1 Struct Definition Changes (`include/engine.h`)

#### Version 1.0 (Legacy Baseline):
```cpp
struct StudentRecord {
    char roll_number[20];
    char name[100];
    char program[20];
    char batch[10];
    int  year;
    char phone_number[15];
    bool is_hosteller;
    uint8_t fingerprint_template[512]; // Vulnerability: Raw minutiae array
};
```

#### Version 2.0 (Security Hardened):
```cpp
struct StudentRecord {
    char roll_number[20];
    char name[100];
    char program[20];
    char batch[10];
    int  year;
    char phone_number[15];
    bool is_hosteller;
    uint8_t encrypted_template[512];   // Hardened: Transformed ciphertext payload
};
```

### 4.2 Matching Delegate Diff (`src/fingerprint.cpp`)

#### Version 1.0 (Direct Byte Match):
```cpp
// Legacy v1.0 logic directly comparing raw bytes in RAM
float compute_similarity(const uint8_t* t1, const uint8_t* t2, int len) {
    int matches = 0;
    for (int i = 0; i < len; i++) {
        if (t1[i] == t2[i]) matches++;
    }
    return (float)matches / len;
}
```

#### Version 2.0 (Delegating to Modular Interface):
```cpp
// v2.0 logic delegating evaluation to the pluggable crypto placeholder
MatchResult fingerprint_match(const uint8_t* live_scan, int scan_length) {
    // 1. Transform incoming live scan using active transformation key
    uint8_t live_transformed[512];
    crypto_enroll_transform(live_scan, scan_length, live_transformed, 512);

    // 2. Query Level-1 FNV-1a index on transformed payload hashes
    uint64_t live_hash = indexer_hash_template(live_transformed, 512);
    
    // 3. Delegate Level-2 evaluation to crypto_placeholder interface
    return crypto_match_evaluate(live_transformed, 512, candidate_record.encrypted_template, 0.75f);
}
```

---

## 5. Security & Memory Exposure Analysis

### Exposure Point 1: Storage at Rest
* **v1.0**: Disk files (`Student_data/2026_batch/*.dat`) contained unencrypted 512-byte minutiae. A physical disk extraction exposed all students' fingerprints permanently.
* **v2.0**: Disk files contain only transformed ciphertext arrays. Disk theft yields unreadable vectors that cannot be converted back to physical fingerprints.

### Exposure Point 2: Engine Memory Space (RAM)
* **v1.0**: During batch searches, unencrypted templates were read from disk directly into RAM buffers. Cold boot or memory-dump attacks allowed scraping raw minutiae.
* **v2.0**: Disk reads load transformed ciphertext arrays into RAM. The matching function operates strictly on transformed representations, ensuring raw features are never present in RAM.

---

## 6. Development Checklist for Version 2.0 Integration

- [x] Create directory `cpp_engine_v2/`
- [x] Write `README.md` detailing system specifications
- [x] Write `what_we_are_changing.md` documenting structural diffs
- [x] Add modular encryption placeholder interface (`include/crypto_placeholder.h` and `src/crypto_placeholder.cpp`)
- [x] Update C++ engine header files (`include/engine.h`, `include/indexer.h`, `include/serializer.h`)
- [x] Update database CRUD & serializer implementations (`src/master_db.cpp`, `src/serializer.cpp`, `src/indexer.cpp`)
- [x] Update matching pipeline in `src/fingerprint.cpp` to delegate to `crypto_placeholder`
- [x] Create CMake build configuration (`CMakeLists.txt`) and CLI harness (`src/main.cpp`)
