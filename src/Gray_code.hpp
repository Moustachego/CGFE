#pragma once

#include <vector>
#include <cstdint>
#include "Loader.hpp"
#include <bitset>
#include <string>
#include <set>

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

// ---------------Function Declarations---------------------

// Module 1: Binary to Gray code conversion
uint16_t binary_to_gray(uint16_t x);
uint16_t gray_to_binary(uint16_t g);

// Module 2: Calculate LCA (Least Common Ancestor) position
// Returns the bit position where s and e first differ (0 = MSB)
int find_lca_position(uint16_t s, uint16_t e);

// Module 3-8: SRGE main algorithm
// Converts a binary range [sb, eb] to a set of ternary strings
SRGEResult srge_encode(uint16_t sb, uint16_t eb, int bits = GRAY_BITS);

// Main entry point for port Gray coding with SRGE
auto SRGE(const std::vector<PortRule> &port_table) -> std::vector<GrayCodedPort>;

// Utility: Convert bitset to string
std::string bitset_to_string(const std::bitset<16>& bs, int bits = GRAY_BITS);

// Debug: Print SRGE result
void print_srge_result(const SRGEResult& result, const std::string& label);
