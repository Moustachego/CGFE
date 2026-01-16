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

    uint16_t Src_LCA;               // least common ancestor position (bit index)
    uint16_t Dst_LCA;            // destination source range LCA position (bit index)
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

// Module 2: Utility functions
std::string bitset_to_string(const std::bitset<16>& bs, int bits = GRAY_BITS);
std::string gray_to_string(uint16_t g, int bits);
void print_srge_result(const SRGEResult& result, const std::string& label);

// Module 3-4: SRGE Gray Domain Algorithm
// Encodes a binary port range [sb, eb] into minimal set of ternary patterns
// Works directly in Gray code space to find optimal hypercube coverage
SRGEResult srge_encode(uint16_t sb, uint16_t eb, int bits = GRAY_BITS);

// Module 5: Port Table Processing
// Convert port rules to Gray-coded entries and apply SRGE
auto SRGE(const std::vector<PortRule> &port_table) -> std::vector<GrayCodedPort>;

// Module 6: TCAM Entry Generation
// Generate expanded TCAM entries from Gray-coded ports
// Each original rule expands to (src_patterns.size() × dst_patterns.size()) TCAM entries
std::vector<GrayTCAM_Entry> generate_tcam_entries(const std::vector<GrayCodedPort>& gray_ports);

// Module 7: Output Functions
// Print TCAM entries in ternary rule format
// If output_file is provided, writes to file; otherwise prints to stdout
void print_tcam_rules(const std::vector<GrayTCAM_Entry>& tcam_entries, 
                      const std::vector<IPRule>& ip_table,
                      const std::string& output_file = "");

