#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "Loader.hpp"

struct TCAM_Entry
{
    uint32_t Src_IP_lo;
    uint32_t Src_IP_hi;
    uint32_t Dst_IP_lo;
    uint32_t Dst_IP_hi;
    uint16_t Src_Port_prefix;
    uint16_t Src_Port_mask;
    uint16_t Dst_Port_prefix;
    uint16_t Dst_Port_mask;
    uint8_t Proto;
    std::string action;
    size_t rule_id;
};

// Convert a port range [lo, hi] to minimal set of (prefix, mask) pairs
std::vector<std::pair<uint16_t, uint16_t>> port_range_to_prefixes(uint16_t lo, uint16_t hi);

// Expand all rules into TCAM entries using Prefix coding for ports
void TCAM_Port_Expansion(
    const std::vector<Rule5D> &rules,
    std::vector<TCAM_Entry> &tcam_entries);

// Print Prefix coding TCAM rules to a file or stdout
void print_prefix_tcam_rules(
    const std::vector<TCAM_Entry> &tcam_entries,
    const std::vector<Rule5D> &rules,
    const std::string &output_file);
