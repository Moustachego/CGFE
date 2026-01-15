#pragma once

#include <vector>
#include <cstdint>
#include "Loader.hpp"
#include <bitset>
#include <string>

// ---------------Struct Declarations---------------------
struct GrayCodedPort
{

    uint16_t src_port_lo;
    uint16_t src_port_hi;
    uint16_t dst_port_lo;
    uint16_t dst_port_hi;
    std::bitset<16> src_port_lo_gray_bs; // binary representation
    std::bitset<16> src_port_hi_gray_bs;
    std::bitset<16> dst_port_lo_gray_bs;
    std::bitset<16> dst_port_hi_gray_bs;

    uint16_t LCA;                 // least common ancestor of s and e;
    uint32_t priority;          // Rule priority
    std::string action;         // Action string
};

struct GrayTCAM_Entry
{
    /* data */
};








// ---------------Function Declarations---------------------
auto Port_Gray_coding(const std::vector<PortRule> &port_table) -> std::vector<GrayCodedPort>;
