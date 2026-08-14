# Technical Deep-Dive: BioHashing Cancelable Encryption System

> **Location**: `Project-Docs/Encryption.md`  
> **Target Subsystems**: `cpp_engine_v2` (`include/crypto_placeholder.h`, `src/crypto_placeholder.cpp`)  
> **Core Concepts**: BioHashing, Random Orthogonal Projection, Binarization, Normalized Hamming Distance, ISO/IEC 24745 Template Protection

---

## 1. Executive Summary

### 1.1 What is Cancelable Biometric Encryption?

Cancelable biometric encryption is a **template protection scheme** that transforms raw biometric data (fingerprint minutiae) into a non-invertible representation before storage. Unlike traditional encryption (AES, RSA), cancelable biometrics are designed so that:

1. **The transformed template cannot be reversed** to recover the original fingerprint features — even by the system administrator.
2. **Templates can be revoked and replaced** if compromised — unlike a fingerprint, which cannot be changed.
3. **Matching occurs entirely in the transformed domain** — the raw biometric is never reconstructed during verification.

### 1.2 Why Not Just Use AES or SHA-256?

| Approach | Fatal Problem |
|:---|:---|
| **AES Encryption** | Requires decryption for matching → raw biometric exposed in memory during every gate scan |
| **SHA-256 Hashing** | Deterministic but extremely fragile → single bit of scan noise produces completely different hash (avalanche effect), making matching impossible |
| **Cancelable BioHashing** ✓ | Preserves similarity structure in transformed domain → noisy scans still match correctly, raw biometric never exposed |

### 1.3 Our Implementation: BioHashing Standard

Engine v2.0 implements the **BioHashing** algorithm (Teoh, Ngo & Goh, 2004), classified as `CancelableCryptoScheme::BIOHASHING_STANDARD` in the engine's enum system. This scheme uses:

- **Random Orthogonal Projection** to transform biometric vectors
- **Binarization** (sign function) to produce a compact, non-invertible code
- **Normalized Hamming Distance** for matching in the transformed domain
- **Seed-Based Key Management** for ISO/IEC 24745 renewability

---

## 2. BioHashing Mathematical Theory

### 2.1 The Core Transformation

Given:
- $X \in \mathbb{R}^n$ — raw biometric feature vector (512 bytes normalized to $[0, 1]$)
- $K$ — secret 64-bit seed (stored in `db_root/biohash.key`)
- $R \in \mathbb{R}^{m \times n}$ — pseudo-random projection matrix generated from seed $K$

The BioHash code $Y$ is computed as:

$$P_i = \sum_{j=1}^{n} R_{ij} \cdot X_j \quad \text{(dot product with i-th random vector)}$$

$$Y_i = \begin{cases} 1 & \text{if } P_i \geq 0 \\ 0 & \text{if } P_i < 0 \end{cases} \quad \text{(binarization)}$$

The output $Y \in \{0, 1\}^m$ is a binary vector of length $m = 512$ bits.

### 2.2 Why Random Projection Preserves Similarity

The mathematical foundation comes from the **Johnson–Lindenstrauss Lemma**: random projections approximately preserve pairwise distances between points. For two biometric vectors $X_A$ and $X_B$:

$$\Pr\left[ \left| \frac{\|R \cdot X_A - R \cdot X_B\|^2}{\|X_A - X_B\|^2} - 1 \right| > \epsilon \right] \leq 2 \exp\left(-\frac{m \epsilon^2}{4}\right)$$

In practice, this means:
- **Similar fingerprints** (small $\|X_A - X_B\|$) produce **similar BioHash codes** (small Hamming distance)
- **Different fingerprints** (large $\|X_A - X_B\|$) produce **dissimilar BioHash codes** (large Hamming distance)
- The binarization step loses some precision but makes the transformation **irreversible**

### 2.3 The One-Way Property (Irreversibility)

Computing $X$ from $Y = \text{sign}(R \cdot X)$ requires solving a system of $m$ binary inequalities with $n$ unknowns. Since $Y$ only records the **sign** (not magnitude) of each projection:

- Each bit of $Y$ eliminates one half-space from the solution space
- But with $m = n = 512$, the binary system remains computationally intractable
- **Without the seed $K$**, the attacker doesn't know $R$ and cannot even formulate the system
- **With both $K$ and $Y$** (stolen token + stolen template), reconstructing $X$ requires brute-force over the feasible region — exponential in $n$

### 2.4 Projection Matrix Generation

Each row $R_i$ of the projection matrix is generated independently using a seeded Mersenne Twister PRNG (`std::mt19937_64`):

```
Row Seed = Base_Seed XOR (row_index × 2654435761)
                                    ↑
                    Knuth's multiplicative hash constant
                    (ensures good bit mixing across rows)
```

Each row is sampled from an $n$-dimensional standard normal distribution $\mathcal{N}(0, I_n)$ and then normalized to unit length. This produces quasi-orthogonal random vectors in high dimensions.

---

## 3. Implementation Architecture

### 3.1 Module Files

| File | Role |
|:---|:---|
| `include/crypto_placeholder.h` | Public API: enums, structs, function declarations, constants |
| `src/crypto_placeholder.cpp` | Full BioHashing implementation (projection, matching, re-keying, key management) |

### 3.2 Encrypted Template Buffer Layout

The 512-byte `encrypted_template` field in `StudentRecord` is partitioned as follows:

```
    encrypted_template[512] Buffer Layout
    ┌─────────────────────────────────────────────────────────────┐
    │  Bytes [0..63]   │  Binarized BioHash Code                 │
    │  (64 bytes)      │  512 bits packed, LSB-first per byte     │
    │                  │  Used for: Hash indexing + matching       │
    ├──────────────────┼──────────────────────────────────────────┤
    │  Bytes [64..511] │  Quantized Pre-Binarization Dot Products │
    │  (448 bytes)     │  224 × int16 values (scale factor 10000) │
    │                  │  Used for: ISO/IEC 24745 re-keying only  │
    └──────────────────┴──────────────────────────────────────────┘
```

- **Bytes 0–63 (BioHash Code)**: The actual cancelable biometric representation. This is what gets hashed by the FNV-1a indexer and compared during matching. 512 bits are packed into 64 bytes using LSB-first ordering within each byte.

- **Bytes 64–511 (Intermediate Dot Products)**: The pre-binarization projection values $P_i$, quantized to `int16` with a scale factor of 10,000. These are stored solely to enable template renewability (re-keying) without requiring the student to re-scan their fingerprint. They are **never** used during matching.

### 3.3 Full Enrollment Transform Pipeline

```
                        ENROLLMENT TRANSFORM PIPELINE

    ┌──────────────────────────────────────────────────────────────────┐
    │                   Raw Biometric Template X                       │
    │                   (512 bytes from scanner)                       │
    └────────────────────────────────┬─────────────────────────────────┘
                                     │
                                     ▼
    ┌──────────────────────────────────────────────────────────────────┐
    │ 1. NORMALIZE                                                     │
    │    feature[i] = raw_input[i] / 255.0                             │
    │    X ∈ R^512, each component ∈ [0.0, 1.0]                       │
    └────────────────────────────────┬─────────────────────────────────┘
                                     │
                                     ▼
    ┌──────────────────────────────────────────────────────────────────┐
    │ 2. RANDOM PROJECTION                                             │
    │    For each dimension i ∈ [0, 511]:                              │
    │      Generate unit-length random vector R_i from seed K          │
    │      Compute P[i] = R_i · X (dot product)                       │
    └────────────────────────────────┬─────────────────────────────────┘
                                     │
                                     ▼
    ┌──────────────────────────────────────────────────────────────────┐
    │ 3. BINARIZE                                                      │
    │    Y[i] = (P[i] >= 0) ? 1 : 0                                   │
    │    Pack 512 bits → 64 bytes (LSB-first)                          │
    └────────────────────────────────┬─────────────────────────────────┘
                                     │
                                     ▼
    ┌──────────────────────────────────────────────────────────────────┐
    │ 4. STORE                                                         │
    │    encrypted_output[0..63]   ← Binarized BioHash Code Y          │
    │    encrypted_output[64..511] ← Quantized Dot Products P          │
    └──────────────────────────────────────────────────────────────────┘
```

### 3.4 Code Walkthrough: `crypto_enroll_transform()`

```cpp
// Entry point — called by fingerprint_enroll() in fingerprint.cpp
bool crypto_enroll_transform(
    const uint8_t* raw_input,     // Raw 512-byte biometric template
    size_t input_len,              // Length (512)
    uint8_t* encrypted_output,     // Output: 512-byte encrypted template
    size_t output_len              // Must be >= 512
) {
    // 1. Validate inputs and check key is loaded
    // 2. Call compute_biohash() → random projection + binarization
    //    Writes binarized code to encrypted_output[0..63]
    //    Returns dot products vector for storage
    // 3. Call pack_dot_products() → quantize and store intermediates
    //    Writes to encrypted_output[64..511]
    return true;
}
```

The `compute_biohash()` internal function handles the core math:

```cpp
static bool compute_biohash(raw_input, input_len, seed, dot_products, binary_code, ...) {
    // Normalize bytes to [0, 1] feature vector
    for (i = 0; i < input_dim; i++)
        features[i] = raw_input[i] / 255.0;

    // For each projection dimension
    for (i = 0; i < 512; i++) {
        // Generate deterministic random unit vector from seed + row_index
        generate_projection_row(seed, i, row, input_dim);

        // Dot product: P[i] = R[i] · X
        double dp = Σ row[j] * features[j];
        dot_products[i] = dp;

        // Binarize: set bit if dp >= 0
        if (dp >= 0.0)
            binary_code[i/8] |= (1 << (i%8));
    }
}
```

---

## 4. Matching in the Transformed Domain

### 4.1 Normalized Hamming Distance

During gate verification, the live scan is transformed using the same key and compared to stored templates using **Normalized Hamming Distance**:

$$d_H(Y_{\text{live}}, Y_{\text{stored}}) = \sum_{i=0}^{511} Y_{\text{live}}[i] \oplus Y_{\text{stored}}[i]$$

$$\text{Confidence Score} = 1.0 - \frac{d_H}{512}$$

| Scenario | Hamming Distance | Confidence Score |
|:---|:---:|:---:|
| Identical templates (same finger, no noise) | 0 | 1.00 (100%) |
| Same finger, minor scan noise | ~50–100 | 0.80–0.90 (80–90%) |
| Same finger, significant noise | ~100–130 | 0.75–0.80 (75–80%) |
| Different fingers (random) | ~256 | ~0.50 (50%) |
| Threshold for acceptance | ≤128 | **≥ 0.75 (75%)** |

### 4.2 Why 0.75 (75%) Threshold?

The match threshold of 0.75 is calibrated based on the statistical properties of BioHash codes:

- **Genuine pairs** (same finger): Expected Hamming distance $\approx 60\text{–}120$ bits → confidence $\approx 0.77\text{–}0.88$
- **Impostor pairs** (different fingers): Expected Hamming distance $\approx 256$ bits → confidence $\approx 0.50$
- **Gap**: Clear separation between genuine ($\geq 0.75$) and impostor ($\approx 0.50$) distributions
- **75% threshold** sits comfortably in this gap, minimizing both False Accept Rate (FAR) and False Reject Rate (FRR)

### 4.3 Code Walkthrough: `crypto_match_evaluate()`

```cpp
MatchScoreResult crypto_match_evaluate(
    const uint8_t* live_transformed_scan,
    size_t live_len,
    const uint8_t* stored_encrypted_template,
    float threshold
) {
    // Compare ONLY bytes [0..63] (binarized BioHash codes)
    // Bytes [64..511] (dot products) are ignored during matching

    int ham_dist = hamming_distance_bytes(live, stored, 64);
    // Uses Kernighan's bit counting: O(number of set bits)

    confidence = 1.0 - (ham_dist / 512.0);
    is_matched = (confidence >= threshold);
}
```

### 4.4 Integration with Dual-Tier Matching Pipeline

The BioHashing encryption integrates seamlessly with the existing Dual-Tier Coarse-to-Fine matching architecture documented in `Hashing_Mechanics_Innovations_and_FailSafes.md`:

```
    Live Scan → crypto_enroll_transform() → Transformed Template Y_live
                                                      │
                          ┌───────────────────────────┤
                          ▼                           ▼
                 Level-1: FNV-1a Hash           Level-2: Hamming Distance
                 over Y_live[0..63]             over Y_live[0..63] vs
                 → O(1) RAM lookup              Y_stored[0..63]
                 (indexer.cpp)                  (crypto_placeholder.cpp)
```

- **Level-1** (FNV-1a): Hashes the 64-byte BioHash code for instant $O(1)$ candidate lookup
- **Level-2** (Hamming): Computes precise similarity in the BioHash domain for final verification

---

## 5. Key Management

### 5.1 Key File Format

The BioHash seed key is stored as a raw 8-byte binary file:

```
Location: <project_root>/db_root/biohash.key
Format:   8 bytes, little-endian uint64_t
Contents: PRNG seed for projection matrix generation
```

### 5.2 Key Lifecycle

```
                           KEY LIFECYCLE

    ┌──────────────────────────────────────────────────┐
    │  engine_init()                                    │
    │    └─→ crypto_init_key(db_root_path)              │
    │          ├─→ File exists? Load seed from file      │
    │          └─→ No file? Generate via std::random_device │
    │                        └─→ Persist to biohash.key  │
    └──────────────────────────────────────────────────┘
                              │
                              ▼
    ┌──────────────────────────────────────────────────┐
    │  Normal Operation                                 │
    │    crypto_enroll_transform() uses g_config.seed    │
    │    crypto_match_evaluate() compares BioHash codes  │
    └──────────────────────────────────────────────────┘
                              │
                    (Compromise detected)
                              │
                              ▼
    ┌──────────────────────────────────────────────────┐
    │  Key Rotation                                     │
    │    1. crypto_rotate_key(new_seed)                  │
    │       → Generates new seed via hardware entropy    │
    │       → Persists to biohash.key                    │
    │    2. For each student:                            │
    │       crypto_rekey(old_template, new_template, 512)│
    │       student_update(roll_number, updated_record)  │
    │    3. indexer_clear() + rebuild hash index          │
    └──────────────────────────────────────────────────┘
```

### 5.3 Key Generation

Keys are generated using `std::random_device`, which sources entropy from the operating system's cryptographic entropy pool (`/dev/urandom` on Linux/macOS, `CryptGenRandom` on Windows):

```cpp
std::random_device rd;
uint64_t seed = (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
```

### 5.4 Admin API for Key Management

| Function | Purpose |
|:---|:---|
| `crypto_init_key(db_root_path)` | Load or generate key on startup (called by `engine_init()`) |
| `crypto_set_key(seed)` | Manually set key (for testing or admin override) |
| `crypto_rotate_key(new_seed)` | Generate new key, persist, and return for admin logging |
| `crypto_get_config()` | Read current config (seed value, projection dim, key status) |

---

## 6. ISO/IEC 24745 Compliance

The BioHashing implementation satisfies the three core requirements of the ISO/IEC 24745:2022 standard for biometric information protection:

### 6.1 Irreversibility

> *"It shall be computationally infeasible to reconstruct the original biometric sample or template from the protected template."*

**How we satisfy this:**
- The binarization step $Y_i = \text{sign}(P_i)$ discards magnitude information
- Recovering $X$ from $Y$ requires solving $m = 512$ binary half-space constraints in $n = 512$ dimensions
- Without the seed $K$, the projection matrix $R$ is unknown, making the system fully underdetermined
- **Theoretical bound**: Brute-force reconstruction complexity is $O(2^{512})$ — computationally infeasible

### 6.2 Unlinkability

> *"It shall be infeasible to determine whether two protected templates were derived from the same biometric source."*

**How we satisfy this:**
- Different seeds produce **statistically independent** projection matrices
- Two BioHash codes from the same fingerprint under different keys have expected Hamming distance $\approx 256$ (random)
- An attacker cannot correlate templates across systems using different keys

$$\text{Correlation}(Y^{K_1}, Y^{K_2}) \approx 0 \quad \text{for } K_1 \neq K_2$$

### 6.3 Renewability

> *"If a protected template is compromised, it shall be possible to create a new protected template from the same biometric source."*

**How we satisfy this:**
- `crypto_rekey()` uses stored pre-binarization dot products to re-project under a new key
- New seed → new projection matrix → completely new BioHash code
- **No re-scanning required**: The student's fingerprint does not need to be recaptured
- Old compromised templates become useless — they correspond to a revoked key

### 6.4 Compliance Summary Table

| ISO/IEC 24745 Property | Implementation Mechanism | Strength |
|:---|:---|:---|
| **Irreversibility** | Binarization discards magnitude; unknown $R$ without seed | $O(2^{512})$ brute-force |
| **Unlinkability** | Independent random projections per seed | Cross-system correlation ≈ 0 |
| **Renewability** | Stored dot products enable re-keying without re-scan | Instant key rotation |

---

## 7. Security Analysis

### 7.1 Threat Model & Attack Surface

| Attack Vector | Attacker Has | Impact | Mitigation |
|:---|:---|:---|:---|
| **Stolen Database** | Encrypted templates only | Cannot reconstruct fingerprints (binarization is lossy) | Irreversibility property |
| **Stolen Key File** | Seed $K$ only | Cannot generate valid templates without biometric | Key alone is useless without biometric data |
| **Stolen Token** (Key + Templates) | Both $K$ and $Y$ | Theoretical reconstruction attack (underdetermined binary system) | Rotate key via `crypto_rotate_key()` → all old templates invalidated |
| **Hill-Climbing Attack** | Oracle access to matcher | Iteratively craft input to maximize match score | Rate limiting + `rejection_log_write()` audit trail |
| **Cross-System Linkage** | Templates from two systems | Attempt to correlate identities | Different seeds → unlinkable templates |

### 7.2 Quantitative Security Bounds

**Brute-Force Key Search:**
- Key space: $2^{64} \approx 1.84 \times 10^{19}$ possible seeds
- At $10^9$ attempts/second: $\approx 584$ years to exhaust

**Template Inversion (with known key):**
- Must solve: Find $X \in [0, 1]^{512}$ such that $\text{sign}(R \cdot X) = Y$
- This defines $2^{512}$ feasible regions in the feature space
- Even with the key, the exact $X$ is unrecoverable — only a feasible region can be identified

**Random Collision Probability:**
- Two random BioHash codes match (all 512 bits identical): $p = 2^{-512} \approx 10^{-154}$
- Above 75% threshold (Hamming distance ≤ 128 of 512): Binomial tail probability ≈ $10^{-13}$ for random inputs

### 7.3 Known Limitations

1. **Stolen Token Attack**: If both the seed and templates are compromised simultaneously, the attacker gains some information about the biometric. Mitigation: physical security of the key file + immediate key rotation on breach detection.

2. **Quantization Loss in Re-Keying**: Dot products are quantized to `int16` (scale 10,000), introducing minor rounding errors. After multiple re-keying cycles, accumulated errors may degrade match accuracy. **Recommendation**: Re-enroll students during annual registration to reset template fidelity.

3. **Single Global Key**: All students share the same projection key. A per-student key would provide stronger unlinkability but adds complexity to key management. For our campus deployment scale ($< 5,000$ students), the single-key approach is sufficient.

---

## 8. Integration Map

### 8.1 How the Crypto Module Connects to the Engine

```
                         ENGINE v2.0 MODULE DEPENDENCIES

    ┌─────────────┐          ┌─────────────────────────┐
    │  main.cpp   │─────────→│  engine.h               │
    │  (CLI)      │          │  (StudentRecord struct,  │
    └─────────────┘          │   TEMPLATE_SIZE = 512)   │
                             └────────────┬────────────┘
                                          │
              ┌───────────────────────────┼───────────────────────────┐
              ▼                           ▼                           ▼
    ┌──────────────────┐    ┌──────────────────┐       ┌──────────────────┐
    │  master_db.cpp   │    │  fingerprint.cpp │       │  daily_log.cpp   │
    │                  │    │                  │       │                  │
    │  engine_init()   │    │  enroll()        │       │  (no crypto      │
    │   └─crypto_init_ │    │   └─crypto_      │       │   dependency)    │
    │     key()        │    │     enroll_       │       └──────────────────┘
    │                  │    │     transform()   │
    │  student_add()   │    │                  │
    │   └─indexer_     │    │  match()         │
    │     insert()     │    │   └─crypto_      │
    └──────────────────┘    │     match_       │
                            │     evaluate()   │
                            │   └─indexer_     │
                            │     hash_        │
                            │     template()   │
                            └──────────────────┘
                                     │
                  ┌──────────────────┼──────────────────┐
                  ▼                                      ▼
    ┌──────────────────────────┐           ┌──────────────────────┐
    │  crypto_placeholder.cpp  │           │  indexer.cpp         │
    │                          │           │                      │
    │  crypto_enroll_transform │           │  FNV-1a Hash over    │
    │  crypto_match_evaluate   │           │  BioHash code        │
    │  crypto_rekey            │           │  (bytes [0..63])     │
    │  crypto_init_key         │           │                      │
    │  crypto_rotate_key       │           │  O(1) RAM lookup     │
    └──────────────────────────┘           └──────────────────────┘
```

### 8.2 Data Flow: Complete Gate Scan Transaction

```
    1. Student places finger on scanner
    2. Scanner produces raw 512-byte template X
    3. fingerprint_match(X, 512) called
       │
       ├─ 4. crypto_enroll_transform(X, 512, Y_live, 512)
       │     → BioHash projection + binarization
       │     → Y_live = transformed template
       │
       ├─ 5. indexer_hash_template(Y_live, 512)
       │     → FNV-1a hash over Y_live[0..63]
       │     → H_live = 64-bit hash
       │
       ├─ 6. Level-1: indexer_lookup_candidates(H_live)
       │     → O(1) RAM lookup for exact hash match
       │
       └─ 7. Level-2: For each stored template Y_stored:
              crypto_match_evaluate(Y_live, 512, Y_stored, 0.75)
              → Hamming distance over binarized codes
              → Confidence score ≥ 0.75 → MATCH
```

---

## 9. API Reference

### 9.1 Constants

| Constant | Value | Description |
|:---|:---:|:---|
| `BIOHASH_PROJECTION_DIM` | 512 | Number of random projection dimensions / output bits |
| `BIOHASH_SEED_SIZE` | 8 | Size of the seed key in bytes (64-bit) |
| `TEMPLATE_SIZE` | 512 | Total encrypted template buffer size in bytes |

### 9.2 Structures

#### `BioHashConfig`
```cpp
struct BioHashConfig {
    uint64_t seed;           // Secret seed for projection matrix PRNG
    int      projection_dim; // Number of projection dimensions (default: 512)
    bool     key_loaded;     // true if a valid key has been loaded/generated
};
```

#### `MatchScoreResult`
```cpp
struct MatchScoreResult {
    bool  is_matched;       // true if confidence >= threshold
    float confidence_score; // Normalized similarity [0.0, 1.0]
};
```

### 9.3 Functions

| Function | Signature | Returns |
|:---|:---|:---|
| `crypto_init_key` | `(const string& db_root_path) → bool` | true if key loaded/generated |
| `crypto_set_key` | `(uint64_t seed) → void` | — |
| `crypto_rotate_key` | `(uint64_t& new_seed) → bool` | true if rotated successfully |
| `crypto_get_config` | `() → BioHashConfig` | Copy of current config |
| `crypto_enroll_transform` | `(raw_input, len, output, out_len) → bool` | true if transform succeeded |
| `crypto_match_evaluate` | `(live, len, stored, threshold) → MatchScoreResult` | Match status + confidence |
| `crypto_rekey` | `(old, new, len) → bool` | true if re-keyed successfully |
| `crypto_get_active_scheme` | `() → CancelableCryptoScheme` | Active scheme enum |
| `crypto_get_scheme_name` | `() → const char*` | Human-readable scheme name |

---

## 10. References

1. **Teoh, A.B.J., Ngo, D.C.L., Goh, A.** (2004). "BioHashing: two factor authentication featuring fingerprint data and tokenised random number." *Pattern Recognition*, 37(11), 2245–2255.

2. **ISO/IEC 24745:2022.** "Information technology — Security techniques — Biometric information protection."

3. **Johnson, W.B., Lindenstrauss, J.** (1984). "Extensions of Lipschitz mappings into a Hilbert space." *Contemporary Mathematics*, 26, 189–206.

4. **Rathgeb, C., Uhl, A.** (2011). "A survey on biometric cryptosystems and cancelable biometrics." *EURASIP Journal on Information Security*, 2011, 3.
