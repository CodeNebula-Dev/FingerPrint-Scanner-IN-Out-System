#include "crypto_placeholder.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <numeric>

static CancelableCryptoScheme g_active_scheme = CancelableCryptoScheme::BIOHASHING_STANDARD;

static BioHashConfig g_config = {
    0ULL,                       // seed
    BIOHASH_PROJECTION_DIM,     // projection_dim
    false                       // key_loaded
};

static std::string g_key_filepath;

static void generate_projection_row(uint64_t seed, int row_index,
                                     std::vector<double>& row_out, int input_dim) {
    uint64_t row_seed = seed ^ (static_cast<uint64_t>(row_index) * 2654435761ULL);
    std::mt19937_64 rng(row_seed);
    std::normal_distribution<double> normal(0.0, 1.0);

    row_out.resize(input_dim);
    double norm = 0.0;

    for (int j = 0; j < input_dim; j++) {
        row_out[j] = normal(rng);
        norm += row_out[j] * row_out[j];
    }

    norm = std::sqrt(norm);
    if (norm > 1e-10) {
        for (int j = 0; j < input_dim; j++) {
            row_out[j] /= norm;
        }
    }
}

static bool compute_biohash(const uint8_t* raw_input, size_t input_len, uint64_t seed,
                             std::vector<double>& dot_products,
                             uint8_t* binary_code, size_t binary_byte_len) {
    int input_dim = static_cast<int>(input_len);
    int proj_dim = g_config.projection_dim;

    dot_products.resize(proj_dim);
    std::memset(binary_code, 0, binary_byte_len);

    // Zero-mean center the feature vector (standard BioHashing requirement)
    double mean = 0.0;
    for (int i = 0; i < input_dim; i++) {
        mean += static_cast<double>(raw_input[i]);
    }
    mean /= (input_dim > 0 ? static_cast<double>(input_dim) : 1.0);

    std::vector<double> features(input_dim);
    for (int i = 0; i < input_dim; i++) {
        features[i] = (static_cast<double>(raw_input[i]) - mean) / 255.0;
    }

    std::vector<double> row;
    for (int i = 0; i < proj_dim; i++) {
        generate_projection_row(seed, i, row, input_dim);

        double dp = 0.0;
        for (int j = 0; j < input_dim; j++) {
            dp += row[j] * features[j];
        }
        dot_products[i] = dp;

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

static int hamming_distance_bytes(const uint8_t* a, const uint8_t* b, size_t len) {
    int distance = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t diff = a[i] ^ b[i];
        while (diff) {
            distance++;
            diff &= (diff - 1);
        }
    }
    return distance;
}

static void pack_dot_products(const std::vector<double>& dot_products,
                               uint8_t* buffer, size_t buffer_len) {
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

bool crypto_init_key(const std::string& db_root_path) {
    g_key_filepath = db_root_path + "/biohash.key";

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

    std::random_device rd;
    uint64_t new_seed = (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
    g_config.seed = new_seed;

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

    if (!g_key_filepath.empty()) {
        std::ofstream key_out(g_key_filepath, std::ios::binary | std::ios::trunc);
        if (key_out.is_open()) {
            key_out.write(reinterpret_cast<const char*>(&new_seed), sizeof(uint64_t));
            key_out.close();
            std::cout << "[BioHash] Key rotated and persisted successfully." << std::endl;
            return true;
        }
    }
    return true;
}

BioHashConfig crypto_get_config() {
    return g_config;
}

CancelableCryptoScheme crypto_get_active_scheme() {
    return g_active_scheme;
}

const char* crypto_get_scheme_name() {
    return "BioHashing (Random Orthogonal Projection + Binarization)";
}

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

    std::memset(encrypted_output, 0, output_len);

    const size_t BINARY_CODE_BYTES = 64;
    const size_t DOT_PRODUCT_OFFSET = BINARY_CODE_BYTES;
    const size_t DOT_PRODUCT_BYTES = output_len - DOT_PRODUCT_OFFSET;

    std::vector<double> dot_products;

    if (!compute_biohash(raw_input, input_len, g_config.seed,
                          dot_products,
                          encrypted_output, BINARY_CODE_BYTES)) {
        return false;
    }

    pack_dot_products(dot_products,
                       encrypted_output + DOT_PRODUCT_OFFSET,
                       DOT_PRODUCT_BYTES);

    return true;
}

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

    const size_t BINARY_CODE_BYTES = 64;
    int proj_dim = g_config.projection_dim;

    int ham_dist = hamming_distance_bytes(live_transformed_scan,
                                           stored_encrypted_template,
                                           BINARY_CODE_BYTES);

    result.confidence_score = 1.0f - (static_cast<float>(ham_dist) / static_cast<float>(proj_dim));
    result.is_matched = (result.confidence_score >= threshold);

    return result;
}

bool crypto_rekey(
    const uint8_t* old_encrypted,
    uint8_t* new_encrypted,
    size_t template_len
) {
    if (!old_encrypted || !new_encrypted || template_len < 512) {
        return false;
    }
    if (!g_config.key_loaded) {
        return false;
    }

    const size_t BINARY_CODE_BYTES = 64;
    const size_t DOT_PRODUCT_OFFSET = BINARY_CODE_BYTES;
    const size_t DOT_PRODUCT_BYTES = template_len - DOT_PRODUCT_OFFSET;

    std::vector<double> old_dot_products;
    int stored_dp_count = static_cast<int>(DOT_PRODUCT_BYTES / 2);
    unpack_dot_products(old_encrypted + DOT_PRODUCT_OFFSET,
                         DOT_PRODUCT_BYTES,
                         old_dot_products,
                         stored_dp_count);

    std::memset(new_encrypted, 0, template_len);

    std::mt19937_64 rekey_rng(g_config.seed);
    std::normal_distribution<double> perturbation(0.0, 0.01);

    for (int i = 0; i < stored_dp_count && i < g_config.projection_dim; i++) {
        double perturbed = old_dot_products[i] + perturbation(rekey_rng);
        if (perturbed >= 0.0) {
            int byte_idx = i / 8;
            int bit_idx  = i % 8;
            if (byte_idx < static_cast<int>(BINARY_CODE_BYTES)) {
                new_encrypted[byte_idx] |= (1 << bit_idx);
            }
        }
        old_dot_products[i] = perturbed;
    }

    pack_dot_products(old_dot_products,
                       new_encrypted + DOT_PRODUCT_OFFSET,
                       DOT_PRODUCT_BYTES);

    return true;
}
