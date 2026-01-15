#pragma once

#include <vector>
#include <cstdint>
#include <bitset>
#include <string>
#include <set>

#include "Loader.hpp"

// ---------------Struct Declarations---------------------

// Ternary string representation: '0', '1', '*'
using TernaryString = std::string;

// Result of SRGE encoding for a single range
struct SRGEResult {
    std::vector<TernaryString> ternary_entries;  // Set of ternary strings covering the range
};

struct GrayCodedPort
{
    uint16_t src_port_lo;
    uint16_t src_port_hi;
    uint16_t dst_port_lo;
    uint16_t dst_port_hi;
    std::bitset<16> src_port_lo_gray_bs; // Gray code binary representation
    std::bitset<16> src_port_hi_gray_bs;
    std::bitset<16> dst_port_lo_gray_bs;
    std::bitset<16> dst_port_hi_gray_bs;

    uint16_t LCA;               // least common ancestor position (bit index)
    uint32_t priority;          // Rule priority
    std::string action;         // Action string
    
    // SRGE results for source and destination port ranges
    SRGEResult src_srge;
    SRGEResult dst_srge;
};

struct GrayTCAM_Entry
{
    TernaryString src_pattern;  // Ternary pattern for source port
    TernaryString dst_pattern;  // Ternary pattern for destination port
    uint32_t priority;
    std::string action;
};

// ---------------Constants---------------------
constexpr int GRAY_BITS = 16;  // Number of bits for port Gray codes

// ---------------Range Structure---------------------
struct Range {
    uint16_t start;
    uint16_t end;
    
    bool empty() const { return start > end; }
};

// ---------------Function Declarations---------------------

// Module 1: Binary to Gray code conversion
uint16_t binary_to_gray(uint16_t x);
uint16_t gray_to_binary(uint16_t g);

// Module 2-8: SRGE main algorithm with corrected Gray tree handling
// IMPORTANT DESIGN NOTE (see Gray_code.cpp):
// Gray ranges spanning multiple subtrees must be split before PrefixCover
SRGEResult srge_encode(uint16_t sb, uint16_t eb, int bits = GRAY_BITS);

// Helper: Check if Gray range [s,e] lies entirely within a single Gray subtree
bool is_single_gray_subtree(uint16_t s, uint16_t e, int bits);

// Helper: Split binary range into Gray-subtree-aligned segments
std::vector<Range> split_into_single_subtrees(uint16_t s, uint16_t e, int bits);

// Helper: PrefixCover for a single Gray subtree
std::vector<std::string> prefix_cover_single_subtree(uint16_t s, uint16_t e, int bits);

// Helper: Get Gray LCA value (internal use)
// uint16_t gray_lca(uint16_t s, uint16_t e, int bits);  // Implemented as static in Gray_code.cpp

// Helper: Get LCA branch bit position (0 = MSB) (internal use)
// int lca_branch_bit(uint16_t p, int bits);  // Implemented as static in Gray_code.cpp

// Helper: Mirror a ternary prefix at given bit position (internal use)
// std::string mirror_prefix(const std::string& g, int bit_pos);  // Implemented as static in Gray_code.cpp

// Helper: Subtract mirrored coverage from a range (internal use)
// Range subtract_mirrored_range(Range other, Range base, uint16_t p, int mirror_bit, int bits);

// Main recursive SRGE implementation
void srge_gray_recursive(
    uint16_t s,
    uint16_t e,
    int bits,
    std::vector<TernaryString>& results
);

// Main entry point for port Gray coding with SRGE
auto Port_Gray_coding(const std::vector<PortRule> &port_table) -> std::vector<GrayCodedPort>;

// Utility: Convert bitset to string
std::string bitset_to_string(const std::bitset<16>& bs, int bits = GRAY_BITS);

// Debug: Print SRGE result
void print_srge_result(const SRGEResult& result, const std::string& label);
