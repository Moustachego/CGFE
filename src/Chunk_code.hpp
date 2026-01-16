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
#include "Loader.hpp"

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
// Module 2: Chunk-aligned Range Decomposition (High-bit First Strategy)
// ===============================================================================

// Find the FIRST (highest) chunk where s_chunk != e_chunk
// Returns -1 if all chunks equal
int find_split_chunk_high(uint16_t s, uint16_t e, const DIRPEConfig& config);

// Split a range [s, e] at chunk k into left/middle/right subranges
std::vector<std::pair<uint16_t, uint16_t>> split_range_by_chunk(
    uint16_t s, uint16_t e, int k, const DIRPEConfig& config);

// Decompose [s, e] into chunk-aligned subranges (recursive)
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

// ===============================================================================
// Module 5: Port Processing (similar to SRGE interface)
// ===============================================================================

// Structure for DIRPE-encoded port (similar to GrayCodedPort)
struct DIRPEPort {
    uint16_t src_port_lo;
    uint16_t src_port_hi;
    uint16_t dst_port_lo;
    uint16_t dst_port_hi;
    uint32_t priority;
    std::string action;
    
    // DIRPE results for source and destination port ranges
    DIRPEResult src_dirpe;
    DIRPEResult dst_dirpe;
};

// Structure for DIRPE TCAM entry (port dimension only)
struct DIRPETCAM_Entry {
    std::string src_pattern;
    std::string dst_pattern;
    uint32_t priority;
    std::string action;
};

// Encode port table using DIRPE
std::vector<DIRPEPort> DIRPE(const std::vector<PortRule>& port_table, 
                              int chunk_width = 2);

// Generate TCAM entries from DIRPE-encoded ports
std::vector<DIRPETCAM_Entry> generate_dirpe_tcam_entries(const std::vector<DIRPEPort>& dirpe_ports);

// Print DIRPE TCAM rules to file or stdout
void print_dirpe_tcam_rules(const std::vector<DIRPETCAM_Entry>& tcam_entries,
                            const std::vector<IPRule>& ip_table,
                            const std::string& output_file = "");

