/** *************************************************************/
// @Name: Gray_code.cpp
// @Function: Main entry point for Gray coding algorithm
// @Author: weijzh (weijzh@pcl.ac.cn)
// @Created: 2026-01-14
/************************************************************* */


#include <vector>
#include <iostream>
#include <bitset>
#include "Gray_code.hpp"
#include "Loader.hpp"

using namespace std;


uint16_t binary_to_gray(uint16_t x) {
    return x ^ (x >> 1);
}

void Port_to_Gray(const vector<PortRule> &port_table, vector<GrayCodedPort> &gray_coded_ports)
{
    for (const auto &pr : port_table) {
        GrayCodedPort gcp;

        // store original port endpoints
        gcp.src_port_lo = static_cast<uint16_t>(pr.src_port_lo);
        gcp.src_port_hi = static_cast<uint16_t>(pr.src_port_hi);
        gcp.dst_port_lo = static_cast<uint16_t>(pr.dst_port_lo);
        gcp.dst_port_hi = static_cast<uint16_t>(pr.dst_port_hi);

        // compute numeric gray values (16-bit)
        uint16_t s_lo_gray = binary_to_gray(gcp.src_port_lo);
        uint16_t s_hi_gray = binary_to_gray(gcp.src_port_hi);
        uint16_t d_lo_gray = binary_to_gray(gcp.dst_port_lo);
        uint16_t d_hi_gray = binary_to_gray(gcp.dst_port_hi);

        // store binary representations in bitsets
        gcp.src_port_lo_gray_bs = std::bitset<16>(s_lo_gray);
        gcp.src_port_hi_gray_bs = std::bitset<16>(s_hi_gray);
        gcp.dst_port_lo_gray_bs = std::bitset<16>(d_lo_gray);
        gcp.dst_port_hi_gray_bs = std::bitset<16>(d_hi_gray);

        // copy metadata
        gcp.priority = pr.priority;
        gcp.action = pr.action;

        gcp.LCA = {};

        gray_coded_ports.push_back(gcp);
    }
}

auto Port_Gray_coding(const vector<PortRule> &port_table) -> vector<GrayCodedPort>
{
    vector<GrayCodedPort> gray_coded_ports;

    //step 1: Gray coding
    Port_to_Gray(port_table, gray_coded_ports);

    //step 2: Merge ranges for shorter TCAM
    

    return gray_coded_ports;
}


#ifdef DEMO_LOADER_MAIN
int main(int argc, char **argv) {
    return 0;
}
#endif