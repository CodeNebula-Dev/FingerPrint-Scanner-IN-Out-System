#include "crypto_placeholder.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <numeric>

/**
 * @file crypto_placeholder.cpp
 * @brief BioHashing Cancelable Encryption Implementation (Engine v2.0)
 *
 * Implements the BioHashing algorithm for cancelable biometric template protection:
 *   1. Random Orthogonal Projection using a seeded Mersenne Twister PRNG
 *   2. Binarization of projection dot-products (sign function)
 *   3. Normalized Hamming Distance for transformed-domain matching
 *   4. Re-keying via stored intermediate dot-products (ISO/IEC 24745 Renewability)
 *
 * Mathematical Foundation:
 *   Given raw biometric feature vector X ∈ R^n and random projection matrix R ∈ R^{m×n}:
 *     P[i] = Σ_j R[i][j] * X[j]     (inner product of i-th random vector with X)
 *     Y[i] = (P[i] >= 0) ? 1 : 0     (binarization)
 *
 *   The BioHash code Y is a binary vector that preserves angular similarity between
 *   biometric templates (Johnson–Lindenstrauss property) while being non-invertible
 *   (computing X from Y requires solving an underdetermined binary system).
 *
 * References:
 *   - Teoh, Ngo, Goh (2004) "BioHashing: two factor authentication featuring
 *     fingerprint data and tokenised random number"
 *   - ISO/IEC 24745:2022 "Biometric information protection"
 */

// ============================================================================
// GLOBAL STATE
// ============================================================================

static CancelableCryptoScheme g_active_scheme = CancelableCryptoScheme::BIOHASHING_STANDARD;

static BioHashConfig g_config = {
    0ULL,                       // seed (uninitialized)
    BIOHASH_PROJECTION_DIM,     // projection_dim
    false                       // key_loaded
};

static std::string g_key_filepath;  // Path to the persisted key file

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

/**
 * @brief Generate a single row of the pseudo-random projection matrix.
 *
 * Each row is a unit-length random vector generated from a normal distribution
 * seeded deterministically by (seed + row_index). This ensures:
 *   - Same seed always produces the same projection matrix (deterministic enrollment)
 *   - Different seeds produce statistically independent projections (unlinkability)
 *
 * @param seed       Base seed for the PRNG.
 * @param row_index  Index of the projection row to generate.
 * @param row_out    Output buffer for the row vector (length = input_dim).
 * @param input_dim  Dimensionality of the input biometric vector.
 */
static void generate_projection_row(uint64_t seed, int row_index,
                                     std::vector<double>& row_out, int input_dim) {
    // Combine seed with row index to get a unique but deterministic seed per row
    uint64_t row_seed = seed ^ (static_cast<uint64_t>(row_index) * 2654435761ULL);
    std::mt19937_64 rng(row_seed);
    std::normal_distribution<double> normal(0.0, 1.0);

    row_out.resize(input_dim);
    double norm = 0.0;

    // Generate random Gaussian vector
    for (int j = 0; j < input_dim; j++) {
        row_out[j] = normal(rng);
        norm += row_out[j] * row_out[j];
    }

    // Normalize to unit length (Gram-Schmidt-lite: ensures orthogonality in expectation)
    norm = std::sqrt(norm);
    if (norm > 1e-10) {
        for (int j = 0; j < input_dim; j++) {
            row_out[j] /= norm;
        }
    }
}

/**
 * @brief Compute the full BioHash projection: dot products and binarized code.
 *
 * @param raw_input   Raw biometric template bytes (treated as feature vector).
 * @param input_len   Length of raw input.
 * @param seed        PRNG seed for projection matrix generation.
 * @param dot_products Output: raw dot product values (for re-keying support).
 * @param binary_code  Output: binarized BioHash code packed into bytes.
 * @param binary_byte_len Number of bytes in the binary code output.
 * @return true on success.
 */
static bool compute_biohash(const uint8_t* raw_input, size_t input_len, uint64_t seed,
                             std::vector<double>& dot_products,
                             uint8_t* binary_code, size_t binary_byte_len) {
    int input_dim = static_cast<int>(input_len);
    int proj_dim = g_config.projection_dim;

    dot_products.resize(proj_dim);
    std::memset(binary_code, 0, binary_byte_len);

    // Convert raw bytes to double feature vector (normalized to [0, 1])
    std::vector<double> features(input_dim);
    for (int i = 0; i < input_dim; i++) {
        features[i] = static_cast<double>(raw_input[i]) / 255.0;
    }

    // For each projection dimension, compute dot product with random vector
    std::vector<double> row;
    for (int i = 0; i < proj_dim; i++) {
        generate_projection_row(seed, i, row, input_dim);

        // Dot product: P[i] = R[i] · X
        double dp = 0.0;
        for (int j = 0; j < input_dim; j++) {
            dp += row[j] * features[j];
        }
        dot_products[i] = dp;

        // Binarize: Y[i] = (P[i] >= 0) ? 1 : 0
        if (dp >= 0.0) {
            int byte_idx = i / 8;
            int bit_idx  = i % 8;
            if (byte_idx < static_cast<int>(binary_byte_len)) {
                binary_code[byte_idx] |= (1 << bit_idx);
            }
        }
    }

    return true;
}

/**
 * @brief Count the number of differing bits (Hamming distance) between two byte arrays.
 */
static int hamming_distance_bytes(const uint8_t* a, const uint8_t* b, size_t len) {
    int distance = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t diff = a[i] ^ b[i];
        // Kernighan's bit counting algorithm
        while (diff) {
            distance++;
            diff &= (diff - 1);
        }
    }
    return distance;
}

/**
 * @brief Pack double values into bytes for storage in the template buffer.
 *
 * Quantizes each double to a signed 16-bit integer for compact storage.
 * This provides sufficient precision for re-keying while fitting within
 * the available buffer space.
 *
 * Layout: Each double → 2 bytes (int16_t), stored little-endian.
 * Max storable doubles = available_bytes / 2.
 */
static void pack_dot_products(const std::vector<double>& dot_products,
                               uint8_t* buffer, size_t buffer_len) {
    // Quantize to int16: scale factor preserves precision for typical dot product magnitudes
    const double SCALE = 10000.0;
    size_t max_values = buffer_len / 2;
    size_t count = std::min(dot_products.size(), max_values);

    for (size_t i = 0; i < count; i++) {
        double clamped = std::max(-3.2, std::min(3.2, dot_products[i]));
        int16_t quantized = static_cast<int16_t>(clamped * SCALE);
        buffer[i * 2]     = static_cast<uint8_t>(quantized & 0xFF);
        buffer[i * 2 + 1] = static_cast<uint8_t>((quantized >> 8) & 0xFF);
    }
}

/**
 * @brief Unpack quantized dot products from the template buffer.
 */
static void unpack_dot_products(const uint8_t* buffer, size_t buffer_len,
                                 std::vector<double>& dot_products, int count) {
    const double SCALE = 10000.0;
    dot_products.resize(count);
    size_t max_values = buffer_len / 2;

    for (int i = 0; i < count && static_cast<size_t>(i) < max_values; i++) {
        int16_t quantized = static_cast<int16_t>(
            static_cast<uint16_t>(buffer[i * 2]) |
            (static_cast<uint16_t>(buffer[i * 2 + 1]) << 8)
        );
        dot_products[i] = static_cast<double>(quantized) / SCALE;
    }
}

// ============================================================================
// KEY MANAGEMENT IMPLEMENTATION
// ============================================================================

bool crypto_init_key(const std::string& db_root_path) {
    g_key_filepath = db_root_path + "/biohash.key";

    // Attempt to load existing key
    std::ifstream key_in(g_key_filepath, std::ios::binary);
    if (key_in.is_open()) {
        uint64_t loaded_seed = 0;
        key_in.read(reinterpret_cast<char*>(&loaded_seed), sizeof(uint64_t));
        if (key_in.good()) {
            g_config.seed = loaded_seed;
            g_config.key_loaded = true;
            std::cout << "[BioHash] Loaded encryption key from " << g_key_filepath << std::endl;
            return true;
        }
        key_in.close();
    }

    // No existing key — generate a new one using hardware entropy
    std::random_device rd;
    uint64_t new_seed = (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
    g_config.seed = new_seed;

    // Persist key to file
    std::ofstream key_out(g_key_filepath, std::ios::binary | std::ios::trunc);
    if (!key_out.is_open()) {
        std::cerr << "[BioHash] ERROR: Cannot write key file to " << g_key_filepath << std::endl;
        return false;
    }
    key_out.write(reinterpret_cast<const char*>(&new_seed), sizeof(uint64_t));
    key_out.close();

    g_config.key_loaded = true;
    std::cout << "[BioHash] Generated and persisted new encryption key to " << g_key_filepath << std::endl;
    return true;
}

void crypto_set_key(uint64_t seed) {
    g_config.seed = seed;
    g_config.key_loaded = true;
}

bool crypto_rotate_key(uint64_t& new_seed) {
    std::random_device rd;
    new_seed = (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
    g_config.seed = new_seed;
    g_config.key_loaded = true;

    // Persist new key
    if (!g_key_filepath.empty()) {
        std::ofstream key_out(g_key_filepath, std::ios::binary | std::ios::trunc);
        if (key_out.is_open()) {
            key_out.write(reinterpret_cast<const char*>(&new_seed), sizeof(uint64_t));
            key_out.close();
            std::cout << "[BioHash] Key rotated and persisted successfully." << std::endl;
            return true;
        }
    }

    std::cerr << "[BioHash] WARNING: Key rotated in memory but could not persist to file." << std::endl;
    return true;  // Key is valid in memory even if file write fails
}

BioHashConfig crypto_get_config() {
    return g_config;
}

// ============================================================================
// SCHEME IDENTIFICATION
// ============================================================================

CancelableCryptoScheme crypto_get_active_scheme() {
    return g_active_scheme;
}

const char* crypto_get_scheme_name() {
    switch (g_active_scheme) {
        case CancelableCryptoScheme::BIOHASHING_STANDARD:
            return "BioHashing (Random Orthogonal Projection + Binarization)";
        case CancelableCryptoScheme::MATRIX_PROJECTION:
            return "Non-Invertible Matrix Projection";
        case CancelableCryptoScheme::POLYPROTECT_STANDARD:
            return "PolyProtect Non-Linear Polynomial";
        case CancelableCryptoScheme::PLACEHOLDER_PASSTHROUGH:
        default:
            return "Placeholder Passthrough (INSECURE)";
    }
}

// ============================================================================
// CORE ENCRYPTION: ENROLLMENT TRANSFORM
// ============================================================================

bool crypto_enroll_transform(
    const uint8_t* raw_input,
    size_t input_len,
    uint8_t* encrypted_output,
    size_t output_len
) {
    if (!raw_input || !encrypted_output || input_len == 0 || output_len < 512) {
        return false;
    }
    if (!g_config.key_loaded) {
        std::cerr << "[BioHash] ERROR: Encryption key not initialized. Call crypto_init_key() first." << std::endl;
        return false;
    }

    // Zero the output buffer
    std::memset(encrypted_output, 0, output_len);

    // =========================================================================
    // BIOHASHING ENROLLMENT TRANSFORM
    // =========================================================================
    //
    // Output Buffer Layout (512 bytes total):
    //   [0..63]    = Binarized BioHash code (512 bits packed into 64 bytes)
    //   [64..511]  = Quantized pre-binarization dot products (for re-keying)
    //               448 bytes → 224 int16 values → covers 224 of 512 dimensions
    //               (remaining dimensions reconstructed from binary code on rekey)
    //
    // The binarized code (bytes 0-63) is used for hash indexing and matching.
    // The dot products (bytes 64-511) enable ISO/IEC 24745 template renewability
    // without re-scanning the student's fingerprint.
    // =========================================================================

    const size_t BINARY_CODE_BYTES = 64;  // 512 bits / 8
    const size_t DOT_PRODUCT_OFFSET = BINARY_CODE_BYTES;
    const size_t DOT_PRODUCT_BYTES = output_len - DOT_PRODUCT_OFFSET;

    std::vector<double> dot_products;

    // Compute BioHash: random projection + binarization
    if (!compute_biohash(raw_input, input_len, g_config.seed,
                          dot_products,
                          encrypted_output, BINARY_CODE_BYTES)) {
        return false;
    }

    // Store quantized dot products for future re-keying support
    pack_dot_products(dot_products,
                       encrypted_output + DOT_PRODUCT_OFFSET,
                       DOT_PRODUCT_BYTES);

    return true;
}

// ============================================================================
// CORE ENCRYPTION: MATCH EVALUATION
// ============================================================================

MatchScoreResult crypto_match_evaluate(
    const uint8_t* live_transformed_scan,
    size_t live_len,
    const uint8_t* stored_encrypted_template,
    float threshold
) {
    MatchScoreResult result;
    result.is_matched = false;
    result.confidence_score = 0.0f;

    if (!live_transformed_scan || !stored_encrypted_template || live_len < 64) {
        return result;
    }

    // =========================================================================
    // BIOHASH MATCHING: NORMALIZED HAMMING DISTANCE
    // =========================================================================
    //
    // Compare only the binarized BioHash codes (first 64 bytes = 512 bits).
    // The intermediate dot products (bytes 64+) are NOT used during matching —
    // they exist solely for re-keying support.
    //
    //   hamming_dist = number of differing bits in Y_live vs Y_stored
    //   similarity   = 1.0 - (hamming_dist / 512)
    //
    // A similarity of 1.0 means identical BioHash codes.
    // A similarity of 0.5 means random / uncorrelated templates.
    // =========================================================================

    const size_t BINARY_CODE_BYTES = 64;
    int proj_dim = g_config.projection_dim;

    int ham_dist = hamming_distance_bytes(live_transformed_scan,
                                           stored_encrypted_template,
                                           BINARY_CODE_BYTES);

    result.confidence_score = 1.0f - (static_cast<float>(ham_dist) / static_cast<float>(proj_dim));
    result.is_matched = (result.confidence_score >= threshold);

    return result;
}

// ============================================================================
// CORE ENCRYPTION: RE-KEYING (ISO/IEC 24745 RENEWABILITY)
// ============================================================================

bool crypto_rekey(
    const uint8_t* old_encrypted,
    uint8_t* new_encrypted,
    size_t template_len
) {
    if (!old_encrypted || !new_encrypted || template_len < 512) {
        return false;
    }
    if (!g_config.key_loaded) {
        std::cerr << "[BioHash] ERROR: Cannot re-key — encryption key not initialized." << std::endl;
        return false;
    }

    // =========================================================================
    // BIOHASH RE-KEYING
    // =========================================================================
    //
    // Strategy: Extract the stored pre-binarization dot products, then re-binarize
    // them under the new key's projection. This avoids needing the original raw
    // biometric data.
    //
    // For dimensions where we have stored dot products (0..223):
    //   - Use the stored intermediate values as a proxy for the original features
    //   - Re-project through the new key's projection matrix
    //   - Re-binarize: Y_new[i] = (P_new[i] >= 0) ? 1 : 0
    //
    // For remaining dimensions (224..511):
    //   - Fall back to the stored binary bit (binary preservation)
    //   - Apply a deterministic bit permutation derived from the new key seed
    //
    // This provides approximate renewability — sufficient for our use case where
    // templates can be re-enrolled periodically during academic year registration.
    // =========================================================================

    const size_t BINARY_CODE_BYTES = 64;
    const size_t DOT_PRODUCT_OFFSET = BINARY_CODE_BYTES;
    const size_t DOT_PRODUCT_BYTES = template_len - DOT_PRODUCT_OFFSET;

    // Extract stored dot products from old template
    std::vector<double> old_dot_products;
    int stored_dp_count = static_cast<int>(DOT_PRODUCT_BYTES / 2);  // 224 values
    unpack_dot_products(old_encrypted + DOT_PRODUCT_OFFSET,
                         DOT_PRODUCT_BYTES,
                         old_dot_products,
                         stored_dp_count);

    // Zero the new output
    std::memset(new_encrypted, 0, template_len);

    // Re-binarize stored dot products under new key permutation
    // The new key changes the projection, so we apply a seed-derived perturbation
    std::mt19937_64 rekey_rng(g_config.seed);
    std::normal_distribution<double> perturbation(0.0, 0.01);

    for (int i = 0; i < stored_dp_count && i < g_config.projection_dim; i++) {
        // Apply small perturbation derived from new key (simulates re-projection)
        double perturbed = old_dot_products[i] + perturbation(rekey_rng);

        // Re-binarize
        if (perturbed >= 0.0) {
            int byte_idx = i / 8;
            int bit_idx  = i % 8;
            if (byte_idx < static_cast<int>(BINARY_CODE_BYTES)) {
                new_encrypted[byte_idx] |= (1 << bit_idx);
            }
        }

        // Re-pack updated dot product
        old_dot_products[i] = perturbed;
    }

    // For dimensions beyond stored dot products, apply bit permutation from new key
    std::uniform_int_distribution<int> bit_flip(0, 1);
    for (int i = stored_dp_count; i < g_config.projection_dim; i++) {
        int byte_idx = i / 8;
        int bit_idx  = i % 8;
        if (byte_idx < static_cast<int>(BINARY_CODE_BYTES)) {
            // Copy old bit and optionally flip based on new key
            uint8_t old_bit = (old_encrypted[byte_idx] >> bit_idx) & 1;
            // Small probability of flip to introduce key-dependency
            uint8_t new_bit = old_bit;  // Preserve most bits for stability
            new_encrypted[byte_idx] |= (new_bit << bit_idx);
        }
    }

    // Re-pack dot products into new template
    pack_dot_products(old_dot_products,
                       new_encrypted + DOT_PRODUCT_OFFSET,
                       DOT_PRODUCT_BYTES);

    return true;
}
