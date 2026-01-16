/** *************************************************************/
// @Name: Chunk_code.hpp
// @Function: DIRPE (Directed Range Prefix Encoding) - Paper Implementation
// @Author: weijzh (weijzh@pcl.ac.cn)
// @Created: 2026-01-16
// @Description: Strict paper reproduction for DIRPE chunk-based encoding
/************************************************************* */

#pragma once

#include <vector>
#include <string>
#include <cstdint>

// ===============================================================================
// DIRPE Configuration
// ===============================================================================

struct DIRPEConfig {
    int W;              // Chunk bit width (e.g., W=2 means 4 values per chunk)
    int total_bits;     // Total bits for the value representation
    
    // Derived parameters
    int num_chunks() const { return total_bits / W; }
    int chunk_max() const { return (1 << W) - 1; }  // 2^W - 1
};

// ===============================================================================
// DIRPE Result Structure
// ===============================================================================

struct DIRPEResult {
    std::vector<std::string> encodings;  // List of ternary encodings
    
    // For debugging/verification
    std::vector<std::pair<uint16_t, uint16_t>> subranges;  // Decomposed subranges
};

// ===============================================================================
// Module 1: Single Chunk Encoding (Paper Formula)
// ===============================================================================

// DIRPE_value(x) = '0'^(2^W - x - 1) + '1'^x
// Encodes a single value within one chunk
std::string dirpe_value_chunk(int x, int W);

// DIRPE_range(s, e) = '0'^(2^W - e - 1) + '*'^(e - s) + '1'^s
// Encodes a range [s, e] within one chunk (requires s <= e)
std::string dirpe_range_chunk(int s, int e, int W);

// ===============================================================================
// Module 2: Chunk-aligned Range Decomposition
// ===============================================================================

// Check if a range needs decomposition (any chunk has s_chunk > e_chunk)
bool needs_decomposition(uint16_t s, uint16_t e, const DIRPEConfig& config);

// Decompose [s, e] into chunk-aligned subranges
// Each subrange satisfies: for all chunks, s_chunk <= e_chunk
std::vector<std::pair<uint16_t, uint16_t>> chunk_aligned_decomposition(
    uint16_t s, uint16_t e, const DIRPEConfig& config);

// ===============================================================================
// Module 3: Complete DIRPE Encoding
// ===============================================================================

// Encode a single value using DIRPE (chunk-wise)
std::string dirpe_encode_value(uint16_t v, const DIRPEConfig& config);

// Encode a range [s, e] using DIRPE (with automatic decomposition)
DIRPEResult dirpe_encode_range(uint16_t s, uint16_t e, const DIRPEConfig& config);

// ===============================================================================
// Module 4: Utility Functions
// ===============================================================================

// Extract chunk value at position i (0 = highest chunk)
int get_chunk(uint16_t value, int chunk_idx, const DIRPEConfig& config);

// Print DIRPE result for debugging
void print_dirpe_result(const DIRPEResult& result, const std::string& label = "");

// Format encoding with chunk separators for readability
std::string format_with_separators(const std::string& encoding, int W);

