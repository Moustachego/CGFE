/** *************************************************************/
// @Name: Chunk_code.cpp
// @Function: DIRPE (Directed Range Prefix Encoding) - Paper Implementation
// @Author: weijzh (weijzh@pcl.ac.cn)
// @Created: 2026-01-16
// @Description: Strict paper reproduction for DIRPE chunk-based encoding
/************************************************************* */

#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <iomanip>
#include <fstream>

#include "Chunk_code.hpp"
#include "Loader.hpp"

using namespace std;

// ===============================================================================
// Module 1: Single Chunk Encoding (Paper Formula)
// ===============================================================================

/**
 * DIRPE_value(x) = '0'^(2^W - x - 1) + '1'^x
 * 
 * Example (W=2, so 2^W = 4):
 *   x=0: '0'^3 + '1'^0 = "000"
 *   x=1: '0'^2 + '1'^1 = "001"
 *   x=2: '0'^1 + '1'^2 = "011"
 *   x=3: '0'^0 + '1'^3 = "111"
 */
std::string dirpe_value_chunk(int x, int W) {
    int max_val = (1 << W);  // 2^W
    int num_zeros = max_val - x - 1;
    int num_ones = x;
    
    std::string result;
    result.reserve(max_val - 1);
    
    // Append '0's
    for (int i = 0; i < num_zeros; i++) {
        result += '0';
    }
    // Append '1's
    for (int i = 0; i < num_ones; i++) {
        result += '1';
    }
    
    return result;
}

/**
 * DIRPE_range(s, e) = '0'^(2^W - e - 1) + '*'^(e - s) + '1'^s
 * 
 * Requires: s <= e (within same chunk)
 * 
 * Example (W=2, so 2^W = 4):
 *   [0,0]: '0'^3 + '*'^0 + '1'^0 = "000"
 *   [0,1]: '0'^2 + '*'^1 + '1'^0 = "00*"
 *   [0,3]: '0'^0 + '*'^3 + '1'^0 = "***"
 *   [2,3]: '0'^0 + '*'^1 + '1'^2 = "*11"
 *   [1,2]: '0'^1 + '*'^1 + '1'^1 = "0*1"
 */
std::string dirpe_range_chunk(int s, int e, int W) {
    assert(s <= e && "DIRPE range encoding requires s <= e within chunk");
    
    int max_val = (1 << W);  // 2^W
    int num_zeros = max_val - e - 1;
    int num_stars = e - s;
    int num_ones = s;
    
    std::string result;
    result.reserve(max_val - 1);
    
    // Append '0's
    for (int i = 0; i < num_zeros; i++) {
        result += '0';
    }
    // Append '*'s
    for (int i = 0; i < num_stars; i++) {
        result += '*';
    }
    // Append '1's
    for (int i = 0; i < num_ones; i++) {
        result += '1';
    }
    
    return result;
}

// ===============================================================================
// Module 2: Chunk-aligned Range Decomposition
// ===============================================================================

/**
 * Extract chunk value at position chunk_idx (0 = highest/leftmost chunk)
 * 
 * Example (total_bits=4, W=2):
 *   value = 9 = 1001 (binary)
 *   chunk_idx=0: bits [3:2] = 10 = 2
 *   chunk_idx=1: bits [1:0] = 01 = 1
 */
int get_chunk(uint16_t value, int chunk_idx, const DIRPEConfig& config) {
    int shift = (config.num_chunks() - 1 - chunk_idx) * config.W;
    int mask = (1 << config.W) - 1;
    return (value >> shift) & mask;
}

/**
 * Check if a range needs decomposition.
 * 
 * A range needs decomposition if any chunk has s_chunk > e_chunk.
 * This means the range "wraps around" within that chunk dimension.
 */
bool needs_decomposition(uint16_t s, uint16_t e, const DIRPEConfig& config) {
    if (s > e) return true;  // Invalid range
    
    for (int i = 0; i < config.num_chunks(); i++) {
        int s_chunk = get_chunk(s, i, config);
        int e_chunk = get_chunk(e, i, config);
        if (s_chunk > e_chunk) {
            return true;
        }
    }
    return false;
}

/**
 * Chunk-aligned Range Decomposition
 * 
 * Decomposes [s, e] into subranges where each subrange satisfies:
 *   for all chunks i: s_chunk[i] <= e_chunk[i]
 * 
 * Strategy (recursive):
 *   1. Find the lowest (rightmost) chunk where s_chunk > e_chunk
 *   2. Split at that chunk's boundary:
 *      - Left part: [s, s with lower bits all 1s] 
 *      - Right part: [e with lower bits all 0s, e]
 *      - Middle part: everything in between (if exists)
 *   3. Recursively decompose each part
 * 
 * Example: [2, 9] with W=2, total_bits=4
 *   2 = 00|10, 9 = 10|01
 *   Chunk 0: 00 < 10 (OK)
 *   Chunk 1: 10 > 01 (FAIL) -> Split here
 *   
 *   Decompose at chunk 1 boundary:
 *   - [2, 3]: s=00|10, end at 00|11 (fill lower chunk to max)
 *   - [4, 7]: 01|00 to 01|11 (middle complete chunk)
 *   - [8, 9]: start at 10|00 to e=10|01
 */
std::vector<std::pair<uint16_t, uint16_t>> chunk_aligned_decomposition(
    uint16_t s, uint16_t e, const DIRPEConfig& config) {
    
    std::vector<std::pair<uint16_t, uint16_t>> result;
    
    if (s > e) {
        return result;  // Empty range
    }
    
    // Check if decomposition is needed
    if (!needs_decomposition(s, e, config)) {
        // No decomposition needed - range is already chunk-aligned
        result.push_back({s, e});
        return result;
    }
    
    // Find the LOWEST (rightmost) chunk where s_chunk > e_chunk
    // This is the chunk that causes the "wrap-around"
    int split_chunk = -1;
    for (int i = config.num_chunks() - 1; i >= 0; i--) {
        int s_chunk = get_chunk(s, i, config);
        int e_chunk = get_chunk(e, i, config);
        if (s_chunk > e_chunk) {
            split_chunk = i;
            break;
        }
    }
    
    if (split_chunk == -1) {
        // Should not happen if needs_decomposition returned true
        result.push_back({s, e});
        return result;
    }
    
    // Calculate chunk boundary for splitting
    // chunk_bits: number of bits below (and including) the split chunk
    int chunk_bits = (config.num_chunks() - split_chunk) * config.W;
    uint16_t lower_mask = (1 << chunk_bits) - 1;  // Mask for lower bits including split chunk
    
    // Get the upper part of s and e (bits above the split chunk)
    uint16_t s_upper = s & ~lower_mask;
    uint16_t e_upper = e & ~lower_mask;
    
    // Subrange 1: [s, s_upper | lower_mask] - left boundary (fill lower bits with 1s)
    uint16_t left_end = s_upper | lower_mask;
    if (s <= left_end) {
        auto sub1 = chunk_aligned_decomposition(s, left_end, config);
        result.insert(result.end(), sub1.begin(), sub1.end());
    }
    
    // Subrange 2: [s_upper + (1 << chunk_bits), e_upper - 1] - middle chunks
    // This is the range between the left boundary end and right boundary start
    uint16_t middle_start = s_upper + (1 << chunk_bits);
    uint16_t middle_end = e_upper - 1;
    if (middle_start <= middle_end && middle_end < 0xFFFF) {
        auto sub2 = chunk_aligned_decomposition(middle_start, middle_end, config);
        result.insert(result.end(), sub2.begin(), sub2.end());
    }
    
    // Subrange 3: [e_upper, e] - right boundary (clear lower bits to 0)
    if (e_upper <= e && e_upper > left_end) {
        auto sub3 = chunk_aligned_decomposition(e_upper, e, config);
        result.insert(result.end(), sub3.begin(), sub3.end());
    }
    
    return result;
}

// ===============================================================================
// Module 3: Complete DIRPE Encoding
// ===============================================================================

/**
 * Encode a single value using DIRPE (chunk-wise concatenation)
 */
std::string dirpe_encode_value(uint16_t v, const DIRPEConfig& config) {
    std::string result;
    
    for (int i = 0; i < config.num_chunks(); i++) {
        int chunk_val = get_chunk(v, i, config);
        result += dirpe_value_chunk(chunk_val, config.W);
    }
    
    return result;
}

/**
 * Encode a single subrange (already chunk-aligned) using DIRPE
 */
static std::string dirpe_encode_subrange(uint16_t s, uint16_t e, const DIRPEConfig& config) {
    std::string result;
    
    for (int i = 0; i < config.num_chunks(); i++) {
        int s_chunk = get_chunk(s, i, config);
        int e_chunk = get_chunk(e, i, config);
        result += dirpe_range_chunk(s_chunk, e_chunk, config.W);
    }
    
    return result;
}

/**
 * Encode a range [s, e] using DIRPE (with automatic decomposition)
 */
DIRPEResult dirpe_encode_range(uint16_t s, uint16_t e, const DIRPEConfig& config) {
    DIRPEResult result;
    
    // Step 1: Chunk-aligned decomposition
    result.subranges = chunk_aligned_decomposition(s, e, config);
    
    // Step 2: Encode each subrange
    for (const auto& subrange : result.subranges) {
        std::string encoding = dirpe_encode_subrange(subrange.first, subrange.second, config);
        result.encodings.push_back(encoding);
    }
    
    return result;
}

// ===============================================================================
// Module 4: Utility Functions
// ===============================================================================

/**
 * Format encoding with chunk separators for readability
 * Example: "000*11" with W=2 -> "000 *11"
 */
std::string format_with_separators(const std::string& encoding, int W) {
    std::string result;
    int chunk_size = (1 << W) - 1;  // 2^W - 1 symbols per chunk
    
    for (size_t i = 0; i < encoding.length(); i++) {
        if (i > 0 && i % chunk_size == 0) {
            result += ' ';
        }
        result += encoding[i];
    }
    
    return result;
}

/**
 * Print DIRPE result for debugging
 */
void print_dirpe_result(const DIRPEResult& result, const std::string& label) {
    if (!label.empty()) {
        std::cout << label << std::endl;
    }
    
    std::cout << "  Subranges: " << result.subranges.size() << std::endl;
    for (size_t i = 0; i < result.encodings.size(); i++) {
        std::cout << "    [" << result.subranges[i].first << ", " 
                  << result.subranges[i].second << "] -> " 
                  << result.encodings[i] << std::endl;
    }
}

// ===============================================================================
// Module 5: Port Processing
// ===============================================================================

/**
 * Encode port table using DIRPE (similar to SRGE)
 */
std::vector<DIRPEPort> DIRPE(const std::vector<PortRule>& port_table, 
                              int chunk_width) {
    std::vector<DIRPEPort> dirpe_ports;
    
    DIRPEConfig config;
    config.W = chunk_width;
    config.total_bits = 16;  // Standard port range is 16-bit
    
    for (const auto& pr : port_table) {
        DIRPEPort dp;
        dp.src_port_lo = pr.src_port_lo;
        dp.src_port_hi = pr.src_port_hi;
        dp.dst_port_lo = pr.dst_port_lo;
        dp.dst_port_hi = pr.dst_port_hi;
        dp.priority = pr.priority;
        dp.action = pr.action;
        
        // Encode source and destination port ranges
        dp.src_dirpe = dirpe_encode_range(pr.src_port_lo, pr.src_port_hi, config);
        dp.dst_dirpe = dirpe_encode_range(pr.dst_port_lo, pr.dst_port_hi, config);
        
        dirpe_ports.push_back(dp);
    }
    
    return dirpe_ports;
}

/**
 * Generate TCAM entries from DIRPE-encoded ports
 */
std::vector<DIRPETCAM_Entry> generate_dirpe_tcam_entries(const std::vector<DIRPEPort>& dirpe_ports) {
    std::vector<DIRPETCAM_Entry> tcam_entries;
    
    for (const auto& dp : dirpe_ports) {
        // Cartesian product: each src_pattern × each dst_pattern
        for (const auto& src_pat : dp.src_dirpe.encodings) {
            for (const auto& dst_pat : dp.dst_dirpe.encodings) {
                DIRPETCAM_Entry entry;
                entry.src_pattern = src_pat;
                entry.dst_pattern = dst_pat;
                entry.priority = dp.priority;
                entry.action = dp.action;
                tcam_entries.push_back(entry);
            }
        }
    }
    
    return tcam_entries;
}

/**
 * Print DIRPE TCAM rules to file or stdout
 */
void print_dirpe_tcam_rules(const std::vector<DIRPETCAM_Entry>& tcam_entries,
                            const std::vector<IPRule>& ip_table,
                            const std::string& output_file) {
    // Determine output stream
    std::ostream* out_stream = &std::cout;
    std::ofstream file_stream;
    
    if (!output_file.empty()) {
        // Create output directory if needed
        size_t last_slash = output_file.find_last_of("/");
        if (last_slash != std::string::npos) {
            std::string dir = output_file.substr(0, last_slash);
            system(("mkdir -p " + dir).c_str());
        }
        
        file_stream.open(output_file);
        if (!file_stream.is_open()) {
            std::cerr << "[ERROR] Cannot open output file: " << output_file << "\n";
            return;
        }
        out_stream = &file_stream;
    }
    
    *out_stream << "=== DIRPE TCAM Rules (Chunk-based Ternary Format) ===\n\n";
    
    for (size_t i = 0; i < tcam_entries.size(); i++) {
        const auto& entry = tcam_entries[i];
        
        // Find corresponding IP rule by priority
        const IPRule* ip_rule = nullptr;
        for (const auto& ipr : ip_table) {
            if (ipr.priority == entry.priority) {
                ip_rule = &ipr;
                break;
            }
        }
        
        if (!ip_rule) {
            std::cerr << "[WARN] No IP rule found for priority " << entry.priority << "\n";
            continue;
        }
        
        // Format: @SRC_IP/MASK  DST_IP/MASK  SPORT_PATTERN  DPORT_PATTERN  PROTO/MASK  ACTION
        *out_stream << "@";
        
        // Source IP
        uint32_t sip = ip_rule->src_ip_lo;
        *out_stream << ((sip >> 24) & 0xFF) << "." 
                    << ((sip >> 16) & 0xFF) << "." 
                    << ((sip >> 8) & 0xFF) << "." 
                    << (sip & 0xFF) << "/" << ip_rule->src_prefix_len;
        
        *out_stream << "     ";
        
        // Destination IP
        uint32_t dip = ip_rule->dst_ip_lo;
        *out_stream << ((dip >> 24) & 0xFF) << "." 
                    << ((dip >> 16) & 0xFF) << "." 
                    << ((dip >> 8) & 0xFF) << "." 
                    << (dip & 0xFF) << "/" << ip_rule->dst_prefix_len;
        
        *out_stream << "         ";
        
        // Source port pattern (show last 8 bits for readability)
        std::string src_short = entry.src_pattern.length() >= 8 ? 
            entry.src_pattern.substr(entry.src_pattern.length() - 8) : entry.src_pattern;
        *out_stream << src_short << "  ";
        
        // Destination port pattern (show last 8 bits for readability)
        std::string dst_short = entry.dst_pattern.length() >= 8 ? 
            entry.dst_pattern.substr(entry.dst_pattern.length() - 8) : entry.dst_pattern;
        *out_stream << dst_short << "   ";
        
        // Protocol
        *out_stream << "0x" << std::hex << std::setw(2) << std::setfill('0') 
                    << (int)ip_rule->proto << "/0xFF   ";
        
        // Action
        *out_stream << std::dec << entry.action;
        
        *out_stream << "\n";
    }
    
    *out_stream << "\n=== Total DIRPE TCAM Entries: " << tcam_entries.size() << " ===\n";
    
    if (file_stream.is_open()) {
        file_stream.close();
    }
}

// ===============================================================================
// Test Main (compile with -DDEMO_CHUNK_MAIN)
// ===============================================================================

#ifdef DEMO_CHUNK_MAIN
int main() {
    cout << "=== DIRPE (Directed Range Prefix Encoding) Test ===\n\n";
    
    // Configuration: W=2, total_bits=4
    DIRPEConfig config;
    config.W = 2;
    config.total_bits = 4;
    
    cout << "Configuration: W=" << config.W << ", total_bits=" << config.total_bits << endl;
    cout << "Chunks: " << config.num_chunks() << ", Max chunk value: " << config.chunk_max() << endl;
    cout << endl;
    
    // =========================================================================
    // Test 1: Single chunk value encoding
    // =========================================================================
    cout << "--- Test 1: Single Chunk Value Encoding (DIRPE_value) ---\n";
    cout << "Formula: DIRPE_value(x) = '0'^(2^W - x - 1) + '1'^x\n\n";
    
    for (int x = 0; x <= config.chunk_max(); x++) {
        string enc = dirpe_value_chunk(x, config.W);
        cout << "  x=" << x << " -> " << enc << endl;
    }
    cout << endl;
    
    // =========================================================================
    // Test 2: Single chunk range encoding
    // =========================================================================
    cout << "--- Test 2: Single Chunk Range Encoding (DIRPE_range) ---\n";
    cout << "Formula: DIRPE_range(s, e) = '0'^(2^W - e - 1) + '*'^(e - s) + '1'^s\n\n";
    
    // Test some representative ranges
    vector<pair<int, int>> test_ranges = {{0, 0}, {0, 1}, {0, 2}, {0, 3}, 
                                           {1, 1}, {1, 2}, {1, 3},
                                           {2, 2}, {2, 3},
                                           {3, 3}};
    for (const auto& r : test_ranges) {
        string enc = dirpe_range_chunk(r.first, r.second, config.W);
        cout << "  [" << r.first << ", " << r.second << "] -> " << enc << endl;
    }
    cout << endl;
    
    // =========================================================================
    // Test 3: Paper Example - Range [2, 9]
    // =========================================================================
    cout << "--- Test 3: Paper Example - Range [2, 9] ---\n";
    cout << "Binary: 2 = 0010, 9 = 1001\n";
    cout << "Chunks: 2 = 00|10, 9 = 10|01\n";
    cout << "Chunk 1: 00 vs 10 (OK: 0 <= 2)\n";
    cout << "Chunk 2: 10 vs 01 (FAIL: 2 > 1) -> Needs decomposition\n\n";
    
    cout << "Expected decomposition: [2,3], [4,7], [8,9]\n";
    cout << "Expected encodings:\n";
    cout << "  [2,3] = [00|10, 00|11] -> 000 *11\n";
    cout << "  [4,7] = [01|00, 01|11] -> 001 ***\n";
    cout << "  [8,9] = [10|00, 10|01] -> 011 00*\n\n";
    
    DIRPEResult result = dirpe_encode_range(2, 9, config);
    
    cout << "Actual result:\n";
    for (size_t i = 0; i < result.encodings.size(); i++) {
        cout << "  [" << result.subranges[i].first << ", " 
             << result.subranges[i].second << "] -> " 
             << format_with_separators(result.encodings[i], config.W) << endl;
    }
    cout << endl;
    
    // Verify expected results
    bool test_passed = true;
    vector<string> expected = {"000*11", "001***", "01100*"};
    
    if (result.encodings.size() != expected.size()) {
        test_passed = false;
        cout << "[FAIL] Wrong number of encodings: got " << result.encodings.size() 
             << ", expected " << expected.size() << endl;
    } else {
        for (size_t i = 0; i < expected.size(); i++) {
            if (result.encodings[i] != expected[i]) {
                test_passed = false;
                cout << "[FAIL] Encoding " << i << ": got " << result.encodings[i] 
                     << ", expected " << expected[i] << endl;
            }
        }
    }
    
    if (test_passed) {
        cout << "[PASS] Paper example [2, 9] verified!\n";
    }
    cout << endl;
    
    // =========================================================================
    // Test 4: Additional test cases
    // =========================================================================
    cout << "--- Test 4: Additional Test Cases ---\n\n";
    
    // Test single value encoding
    cout << "Single value v=6:\n";
    string val_enc = dirpe_encode_value(6, config);
    cout << "  6 = 0110 -> " << format_with_separators(val_enc, config.W) << endl;
    cout << "  Expected: 001 011 (chunk 01 -> 001, chunk 10 -> 011)\n\n";
    
    // Test non-decomposable range
    cout << "Range [4, 7] (no decomposition needed):\n";
    DIRPEResult r2 = dirpe_encode_range(4, 7, config);
    print_dirpe_result(r2, "");
    cout << "  Expected: 001*** (single encoding)\n\n";
    
    // Test full range
    cout << "Range [0, 15] (full range):\n";
    DIRPEResult r3 = dirpe_encode_range(0, 15, config);
    print_dirpe_result(r3, "");
    cout << "  Expected: ****** (single encoding, all wildcards)\n\n";
    
    // Test single point
    cout << "Range [5, 5] (single point):\n";
    DIRPEResult r4 = dirpe_encode_range(5, 5, config);
    print_dirpe_result(r4, "");
    cout << "  Expected: 001001 (same as value encoding for 5)\n\n";
    
    // =========================================================================
    // Test 5: Different chunk widths
    // =========================================================================
    cout << "--- Test 5: Different Chunk Widths ---\n\n";
    
    // W=1, total_bits=4 (4 chunks of 1 bit each)
    DIRPEConfig config_w1 = {1, 4};
    cout << "Config: W=1, total_bits=4 (4 chunks)\n";
    cout << "Range [2, 9]:\n";
    DIRPEResult r5 = dirpe_encode_range(2, 9, config_w1);
    for (size_t i = 0; i < r5.encodings.size(); i++) {
        cout << "  [" << r5.subranges[i].first << ", " 
             << r5.subranges[i].second << "] -> " << r5.encodings[i] << endl;
    }
    cout << endl;
    
    // W=4, total_bits=8 (2 chunks of 4 bits each)
    DIRPEConfig config_w4 = {4, 8};
    cout << "Config: W=4, total_bits=8 (2 chunks)\n";
    cout << "Range [20, 200]:\n";
    DIRPEResult r6 = dirpe_encode_range(20, 200, config_w4);
    for (size_t i = 0; i < r6.encodings.size(); i++) {
        cout << "  [" << r6.subranges[i].first << ", " 
             << r6.subranges[i].second << "] -> " << r6.encodings[i] << endl;
    }
    cout << endl;
    
    cout << "=== DIRPE Test Complete ===\n";
    return 0;
}
#endif
