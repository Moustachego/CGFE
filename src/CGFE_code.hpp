/** *************************************************************/
// @Name: CGFE_code.hpp
// @Function: CGFE (Chunk-based Gray-code Factored Encoding) - Header
// @Author: weijzh (weijzh@pcl.ac.cn)
// @Created: 2026-01-17
// @Description: CGFE encoder with MSC/TC decomposition and reflection
/************************************************************* */

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <utility>
#include "Loader.hpp"

// ===============================================================================
// CGFE Configuration
// ===============================================================================

struct CGFEConfig {
    int W;              // Total bit width (e.g., 16 for ports)
    int c;              // Chunk parameter (bits per chunk)
    
    // Derived parameters
    int block_size() const { return 1 << (W - c); }  // 2^(W-c)
    int num_blocks() const { return 1 << c; }        // 2^c
    int tc_bits() const { return W - c; }            // Bits for TC
    int msc_bits() const { return c; }               // Bits for MSC
};

// ===============================================================================
// CGFE Encoding Entry
// ===============================================================================

struct CGFEEntry {
    int msc_lo;                 // MSC range low
    int msc_hi;                 // MSC range high
    std::string tc_pattern;     // TC ternary pattern (with *)
    
    // For debugging
    uint16_t orig_lo;           // Original range low
    uint16_t orig_hi;           // Original range high
};

// ===============================================================================
// CGFE Result
// ===============================================================================

struct CGFEResult {
    std::vector<CGFEEntry> entries;
    
    // Statistics
    int total_entries() const { return entries.size(); }
};

// ===============================================================================
// Module 1: Basic Math Functions
// ===============================================================================

// MSC(x) = floor(x / BLOCK_SIZE) - Most Significant Chunk
int cgfe_msc(uint16_t x, const CGFEConfig& config);

// TC(x) = x mod BLOCK_SIZE - Tail Chunk
int cgfe_tc(uint16_t x, const CGFEConfig& config);

// Start of block: msc * BLOCK_SIZE
uint16_t block_start(int msc, const CGFEConfig& config);

// End of block: (msc + 1) * BLOCK_SIZE - 1
uint16_t block_end(int msc, const CGFEConfig& config);

// ===============================================================================
// Module 2: Single Point Chunk Encoding (Gray-code based)
// ===============================================================================

// Encode a single TC value to ternary pattern
// Uses Gray-code chunk encoding with parity propagation
std::string encode_tc_point(int tc, const CGFEConfig& config);

// ===============================================================================
// Module 3: TC Range Encoding
// ===============================================================================

// Encode a TC range [tc_lo, tc_hi] to ternary patterns
// Returns multiple patterns if range needs decomposition
// msc_parity: true if MSC is odd (need reflected encoding)
std::vector<std::string> encode_tc_range(int tc_lo, int tc_hi, 
                                          const CGFEConfig& config,
                                          int skip_prefix_len = 0,
                                          bool msc_parity = false);

// ===============================================================================
// Module 4: Reflection Operation
// ===============================================================================

// Reflect a TC pattern (reverse Gray direction)
// This operates on the encoding, not the value
std::string reflect_tc_pattern(const std::string& pattern, const CGFEConfig& config);

// ===============================================================================
// Module 5: MSC Range Encoding
// ===============================================================================

// Encode MSC range to ternary pattern
std::string encode_msc_range(int msc_lo, int msc_hi, const CGFEConfig& config);

// ===============================================================================
// Module 6: Main CGFE Algorithm
// ===============================================================================

// Main entry: Encode range [s, e] using CGFE algorithm
// skip_prefix_len: for partial encoding (bits already covered)
CGFEResult cgfe_encode_range(uint16_t s, uint16_t e, 
                              const CGFEConfig& config,
                              int skip_prefix_len = 0);

// ===============================================================================
// Module 7: Utility Functions
// ===============================================================================

// Print CGFE result for debugging
void print_cgfe_result(const CGFEResult& result, const std::string& label = "");

// Convert CGFE entries to full ternary strings
std::vector<std::string> cgfe_to_ternary(const CGFEResult& result, const CGFEConfig& config);

// ===============================================================================
// Port Processing Structures
// ===============================================================================

// Forward declaration
struct PortRule;
struct IPRule;

struct CGFEPort {
    uint16_t src_port_lo, src_port_hi;
    uint16_t dst_port_lo, dst_port_hi;
    uint32_t priority;
    std::string action;
    CGFEResult src_cgfe;
    CGFEResult dst_cgfe;
};

struct CGFETCAM_Entry {
    std::string src_pattern;
    std::string dst_pattern;
    uint32_t priority;
    std::string action;
};

// Encode all port rules using CGFE
std::vector<CGFEPort> CGFE_encode_ports(const std::vector<PortRule>& port_table, 
                                        const CGFEConfig& config);

// Generate TCAM entries from CGFE encoded ports
std::vector<CGFETCAM_Entry> generate_cgfe_tcam_entries(const std::vector<CGFEPort>& cgfe_ports);

// Print TCAM rules
void print_cgfe_tcam_rules(const std::vector<CGFETCAM_Entry>& tcam_entries,
                           const std::vector<IPRule>& ip_table,
                           const std::string& output_file = "");
