/** *************************************************************/
// @Name: Chunk_code.cpp
// @Function: DIRPE (Database Independent Range Prefix Encoding) - Paper Implementation
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
// Module 2: Chunk-aligned Range Decomposition (HIGH-bit First Strategy)
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
 * Find the FIRST (highest) chunk where s_chunk != e_chunk
 * Returns -1 if all chunks are equal (single value)
 */
int find_split_chunk_high(uint16_t s, uint16_t e, const DIRPEConfig& config) {
    for (int i = 0; i < config.num_chunks(); i++) {
        int s_chunk = get_chunk(s, i, config);
        int e_chunk = get_chunk(e, i, config);
        if (s_chunk != e_chunk) {
            return i;
        }
    }
    return -1;  // All chunks equal (s == e)
}

/**
 * Check if a range can be directly encoded without decomposition.
 * 
 * A range [s, e] can be directly encoded if:
 *   1. All chunks satisfy s_chunk <= e_chunk
 *   2. Once a chunk has s_chunk < e_chunk, ALL subsequent (lower) chunks must 
 *      have s_chunk = 0 and e_chunk = 2^W - 1 (full range)
 * 
 * This is because DIRPE encoding uses Cartesian product of chunk ranges.
 * If chunk i has s_chunk[i] < e_chunk[i], the encoded pattern will match
 * ALL combinations of lower chunk values, not just the range [s, e].
 * 
 * Example: [1, 6] with W=2, 6 bits (3 chunks)
 *   s=1 = 00|00|01, e=6 = 00|01|10
 *   Chunk 0: 00 = 00 (equal, OK)
 *   Chunk 1: 00 < 01 (different!) 
 *   Chunk 2: 01, 10 - but this must be [0, 3] for direct encoding!
 *   Since chunk 2 is [1, 2] not [0, 3], cannot directly encode -> need decomposition
 */
bool can_directly_encode(uint16_t s, uint16_t e, const DIRPEConfig& config) {
    bool found_diff = false;  // Have we seen a chunk where s != e?
    int max_chunk_val = (1 << config.W) - 1;  // 2^W - 1
    
    for (int i = 0; i < config.num_chunks(); i++) {
        int s_chunk = get_chunk(s, i, config);
        int e_chunk = get_chunk(e, i, config);
        
        if (s_chunk > e_chunk) {
            // Invalid: s_chunk > e_chunk never allows direct encoding
            return false;
        }
        
        if (found_diff) {
            // After we've seen a differing chunk, ALL subsequent chunks
            // must be full range [0, max] for Cartesian product to work
            if (s_chunk != 0 || e_chunk != max_chunk_val) {
                return false;
            }
        } else if (s_chunk < e_chunk) {
            // First differing chunk found
            found_diff = true;
        }
        // If s_chunk == e_chunk, continue checking
    }
    
    return true;
}

/**
 * Split a range [s, e] at chunk k into subranges.
 * 
 * Given that chunk k is the first (highest) chunk where s_chunk[k] != e_chunk[k]:
 * 
 * 1. Left subrange:  [s, s_prefix | remaining_bits_all_1s]
 *    - Covers from s to the end of s's "block" at chunk k
 * 
 * 2. Middle subranges: For each c in (s_chunk[k]+1) .. (e_chunk[k]-1)
 *    - Full chunks: [c << remaining_bits, (c << remaining_bits) | remaining_bits_all_1s]
 * 
 * 3. Right subrange: [(e_prefix), e]
 *    - Covers from the start of e's "block" to e
 * 
 * Example: [1, 6] with W=2, total_bits=6 (3 chunks)
 *   s=1 = 00|00|01, e=6 = 00|01|10
 *   First diff chunk: k=1 (s_chunk[1]=00, e_chunk[1]=01)
 *   remaining_bits = 2 (chunk 2 only)
 *   
 *   s_prefix (bits at and above k) = 00|00|xx -> keep 00|00 = 0
 *   e_prefix (bits at and above k) = 00|01|xx -> keep 00|01 = 4
 *   
 *   Left:  [1, 0|remaining_mask] = [1, 3]
 *   Middle: none (s_chunk[1]+1=1, e_chunk[1]-1=0, no values)
 *   Right: [4, 6]
 */
std::vector<std::pair<uint16_t, uint16_t>> split_range_by_chunk(
    uint16_t s, uint16_t e, int k, const DIRPEConfig& config) {
    
    std::vector<std::pair<uint16_t, uint16_t>> result;
    
    // remaining_bits: bits BELOW chunk k (not including chunk k itself)
    int remaining_bits = (config.num_chunks() - k - 1) * config.W;
    
    if (remaining_bits < 0) remaining_bits = 0;
    
    uint16_t remaining_mask = (remaining_bits > 0) ? ((1 << remaining_bits) - 1) : 0;
    
    // Bits AT AND ABOVE chunk k (including chunk k)
    int upper_bits = (k + 1) * config.W;
    uint16_t upper_shift = config.total_bits - upper_bits;
    
    // Get s and e's chunk values at position k
    int s_chunk_k = get_chunk(s, k, config);
    int e_chunk_k = get_chunk(e, k, config);
    
    // Extract the prefix part (bits above chunk k, not including k)
    int prefix_bits = k * config.W;  // bits strictly above chunk k
    uint16_t prefix_shift = config.total_bits - prefix_bits;
    uint16_t prefix_from_s = (prefix_bits > 0) ? (s >> prefix_shift) : 0;
    
    // 1. Left subrange: [s, left_end]
    // left_end = same prefix as s + s_chunk_k at position k + all 1s below
    uint16_t left_prefix = (prefix_from_s << prefix_shift) | (s_chunk_k << remaining_bits);
    uint16_t left_end = left_prefix | remaining_mask;
    if (s <= left_end && left_end <= e) {
        result.push_back({s, left_end});
    }
    
    // 2. Middle subranges: for each full chunk value c in (s_chunk_k+1)..(e_chunk_k-1)
    for (int c = s_chunk_k + 1; c <= e_chunk_k - 1; c++) {
        uint16_t mid_base = (prefix_from_s << prefix_shift) | (c << remaining_bits);
        uint16_t mid_end = mid_base | remaining_mask;
        result.push_back({mid_base, mid_end});
    }
    
    // 3. Right subrange: [right_start, e]
    // right_start = same prefix as s (which equals prefix of e) + e_chunk_k at position k + all 0s below
    uint16_t right_base = (prefix_from_s << prefix_shift) | (e_chunk_k << remaining_bits);
    uint16_t right_start = right_base;  // Lower bits are 0
    if (right_start <= e && right_start > left_end) {
        result.push_back({right_start, e});
    }
    
    return result;
}

/**
 * DIRPE Decomposition (Correct High-bit First Algorithm)
 * 
 * Recursively decomposes [s, e] into subranges where each subrange can be
 * directly encoded as a Cartesian product of chunk ranges.
 * 
 * Algorithm:
 *   1. Check if [s,e] can be directly encoded
 *   2. If yes, return [{s, e}]
 *   3. If no, find first (highest) chunk k where s_chunk != e_chunk
 *   4. Split at chunk k and recursively process each subrange
 */
std::vector<std::pair<uint16_t, uint16_t>> chunk_aligned_decomposition(
    uint16_t s, uint16_t e, const DIRPEConfig& config) {
    
    std::vector<std::pair<uint16_t, uint16_t>> result;
    
    if (s > e) {
        return result;  // Empty range
    }
    
    // Step 1: Check if directly encodable
    if (can_directly_encode(s, e, config)) {
        result.push_back({s, e});
        return result;
    }
    
    // Step 2: Find first (highest) chunk where s_chunk != e_chunk
    int split_chunk = find_split_chunk_high(s, e, config);
    
    if (split_chunk == -1) {
        // All chunks equal means s == e, which is directly encodable
        result.push_back({s, e});
        return result;
    }
    
    // Step 3: Split at this chunk
    auto subranges = split_range_by_chunk(s, e, split_chunk, config);
    
    // Step 4: Recursively decompose each subrange
    for (const auto& sub : subranges) {
        auto decomposed = chunk_aligned_decomposition(sub.first, sub.second, config);
        result.insert(result.end(), decomposed.begin(), decomposed.end());
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
    // Test 4: THREE CRITICAL TEST CASES (User反例验证)
    // =========================================================================
    cout << "--- Test 4: THREE CRITICAL TEST CASES ---\n\n";
    
    // All tests use W=2, total_bits=4 (2 chunks)
    // This is the standard 4-bit configuration
    
    // =========================================================================
    // CRITICAL TEST 1: [1, 6]
    // =========================================================================
    cout << "*** CRITICAL TEST 1: [1, 6] ***\n";
    cout << "1 = 00|01, 6 = 01|10 (W=2, 4-bit)\n";
    cout << "Chunk0: 00 vs 01 → 分裂点!\n";
    cout << "Expected: [1,3], [4,6]\n";
    cout << "Expected encodings: 000**1, 0010**\n\n";
    
    DIRPEResult r_1_6 = dirpe_encode_range(1, 6, config);
    
    cout << "Actual:\n";
    for (size_t i = 0; i < r_1_6.encodings.size(); i++) {
        cout << "  [" << r_1_6.subranges[i].first << ", " 
             << r_1_6.subranges[i].second << "] -> " 
             << r_1_6.encodings[i] << endl;
    }
    
    bool test1_pass = (r_1_6.subranges.size() == 2 &&
                       r_1_6.subranges[0] == make_pair<uint16_t,uint16_t>(1, 3) &&
                       r_1_6.subranges[1] == make_pair<uint16_t,uint16_t>(4, 6) &&
                       r_1_6.encodings[0] == "000**1" &&
                       r_1_6.encodings[1] == "0010**");
    cout << (test1_pass ? "[PASS]" : "[FAIL]") << " Test 1: [1,6]\n\n";
    
    // =========================================================================
    // CRITICAL TEST 2: [6, 14]
    // =========================================================================
    cout << "*** CRITICAL TEST 2: [6, 14] ***\n";
    cout << "6 = 01|10, 14 = 11|10 (W=2, 4-bit)\n";
    cout << "Chunk0: 01 vs 11 → 分裂点!\n";
    cout << "Expected: [6,7], [8,11], [12,14]\n";
    cout << "Expected encodings: 001*11, 011***, *110**\n\n";
    
    DIRPEResult r_6_14 = dirpe_encode_range(6, 14, config);
    
    cout << "Actual:\n";
    for (size_t i = 0; i < r_6_14.encodings.size(); i++) {
        cout << "  [" << r_6_14.subranges[i].first << ", " 
             << r_6_14.subranges[i].second << "] -> " 
             << r_6_14.encodings[i] << endl;
    }
    
    bool test2_pass = (r_6_14.subranges.size() == 3 &&
                       r_6_14.subranges[0] == make_pair<uint16_t,uint16_t>(6, 7) &&
                       r_6_14.subranges[1] == make_pair<uint16_t,uint16_t>(8, 11) &&
                       r_6_14.subranges[2] == make_pair<uint16_t,uint16_t>(12, 14));
    cout << (test2_pass ? "[PASS]" : "[FAIL]") << " Test 2: [6,14]\n\n";
    
    // =========================================================================
    // CRITICAL TEST 3: [1, 13] (最致命)
    // =========================================================================
    cout << "*** CRITICAL TEST 3: [1, 13] (最致命) ***\n";
    cout << "1 = 00|01, 13 = 11|01 (W=2, 4-bit)\n";
    cout << "Chunk0: 00 vs 11 → 分裂点!\n";
    cout << "Expected: [1,3], [4,7], [8,11], [12,13]\n\n";
    
    DIRPEResult r_1_13 = dirpe_encode_range(1, 13, config);
    
    cout << "Actual:\n";
    for (size_t i = 0; i < r_1_13.encodings.size(); i++) {
        cout << "  [" << r_1_13.subranges[i].first << ", " 
             << r_1_13.subranges[i].second << "] -> " 
             << r_1_13.encodings[i] << endl;
    }
    
    bool test3_pass = (r_1_13.subranges.size() == 4 &&
                       r_1_13.subranges[0] == make_pair<uint16_t,uint16_t>(1, 3) &&
                       r_1_13.subranges[1] == make_pair<uint16_t,uint16_t>(4, 7) &&
                       r_1_13.subranges[2] == make_pair<uint16_t,uint16_t>(8, 11) &&
                       r_1_13.subranges[3] == make_pair<uint16_t,uint16_t>(12, 13));
    cout << (test3_pass ? "[PASS]" : "[FAIL]") << " Test 3: [1,13]\n\n";
    
    // Summary
    cout << "========================================\n";
    cout << "SUMMARY: " << (test1_pass && test2_pass && test3_pass ? "ALL PASS ✓" : "SOME FAILED ✗") << "\n";
    cout << "========================================\n\n";
    
    // =========================================================================
    // EXTRA TEST: [26, 36] with W=2, total_bits=6
    // =========================================================================
    cout << "*** EXTRA TEST: Range [26, 36] (W=2, 6-bit) ***\n";
    cout << "26 = 011010 (3 chunks: 01|10|10)\n";
    cout << "36 = 100100 (3 chunks: 10|01|00)\n";
    cout << "Chunk 0: 01 vs 10 → 分裂点!\n\n";
    
    cout << "Step 1: Split at chunk 0\n";
    cout << "  Left:  [26, 01|11|11] = [26, 31]\n";
    cout << "    Chunk 0: 01, Chunk 1: [10,11], Chunk 2: [10,11]\n";
    cout << "    Chunk 1 不完整 [10,11] ≠ [00,11] → 需要递归分裂\n";
    cout << "  Right: [10|00|00, 36] = [32, 36]\n";
    cout << "    Chunk 0: 10, Chunk 1: [00,01], Chunk 2: [00,00]\n";
    cout << "    Chunk 1 不完整 [00,01] ≠ [00,11] → 需要递归分裂\n\n";
    
    cout << "Step 2: Recursive decomposition needed\n";
    cout << "Expected: Multiple subranges due to incomplete middle chunks\n\n";
    
    DIRPEConfig config_6bit = {2, 6};  // W=2, total_bits=6
    DIRPEResult r_26_36 = dirpe_encode_range(26, 36, config_6bit);
    
    cout << "Actual decomposition (" << r_26_36.subranges.size() << " subranges):\n";
    for (size_t i = 0; i < r_26_36.encodings.size(); i++) {
        cout << "  [" << r_26_36.subranges[i].first << ", " 
             << r_26_36.subranges[i].second << "] -> " 
             << r_26_36.encodings[i] << endl;
    }
    cout << endl;
    
    // Verify key properties: all ranges should be valid and cover [26,36]
    uint16_t min_val = r_26_36.subranges[0].first;
    uint16_t max_val = r_26_36.subranges[r_26_36.subranges.size()-1].second;
    
    bool test_26_36_valid = (min_val == 26 && max_val == 36 && 
                             r_26_36.subranges.size() > 0 &&
                             r_26_36.encodings.size() == r_26_36.subranges.size());
    
    cout << (test_26_36_valid ? "[PASS]" : "[FAIL]") 
         << " [26,36] decomposition valid (covers [26," << max_val << "], "
         << r_26_36.subranges.size() << " subranges)\n\n";
    
    // Continue with other tests
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