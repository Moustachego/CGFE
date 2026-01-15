#pragma once

#include <vector>
#include <cstdint>
#include "Loader.hpp"

// ---------------Struct Declarations---------------------
struct GrayCodedPort
{
    uint16_t src_port_lo_gray;  //src s
    uint16_t src_port_hi_gray;  //src e
    uint16_t dst_port_lo_gray;  //dst s
    uint16_t dst_port_hi_gray;  //dst e
    uint16_t P;                 // least common ancestor of s and e;
    uint32_t priority;          // Rule priority
    std::string action;         // Action string
};

struct GrayTCAM_Entry
{
    /* data */
};








// ---------------Function Declarations---------------------
auto Port_Gray_coding(const std::vector<PortRule> &port_table) -> std::vector<GrayCodedPort>;
