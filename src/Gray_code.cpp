/** *************************************************************/
// @Name: Gray_code.cpp
// @Function: Main entry point for Gray coding algorithm
// @Author: weijzh (weijzh@pcl.ac.cn)
// @Created: 2026-01-14
/************************************************************* */


#include <vector>
#include <iostream>
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

        // Gray code conversion for source port range
        gcp.src_port_lo_gray = binary_to_gray(pr.src_port_lo);
        gcp.src_port_hi_gray = binary_to_gray(pr.src_port_hi);

        // Gray code conversion for destination port range
        gcp.dst_port_lo_gray = binary_to_gray(pr.dst_port_lo);
        gcp.dst_port_hi_gray = binary_to_gray(pr.dst_port_hi);

        // Copy other fields
        gcp.priority = pr.priority;
        gcp.action = pr.action;

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