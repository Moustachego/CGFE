/** *************************************************************/
// @Name: Gray_code.cpp
// @Function: SRGE (Gray Code Range Encoding) algorithm implementation
// @Author: weijzh (weijzh@pcl.ac.cn)
// @Created: 2026-01-14
/************************************************************* */

#include <vector>
#include <iostream>
#include <bitset>
#include <algorithm>
#include <set>
#include "Gray_code.hpp"
#include "Loader.hpp"

using namespace std;

// ============================================================
// Module 1: Gray Code Conversion (已完成)
// ============================================================

uint16_t binary_to_gray(uint16_t x) {
    return x ^ (x >> 1);
}

uint16_t gray_to_binary(uint16_t g) {
    uint16_t b = 0;
    for (; g; g >>= 1) {
        b ^= g;
    }
    return b;
}

// ============================================================
// Utility Functions
// ============================================================

string bitset_to_string(const bitset<16>& bs, int bits) {
    string s = bs.to_string();
    return s.substr(16 - bits);  // Take only the rightmost 'bits' characters
}

void print_srge_result(const SRGEResult& result, const string& label) {
    cout << label << " SRGE result (" << result.ternary_entries.size() << " entries):" << endl;
    for (const auto& entry : result.ternary_entries) {
        cout << "  " << entry << endl;
    }
}

// ============================================================
// Module 2: LCA (Least Common Ancestor) Calculation
// Gray code 树的 LCA
// ============================================================

// Find the position of the first differing bit (0 = MSB)
// Returns bits if s == e (no difference)
int find_lca_position(uint16_t s, uint16_t e) {
    if (s == e) return GRAY_BITS;
    
    uint16_t diff = s ^ e;
    // Find position of highest set bit in diff (0 = MSB)
    for (int i = 0; i < GRAY_BITS; i++) {
        if (diff & (1 << (GRAY_BITS - 1 - i))) {
            return i;
        }
    }
    return GRAY_BITS;
}

// Local version that respects the bits parameter
static int find_lca_position_bits(uint16_t s, uint16_t e, int bits) {
    if (s == e) return bits;
    
    uint16_t diff = s ^ e;
    // Find position of highest set bit in diff (0 = MSB)
    for (int i = 0; i < bits; i++) {
        if (diff & (1 << (bits - 1 - i))) {
            return i;
        }
    }
    return bits;
}

// Get the Gray code value of LCA node
// LCA is the common prefix with remaining bits filled according to Gray code tree structure
static uint16_t get_lca_value(uint16_t s, uint16_t e, int bits) {
    int lca_pos = find_lca_position(s, e);
    if (lca_pos >= bits) return s;
    
    // LCA value: common prefix + 1 + 0s (for right subtree root in Gray code tree)
    uint16_t mask = ~((1 << (bits - lca_pos)) - 1);  // Keep prefix bits
    uint16_t lca = (s & mask) | (1 << (bits - 1 - lca_pos));  // Set branch bit to 1
    // Fill rest with 0s
    return lca & ~((1 << (bits - 1 - lca_pos)) - 1) | (1 << (bits - 1 - lca_pos));
}

// Get LCA prefix string (common prefix of s and e)
static string get_lca_prefix(uint16_t s, uint16_t e, int bits) {
    int lca_pos = find_lca_position(s, e);
    if (lca_pos >= bits) {
        return bitset_to_string(bitset<16>(s), bits);
    }
    string s_str = bitset_to_string(bitset<16>(s), bits);
    return s_str.substr(0, lca_pos);
}

// ============================================================
// Module 3: Gray Code Tree Navigation
// 获取子树的边界叶节点
// ============================================================

// In Gray code tree, get the rightmost leaf of left subtree at branch position
// This is the last Gray code before crossing to right subtree
static uint16_t get_pl(uint16_t s, int lca_pos, int bits) {
    // pl: same prefix as s, then at lca_pos set to s's bit, 
    // then fill with pattern to get rightmost in that subtree
    // In Gray code, rightmost in left subtree has bit pattern: prefix + 0 + 100...0
    string s_str = bitset_to_string(bitset<16>(s), bits);
    string pl_str = s_str.substr(0, lca_pos);  // Common prefix
    pl_str += s_str[lca_pos];  // Same bit as s at branch position
    
    // Fill remaining: in Gray code tree, rightmost leaf is 100...0 pattern
    if (lca_pos + 1 < bits) {
        pl_str += '1';
        for (int i = lca_pos + 2; i < bits; i++) {
            pl_str += '0';
        }
    }
    
    return static_cast<uint16_t>(bitset<16>(pl_str).to_ulong());
}

// Get the leftmost leaf of right subtree at branch position
static uint16_t get_pr(uint16_t e, int lca_pos, int bits) {
    // pr: same prefix, then opposite bit at lca_pos, then 100...0
    string e_str = bitset_to_string(bitset<16>(e), bits);
    string pr_str = e_str.substr(0, lca_pos);
    pr_str += e_str[lca_pos];  // Same bit as e at branch position
    
    // Fill remaining: leftmost leaf is 100...0 pattern
    if (lca_pos + 1 < bits) {
        pr_str += '1';
        for (int i = lca_pos + 2; i < bits; i++) {
            pr_str += '0';
        }
    }
    
    return static_cast<uint16_t>(bitset<16>(pr_str).to_ulong());
}

// ============================================================
// Module 5: PrefixCover - 合并 Gray code 区间为 ternary prefix
// ============================================================

// Convert a Gray code interval to a single ternary prefix if possible
// Returns empty string if cannot be represented as single prefix
static string prefix_cover_single(uint16_t a, uint16_t b, int bits) {
    if (a == b) {
        return bitset_to_string(bitset<16>(a), bits);
    }
    
    string a_str = bitset_to_string(bitset<16>(a), bits);
    string b_str = bitset_to_string(bitset<16>(b), bits);
    
    // Find common prefix
    int common_len = 0;
    while (common_len < bits && a_str[common_len] == b_str[common_len]) {
        common_len++;
    }
    
    // Build ternary: common prefix + wildcards
    string result = a_str.substr(0, common_len);
    for (int i = common_len; i < bits; i++) {
        result += '*';
    }
    
    return result;
}

// Compute interval size in Gray code domain
static int gray_interval_size(uint16_t s, uint16_t e, int bits) {
    // Convert to binary to get actual size
    uint16_t sb = gray_to_binary(s);
    uint16_t eb = gray_to_binary(e);
    if (sb <= eb) {
        return eb - sb + 1;
    } else {
        return sb - eb + 1;
    }
}

// ============================================================
// Module 7-8: SRGE Main Algorithm (Gray Code Domain LCA Method)
// ============================================================

// Forward declaration
static void srge_gray_recursive(uint16_t s, uint16_t e, int bits, vector<TernaryString>& results, int depth = 0);

// Mirror a ternary string at position i (set bit i to '*')
static string mirror_at_position(const string& ternary, int pos) {
    string result = ternary;
    if (pos < (int)result.length()) {
        result[pos] = '*';
    }
    return result;
}

// Main SRGE algorithm following the Gray code LCA method
static void srge_gray_recursive(uint16_t s, uint16_t e, int bits, vector<TernaryString>& results, int depth) {
    string indent(depth * 2, ' ');
    
    // Base case: single point
    if (s == e) {
        string result = bitset_to_string(bitset<16>(s), bits);
        cout << indent << "Single point: " << result << endl;
        results.push_back(result);
        return;
    }
    
    // Step 2: Find LCA position (branching bit) using bits parameter
    int lca_pos = find_lca_position_bits(s, e, bits);
    
    cout << indent << "Processing [" << bitset_to_string(bitset<16>(s), bits) 
         << ", " << bitset_to_string(bitset<16>(e), bits) << "]" << endl;
    cout << indent << "LCA position: " << lca_pos << " (bit from MSB)" << endl;
    
    // Step 3: Get boundary points pl and pr
    uint16_t pl = get_pl(s, lca_pos, bits);
    uint16_t pr = get_pr(e, lca_pos, bits);
    
    cout << indent << "pl (rightmost of left subtree): " << bitset_to_string(bitset<16>(pl), bits) << endl;
    cout << indent << "pr (leftmost of right subtree): " << bitset_to_string(bitset<16>(pr), bits) << endl;
    
    // Step 4: Compare interval sizes
    int left_size = gray_interval_size(s, pl, bits);
    int right_size = gray_interval_size(pr, e, bits);
    
    cout << indent << "Left interval [" << bitset_to_string(bitset<16>(s), bits) 
         << ", " << bitset_to_string(bitset<16>(pl), bits) << "] size: " << left_size << endl;
    cout << indent << "Right interval [" << bitset_to_string(bitset<16>(pr), bits)
         << ", " << bitset_to_string(bitset<16>(e), bits) << "] size: " << right_size << endl;
    
    if (left_size <= right_size) {
        // Case A: Right side is larger or equal
        cout << indent << "Case A: Right side larger, processing left first" << endl;
        
        // Step A1: Cover left part [s, pl] and mirror
        string left_prefix = prefix_cover_single(s, pl, bits);
        cout << indent << "Left prefix: " << left_prefix << endl;
        
        string mirrored = mirror_at_position(left_prefix, lca_pos);
        cout << indent << "Mirrored (bit " << lca_pos << " -> *): " << mirrored << endl;
        results.push_back(mirrored);
        
        // Step A2: Calculate remaining range on right side
        // The mirrored prefix covers left_size elements on the right side too
        // Remaining starts after those covered elements
        uint16_t pr_bin = gray_to_binary(pr);
        uint16_t e_bin = gray_to_binary(e);
        
        // Skip 'left_size' elements from pr
        uint16_t remaining_start_bin = pr_bin + left_size;
        
        if (remaining_start_bin > e_bin) {
            cout << indent << "No remaining range" << endl;
            return;
        }
        
        uint16_t remaining_start_gray = binary_to_gray(remaining_start_bin);
        cout << indent << "Remaining range: [" << bitset_to_string(bitset<16>(remaining_start_gray), bits)
             << ", " << bitset_to_string(bitset<16>(e), bits) << "]" << endl;
        
        // Step A3: Recursively cover remaining
        srge_gray_recursive(remaining_start_gray, e, bits, results, depth + 1);
        
    } else {
        // Case B: Left side is larger
        cout << indent << "Case B: Left side larger, processing right first" << endl;
        
        // Step B1: Cover right part [pr, e] and mirror
        string right_prefix = prefix_cover_single(pr, e, bits);
        cout << indent << "Right prefix: " << right_prefix << endl;
        
        string mirrored = mirror_at_position(right_prefix, lca_pos);
        cout << indent << "Mirrored (bit " << lca_pos << " -> *): " << mirrored << endl;
        results.push_back(mirrored);
        
        // Step B2: Calculate remaining range on left side
        uint16_t s_bin = gray_to_binary(s);
        uint16_t pl_bin = gray_to_binary(pl);
        
        // Skip 'right_size' elements from end of left interval
        if (right_size >= left_size) {
            cout << indent << "No remaining range" << endl;
            return;
        }
        
        uint16_t remaining_end_bin = pl_bin - right_size;
        
        if (remaining_end_bin < s_bin) {
            cout << indent << "No remaining range" << endl;
            return;
        }
        
        uint16_t remaining_end_gray = binary_to_gray(remaining_end_bin);
        cout << indent << "Remaining range: [" << bitset_to_string(bitset<16>(s), bits)
             << ", " << bitset_to_string(bitset<16>(remaining_end_gray), bits) << "]" << endl;
        
        // Recursively cover remaining
        srge_gray_recursive(s, remaining_end_gray, bits, results, depth + 1);
    }
}

// ============================================================
// Module 9: SRGE Public Interface
// ============================================================

SRGEResult srge_encode(uint16_t sb, uint16_t eb, int bits) {
    SRGEResult result;
    
    if (sb > eb) {
        swap(sb, eb);
    }
    
    // Special case: single value
    if (sb == eb) {
        uint16_t g = binary_to_gray(sb);
        result.ternary_entries.push_back(bitset_to_string(bitset<16>(g), bits));
        return result;
    }
    
    // Special case: full range
    uint16_t max_val = (1u << bits) - 1;
    if (sb == 0 && eb >= max_val) {
        result.ternary_entries.push_back(string(bits, '*'));
        return result;
    }
    
    // Convert range endpoints to Gray code
    uint16_t s = binary_to_gray(sb);
    uint16_t e = binary_to_gray(eb);
    
    cout << "\n=== SRGE Encoding ===" << endl;
    cout << "Binary range [" << sb << ", " << eb << "]" << endl;
    cout << "Gray range [" << bitset_to_string(bitset<16>(s), bits) << ", " 
         << bitset_to_string(bitset<16>(e), bits) << "]" << endl;
    cout << endl;
    
    // Run Gray code domain SRGE algorithm
    srge_gray_recursive(s, e, bits, result.ternary_entries);
    
    // Remove duplicates
    sort(result.ternary_entries.begin(), result.ternary_entries.end());
    result.ternary_entries.erase(
        unique(result.ternary_entries.begin(), result.ternary_entries.end()),
        result.ternary_entries.end()
    );
    
    return result;
}

// ============================================================
// Port Processing Functions
// ============================================================

void Port_to_Gray(const vector<PortRule> &port_table, vector<GrayCodedPort> &gray_coded_ports)
{
    for (const auto &pr : port_table) {
        GrayCodedPort gcp;

        // Store original port endpoints 
        gcp.src_port_lo = static_cast<uint16_t>(pr.src_port_lo); 
        gcp.src_port_hi = static_cast<uint16_t>(pr.src_port_hi); 
        gcp.dst_port_lo = static_cast<uint16_t>(pr.dst_port_lo); 
        gcp.dst_port_hi = static_cast<uint16_t>(pr.dst_port_hi); 

        // Compute numeric gray values (16-bit) 
        uint16_t s_lo_gray = binary_to_gray(gcp.src_port_lo); 
        uint16_t s_hi_gray = binary_to_gray(gcp.src_port_hi); 
        uint16_t d_lo_gray = binary_to_gray(gcp.dst_port_lo); 
        uint16_t d_hi_gray = binary_to_gray(gcp.dst_port_hi); 

        // Store binary representations in bitsets 
        gcp.src_port_lo_gray_bs = std::bitset<16>(s_lo_gray); 
        gcp.src_port_hi_gray_bs = std::bitset<16>(s_hi_gray); 
        gcp.dst_port_lo_gray_bs = std::bitset<16>(d_lo_gray); 
        gcp.dst_port_hi_gray_bs = std::bitset<16>(d_hi_gray); 

        // Copy metadata 
        gcp.priority = pr.priority; 
        gcp.action = pr.action;
        
        // Calculate LCA for source port range
        gcp.LCA = static_cast<uint16_t>(find_lca_position(s_lo_gray, s_hi_gray));

        gray_coded_ports.push_back(gcp);
    }
}

void Apply_SRGE_to_Ports(vector<GrayCodedPort> &gray_coded_ports)
{
    cout << "\n========== Applying SRGE to Port Ranges ==========\n" << endl;
    
    for (auto &gcp : gray_coded_ports) {
        cout << "Processing rule with priority " << gcp.priority << ":" << endl;
        
        // Apply SRGE to source port range
        cout << "  Source ports [" << gcp.src_port_lo << ", " << gcp.src_port_hi << "]:" << endl;
        gcp.src_srge = srge_encode(gcp.src_port_lo, gcp.src_port_hi);
        for (const auto& entry : gcp.src_srge.ternary_entries) {
            cout << "    " << entry << endl;
        }
        
        // Apply SRGE to destination port range
        cout << "  Dest ports [" << gcp.dst_port_lo << ", " << gcp.dst_port_hi << "]:" << endl;
        gcp.dst_srge = srge_encode(gcp.dst_port_lo, gcp.dst_port_hi);
        for (const auto& entry : gcp.dst_srge.ternary_entries) {
            cout << "    " << entry << endl;
        }
        
        cout << endl;
    }
}

auto SRGE(const vector<PortRule> &port_table) -> vector<GrayCodedPort>
{
    vector<GrayCodedPort> gray_coded_ports;

    // Step 1: Gray coding (Module 1)
    Port_to_Gray(port_table, gray_coded_ports);

    // Step 2: Apply SRGE algorithm (Modules 2-8)
    Apply_SRGE_to_Ports(gray_coded_ports);

    return gray_coded_ports;
}


#ifdef DEMO_LOADER_MAIN
int main(int argc, char **argv) {
    // First, let's verify the Gray code sequence
    cout << "===== Gray Code Sequence (4 bits) =====" << endl;
    cout << "Binary -> Gray" << endl;
    for (int i = 0; i <= 15; i++) {
        uint16_t g = binary_to_gray(i);
        cout << "  " << i << " -> " << bitset_to_string(bitset<16>(g), 4) 
             << " (decimal: " << g << ")" << endl;
    }
    cout << endl;
    
    // Test SRGE with example [6, 14]
    cout << "===== SRGE Test: Range [6, 14] (4 bits) =====" << endl;
    SRGEResult result = srge_encode(6, 14, 4);
    print_srge_result(result, "[6,14]");
    
    // Verify coverage
    cout << "\nVerification - What each ternary covers:" << endl;
    for (const auto& entry : result.ternary_entries) {
        cout << "  " << entry << " covers: ";
        for (int i = 0; i <= 15; i++) {
            uint16_t g = binary_to_gray(i);
            string g_str = bitset_to_string(bitset<16>(g), 4);
            bool match = true;
            for (int j = 0; j < 4 && match; j++) {
                if (entry[j] != '*' && entry[j] != g_str[j]) {
                    match = false;
                }
            }
            if (match) cout << i << " ";
        }
        cout << endl;
    }
    
    return 0;
}
#endif