/** *************************************************************/
// @Name: CGFE.cpp
// @Function: Main entry point for CGFE coding algorithm
// @Author: weijzh (weijzh@pcl.ac.cn)
// @Created: 2026-01-14
/************************************************************* */

#include <bits/stdc++.h>
#include <iostream>
#include "Loader.hpp"
#include "Gray_code.hpp"

using namespace std;

int main(int argc, char **argv)
{
    // Parse command-line arguments
    string rules_path = "src/ACL_rules/graycode.rules";
    if (argc >= 2) {
        rules_path = string(argv[1]);
    }

    cout << "===============================================================================\n";
    cout << "---------------------------------- CGFE ----------------------------------\n";
    cout << "===============================================================================\n\n";

    // Step 1: Load rules from file
    cout << "[STEP 1] Loading rules from: " << rules_path << endl;
    vector<Rule5D> rules;
    try {
        load_rules_from_file(rules_path, rules);
    } catch (const std::exception &e) {
        cerr << "[ERROR] Failed to load rules: " << e.what() << endl;
        return 1;
    }

    cout << "[SUCCESS] Loaded " << rules.size() << " rules\n\n";

    // Step 2: Split rules into IP and Port tables
    cout << "[STEP 2] Splitting rules into IP and Port tables...\n";
    vector<IPRule> ip_table;
    vector<PortRule> port_table;
    split_rules(rules, ip_table, port_table);
    cout << "[SUCCESS] IP table: " << ip_table.size() << " entries, "
         << "Port table: " << port_table.size() << " entries\n\n";

    // Step 3: Apply SRGE to Port rules
    cout << "[STEP 3] Applying SRGE Gray Code Encoding to Port ranges...\n";
    auto gray_coded_ports = SRGE(port_table);
    
    // Calculate statistics and show details
    size_t total_src_entries = 0, total_dst_entries = 0;
    cout << "\n[SRGE Results]:\n";
    for (size_t i = 0; i < gray_coded_ports.size(); i++) {
        const auto& gcp = gray_coded_ports[i];
        total_src_entries += gcp.src_srge.ternary_entries.size();
        total_dst_entries += gcp.dst_srge.ternary_entries.size();
        
        cout << "\nRule #" << (i+1) << ":\n";
        cout << "  Src port [" << gcp.src_port_lo << ", " << gcp.src_port_hi << "] -> "
             << gcp.src_srge.ternary_entries.size() << " entries:\n";
        for (const auto& entry : gcp.src_srge.ternary_entries) {
            cout << "    " << entry << "\n";
        }
        cout << "  Dst port [" << gcp.dst_port_lo << ", " << gcp.dst_port_hi << "] -> "
             << gcp.dst_srge.ternary_entries.size() << " entries:\n";
        for (const auto& entry : gcp.dst_srge.ternary_entries) {
            cout << "    " << entry << "\n";
        }
    }
    
    cout << "\n[SUCCESS] SRGE encoding complete:\n";
    cout << "  - Original port rules: " << port_table.size() << "\n";
    cout << "  - Total source port ternary entries: " << total_src_entries << "\n";
    cout << "  - Total dest port ternary entries: " << total_dst_entries << "\n";
    cout << "  - Average expansion factor: " 
         << fixed << setprecision(2) 
         << (double)(total_src_entries + total_dst_entries) / (2.0 * port_table.size()) << "x\n\n";


    cout << "CGFE Main Entry Point" << endl;
    return 0;
}
