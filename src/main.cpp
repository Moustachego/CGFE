/** *************************************************************/
// @Name: CGFE.cpp
// @Function: Main entry point for SRGE, DIRPE and CGFE coding algorithm
// @Author: weijzh (weijzh@pcl.ac.cn)
// @Created: 2026-01-14
/************************************************************* */

#include <bits/stdc++.h>
#include <iostream>
#include "Loader.hpp"
#include "Gray_code.hpp"
#include "Chunk_code.hpp"
#include "CGFE_code.hpp"

using namespace std;

int main(int argc, char **argv)
{
    // Parse command-line arguments
    string rules_path = "src/ACL_rules/example.rules";
    if (argc >= 2) {
        rules_path = string(argv[1]);
    }

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
        
    // ===============================================================================
    // SRGE Algorithm
    // ===============================================================================
    cout << "\n===============================================================================\n";
    cout << "----------------------------------- SRGE --------------------------------------\n";
    cout << "===============================================================================\n\n";
    cout << "[STEP 3] Applying SRGE Gray Code Encoding to Port ranges...\n\n";
    
    auto gray_coded_ports = SRGE(port_table);
    auto tcam_entries = generate_tcam_entries(gray_coded_ports);
    
    cout << "[SRGE Results]:\n\n";
    cout << "[SUCCESS] SRGE encoding complete:\n";
    cout << "  - Original port rules: " << port_table.size() << "\n";
    cout << "  - Generated TCAM entries: " << tcam_entries.size() << "\n";
    cout << "  - Average expansion factor: " 
         << fixed << setprecision(0) 
         << (double)tcam_entries.size() / port_table.size() << "x\n\n";
    
    // Extract base filename for output
    string base_name = rules_path.substr(rules_path.find_last_of("/") + 1);
    base_name = base_name.substr(0, base_name.find_last_of("."));
    string output_file = "src/output/" + base_name + "_SRGE.txt";
    
    // Save TCAM rules to file
    print_tcam_rules(tcam_entries, ip_table, output_file);
    cout << "[OUTPUT] TCAM rules saved to: " << output_file << "\n";
    
    cout << "\nend\n";
    
    // ===============================================================================
    // DIRPE algorithm
    // ===============================================================================
    cout << "\n===============================================================================\n";
    cout << "----------------------------------- DIRPE ---------------------------------------\n";
    cout << "===============================================================================\n\n";
    cout << "[STEP 4] Applying DIRPE Chunk-based Encoding to Port ranges...\n\n";
    
    // Encode ports using DIRPE with W=2 (chunk width = 2 bits)
    int chunk_width = 2;
    auto dirpe_ports = DIRPE(port_table, chunk_width);
    auto dirpe_tcam = generate_dirpe_tcam_entries(dirpe_ports);
    
    cout << "[DIRPE Results]:\n\n";
    cout << "[SUCCESS] DIRPE encoding complete:\n";
    cout << "  - Original port rules: " << port_table.size() << "\n";
    cout << "  - Generated TCAM entries: " << dirpe_tcam.size() << "\n";
    cout << "  - Chunk width (W): " << chunk_width << " bits\n";
    cout << "  - Average expansion factor: " 
         << fixed << setprecision(0) 
         << (double)dirpe_tcam.size() / port_table.size() << "x\n\n";
    
    // Save DIRPE TCAM rules to file
    string dirpe_output_file = "src/output/" + base_name + "_DIRPE.txt";
    print_dirpe_tcam_rules(dirpe_tcam, ip_table, dirpe_output_file);
    cout << "[OUTPUT] DIRPE TCAM rules saved to: " << dirpe_output_file << "\n";
    
    cout << "\nend\n";
    
    // ===============================================================================
    // CGFE algorithm
    // ===============================================================================
    cout << "\n===============================================================================\n";
    cout << "----------------------------------- CGFE ---------------------------------------\n";
    cout << "===============================================================================\n\n";
    cout << "[STEP 5] Applying CGFE (Chunked Gray Fence Encoding) to Port ranges...\n\n";

    // Configuration: W=16 (port bits), c=2 (chunk parameter)
    CGFEConfig cgfe_config = {16, 2};
    
    auto cgfe_ports = CGFE_encode_ports(port_table, cgfe_config);
    auto cgfe_tcam = generate_cgfe_tcam_entries(cgfe_ports);
    
    cout << "[CGFE Results]:\n\n";
    cout << "[SUCCESS] CGFE encoding complete:\n";
    cout << "  - Original port rules: " << port_table.size() << "\n";
    cout << "  - Generated TCAM entries: " << cgfe_tcam.size() << "\n";
    cout << "  - Config: W=" << cgfe_config.W << ", c=" << cgfe_config.c << "\n";
    cout << "  - Average expansion factor: " 
         << fixed << setprecision(0) 
         << (double)cgfe_tcam.size() / port_table.size() << "x\n\n";
    
    // Save CGFE TCAM rules to file
    string cgfe_output_file = "src/output/" + base_name + "_CGFE.txt";
    print_cgfe_tcam_rules(cgfe_tcam, ip_table, cgfe_output_file);
    cout << "[OUTPUT] CGFE TCAM rules saved to: " << cgfe_output_file << "\n";
    
    cout << "\nend\n";

    return 0;
}
