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
    string rules_path = "src/ACL_rules/test.rules";
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

    auto gray_coded_ports = Port_Gray_coding(port_table);       // Gray coding for Port rules, only reads port_table
    cout << "[SUCCESS] Completed Gray coding for Port rules.\n\n";


    cout << "CGFE Main Entry Point" << endl;
    return 0;
}
