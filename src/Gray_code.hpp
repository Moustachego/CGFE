#pragma once

#include <vector>
#include <cstdint>
#include "Loader.hpp"

// ---------------Struct Declarations---------------------
struct GrayCodedPort
{
    /* data */
};







// ---------------Function Declarations---------------------
auto Port_Gray_coding(const std::vector<PortRule> &port_table) -> std::vector<GrayCodedPort>;
