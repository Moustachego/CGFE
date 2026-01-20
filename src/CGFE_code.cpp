/** *************************************************************/
// @Name: CGFE_code.cpp
// @Function: CGFE (Chunked Gray Fence Encoding) - Implementation
// @Author: weijzh (weijzh@pcl.ac.cn)
// @Created: 2026-01-20
// @Description: Clean implementation based on algorithm plan
/************************************************************* */

#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <iomanip>
#include <fstream>

#include "CGFE_code.hpp"
#include "Loader.hpp"

using namespace std;

// ===============================================================================
// Module 1: Basic Math Functions
// ===============================================================================

int cgfe_msc(uint16_t x, const CGFEConfig& config) {
    return x / config.block_size();
}

int cgfe_tc(uint16_t x, const CGFEConfig& config) {
    return x % config.block_size();
}

uint16_t block_start(int msc, const CGFEConfig& config) {
    return msc * config.block_size();
}

uint16_t block_end(int msc, const CGFEConfig& config) {
    return (msc + 1) * config.block_size() - 1;
}

// ===============================================================================
// Module 2: Fence Encoding Functions
// ===============================================================================

/**
 * fence_encode_value: Encode a single c-bit value
 * 
 * F(value) = '0'^(2^c - 1 - value) + '1'^value
 * Output length: 2^c - 1 bits
 */
static string fence_encode_value(int value, int c) {
    int max_val = 1 << c;  // 2^c
    int encoded_len = max_val - 1;  // 2^c - 1
    
    int num_zeros = max_val - 1 - value;
    int num_ones = value;
    
    string result;
    result.reserve(encoded_len);
    
    for (int i = 0; i < num_zeros; i++) result += '0';
    for (int i = 0; i < num_ones; i++) result += '1';
    
    return result;
}

/**
 * fence_encode_range: Encode a range [start, end] within one chunk
 * 
 * F([s, e]) = '0'^(2^c - 1 - e) + '*'^(e - s) + '1'^s
 * Output length: 2^c - 1 bits
 */
static string fence_encode_range(int start, int end, int c) {
    assert(start <= end);
    
    int max_val = 1 << c;  // 2^c
    int encoded_len = max_val - 1;  // 2^c - 1
    
    int num_zeros = max_val - 1 - end;
    int num_stars = end - start;
    int num_ones = start;
    
    string result;
    result.reserve(encoded_len);
    
    for (int i = 0; i < num_zeros; i++) result += '0';
    for (int i = 0; i < num_stars; i++) result += '*';
    for (int i = 0; i < num_ones; i++) result += '1';
    
    return result;
}

/**
 * fence_decode_range: Decode a fence-encoded string back to [start, end]
 */
static pair<int, int> fence_decode_range(const string& enc, int c) {
    int max_val = 1 << c;
    int expected_len = max_val - 1;
    
    if ((int)enc.length() != expected_len) return {-1, -1};
    
    int num_zeros = 0;
    while (num_zeros < (int)enc.length() && enc[num_zeros] == '0') num_zeros++;
    
    int num_ones = 0;
    while (num_ones < (int)enc.length() && enc[enc.length() - 1 - num_ones] == '1') num_ones++;
    
    int start = num_ones;
    int end = max_val - 1 - num_zeros;
    
    return {start, end};
}

// ===============================================================================
// Module 3: Single Value CGFE Encoding
// ===============================================================================

/**
 * cgfe_encode_value_internal: Encode a single w-bit value
 * 
 * KEY: When MSC is odd, flip the first chunk of the tail encoding
 */
static string cgfe_encode_value_internal(int x, int w, int c) {
    // Base case: single chunk
    if (w == c) {
        return fence_encode_value(x, c);
    }
    
    // Decompose
    int block_size = 1 << (w - c);
    int msc = x / block_size;
    int tc = x % block_size;
    bool is_even = (msc % 2 == 0);
    
    // Recursively encode tail
    string tail_encoded = cgfe_encode_value_internal(tc, w - c, c);
    
    // Encode MSC
    string msc_encoded = fence_encode_value(msc, c);
    
    // If MSC is odd, flip the first chunk of tail
    if (!is_even) {
        int chunk_len = (1 << c) - 1;
        string first_chunk = tail_encoded.substr(0, chunk_len);
        
        auto [start, end] = fence_decode_range(first_chunk, c);
        int chunk_val = start;
        int flipped_val = (1 << c) - 1 - chunk_val;
        
        string flipped_chunk = fence_encode_value(flipped_val, c);
        tail_encoded = flipped_chunk + tail_encoded.substr(chunk_len);
    }
    
    return msc_encoded + tail_encoded;
}

// Public interface
string encode_tc_point(int tc, const CGFEConfig& config) {
    return cgfe_encode_value_internal(tc, config.tc_bits(), config.c);
}

// ===============================================================================
// Module 4: Helper Functions for Range Encoding
// ===============================================================================

/**
 * prepend_value: Add MSC encoding to each pattern
 * If MSC is odd, flip the first chunk of each pattern
 */
static vector<string> prepend_value(int p, const vector<string>& E, int c) {
    vector<string> result;
    
    string p_encoded = fence_encode_value(p, c);
    int chunk_len = (1 << c) - 1;
    bool is_odd = (p % 2 == 1);
    
    for (const string& e : E) {
        string new_entry = e;
        
        if (is_odd && (int)e.length() >= chunk_len) {
            string first_chunk = e.substr(0, chunk_len);
            auto [s, t] = fence_decode_range(first_chunk, c);
            
            int max_val = (1 << c) - 1;
            int flipped_s = max_val - t;
            int flipped_t = max_val - s;
            
            string flipped_chunk = fence_encode_range(flipped_s, flipped_t, c);
            new_entry = flipped_chunk + e.substr(chunk_len);
        }
        
        result.push_back(p_encoded + new_entry);
    }
    
    return result;
}

/**
 * TC_extract: Remove first chunk from each encoding
 */
static vector<string> TC_extract(const vector<string>& E, int c) {
    vector<string> result;
    int chunk_len = (1 << c) - 1;
    
    for (const string& e : E) {
        if ((int)e.length() > chunk_len) {
            result.push_back(e.substr(chunk_len));
        }
    }
    
    return result;
}

/**
 * reflected_extension: Create reflection-extended encoding
 */
static vector<string> reflected_extension(int p, int q, const vector<string>& tc_encoding, int c) {
    vector<string> result;
    
    string range_enc = fence_encode_range(p, q, c);
    
    for (const string& tc : tc_encoding) {
        result.push_back(range_enc + tc);
    }
    
    return result;
}

/**
 * Generate all-star tail
 */
static string generate_star_tail(int w, int c) {
    int num_chunks = (w - c) / c;
    int chunk_len = (1 << c) - 1;
    
    string result;
    for (int i = 0; i < num_chunks * chunk_len; i++) {
        result += '*';
    }
    
    return result;
}

// ===============================================================================
// Module 5: Main CGFE Algorithm (Internal)
// ===============================================================================

// Forward declaration
static vector<string> CGFE_internal(int start, int end, int w, int c);

/**
 * CGFE_PARTIAL: Encode with k values already covered
 */
static vector<string> CGFE_PARTIAL(int start, int end, int k, int w, int c) {
    int size = end - start + 1;
    
    if (k >= size) return {};
    if (k == 0) return CGFE_internal(start, end, w, c);
    
    return CGFE_internal(start + k, end, w, c);
}

/**
 * Main CGFE algorithm
 */
static vector<string> CGFE_internal(int start, int end, int w, int c) {
    if (start > end) return {};
    
    int block_size = 1 << (w - c);
    int max_tc = block_size - 1;
    
    // Decompose
    int ms = start / block_size;
    int me = end / block_size;
    int ts = start % block_size;
    int te = end % block_size;
    
    // Case 1: Local range (same block)
    if (ms == me) {
        if (w == c) {
            return { fence_encode_range(start, end, c) };
        }
        vector<string> E = CGFE_internal(ts, te, w - c, c);
        return prepend_value(ms, E, c);
    }
    
    // Case 2: Middle range (ts == 0 and te == max_tc)
    if (ts == 0 && te == max_tc) {
        string range_enc = fence_encode_range(ms, me, c);
        string star_tail = generate_star_tail(w, c);
        return { range_enc + star_tail };
    }
    
    // Case 3: Bottom range (ts == 0)
    if (ts == 0 && te != max_tc) {
        vector<string> result;
        
        if (ms <= me - 1) {
            string range_enc = fence_encode_range(ms, me - 1, c);
            string star_tail = generate_star_tail(w, c);
            result.push_back(range_enc + star_tail);
        }
        
        vector<string> E2 = CGFE_internal(0, te, w - c, c);
        vector<string> E2_with_msc = prepend_value(me, E2, c);
        result.insert(result.end(), E2_with_msc.begin(), E2_with_msc.end());
        
        return result;
    }
    
    // Case 4: Top range (te == max_tc)
    if (te == max_tc && ts != 0) {
        vector<string> result;
        
        vector<string> E1 = CGFE_internal(ts, max_tc, w - c, c);
        vector<string> E1_with_msc = prepend_value(ms, E1, c);
        result.insert(result.end(), E1_with_msc.begin(), E1_with_msc.end());
        
        if (ms + 1 <= me) {
            string range_enc = fence_encode_range(ms + 1, me, c);
            string star_tail = generate_star_tail(w, c);
            result.push_back(range_enc + star_tail);
        }
        
        return result;
    }
    
    // Case 5: Regular range
    {
        vector<string> result;
        
        int r1_end = (ms + 1) * block_size - 1;
        int r3_start = me * block_size;
        
        int r1_size = r1_end - start + 1;
        int r3_size = end - r3_start + 1;
        
        int delta = me - ms;
        
        if (delta % 2 == 1) {
            // Odd delta
            if (r1_size <= r3_size) {
                vector<string> E1 = CGFE_internal(ts, max_tc, w - c, c);
                vector<string> TC_E1 = TC_extract(prepend_value(ms, E1, c), c);
                
                vector<string> E1_ext = reflected_extension(ms, me, TC_E1, c);
                result.insert(result.end(), E1_ext.begin(), E1_ext.end());
                
                vector<string> E3 = CGFE_PARTIAL(0, te, r1_size, w - c, c);
                vector<string> E3_with_msc = prepend_value(me, E3, c);
                result.insert(result.end(), E3_with_msc.begin(), E3_with_msc.end());
            } else {
                vector<string> E3 = CGFE_internal(0, te, w - c, c);
                vector<string> TC_E3 = TC_extract(prepend_value(me, E3, c), c);
                
                vector<string> E3_ext = reflected_extension(ms, me, TC_E3, c);
                result.insert(result.end(), E3_ext.begin(), E3_ext.end());
                
                int r1_partial_end = max_tc - r3_size;
                if (ts <= r1_partial_end) {
                    vector<string> E1 = CGFE_internal(ts, r1_partial_end, w - c, c);
                    vector<string> E1_with_msc = prepend_value(ms, E1, c);
                    result.insert(result.end(), E1_with_msc.begin(), E1_with_msc.end());
                }
            }
            
            if (ms + 1 <= me - 1) {
                string range_enc = fence_encode_range(ms + 1, me - 1, c);
                string star_tail = generate_star_tail(w, c);
                result.push_back(range_enc + star_tail);
            }
        } else {
            // Even delta
            vector<string> E1 = CGFE_internal(ts, max_tc, w - c, c);
            vector<string> TC_E1 = TC_extract(prepend_value(ms, E1, c), c);
            
            vector<string> E3 = CGFE_internal(0, te, w - c, c);
            vector<string> TC_E3 = TC_extract(prepend_value(me, E3, c), c);
            
            if (r1_size + r3_size >= block_size) {
                vector<string> E1_ext = reflected_extension(ms, me - 1, TC_E1, c);
                result.insert(result.end(), E1_ext.begin(), E1_ext.end());
                
                vector<string> E3_ext = reflected_extension(ms + 1, me, TC_E3, c);
                result.insert(result.end(), E3_ext.begin(), E3_ext.end());
            } else {
                vector<string> E1_with_msc = prepend_value(ms, E1, c);
                result.insert(result.end(), E1_with_msc.begin(), E1_with_msc.end());
                
                vector<string> E3_with_msc = prepend_value(me, E3, c);
                result.insert(result.end(), E3_with_msc.begin(), E3_with_msc.end());
                
                if (ms + 1 <= me - 1) {
                    string range_enc = fence_encode_range(ms + 1, me - 1, c);
                    string star_tail = generate_star_tail(w, c);
                    result.push_back(range_enc + star_tail);
                }
            }
        }
        
        return result;
    }
}

// ===============================================================================
// Module 6: Public Interface - CGFEResult generation
// ===============================================================================

string encode_msc_range(int msc_lo, int msc_hi, const CGFEConfig& config) {
    return fence_encode_range(msc_lo, msc_hi, config.c);
}

vector<string> encode_tc_range(int tc_lo, int tc_hi, 
                                const CGFEConfig& config,
                                int skip_prefix_len,
                                bool msc_parity) {
    // Use the new internal CGFE algorithm
    return CGFE_internal(tc_lo, tc_hi, config.tc_bits(), config.c);
}

CGFEResult cgfe_encode_range(uint16_t s, uint16_t e, 
                              const CGFEConfig& config,
                              int skip_prefix_len) {
    CGFEResult result;
    
    if (s > e) return result;
    
    // Use the new algorithm
    vector<string> patterns = CGFE_internal(s, e, config.W, config.c);
    
    for (const string& pat : patterns) {
        CGFEEntry entry;
        // For the new algorithm, MSC is encoded in the pattern
        // We set msc_lo/hi to 0 and store the full pattern in tc_pattern
        entry.msc_lo = 0;
        entry.msc_hi = 0;
        entry.tc_pattern = pat;  // Full pattern including MSC
        entry.orig_lo = s;
        entry.orig_hi = e;
        result.entries.push_back(entry);
    }
    
    return result;
}

// ===============================================================================
// Module 7: Utility Functions
// ===============================================================================

void print_cgfe_result(const CGFEResult& result, const string& label) {
    if (!label.empty()) {
        cout << label << endl;
    }
    
    cout << "Total entries: " << result.entries.size() << endl;
    for (size_t i = 0; i < result.entries.size(); i++) {
        const auto& e = result.entries[i];
        cout << "  [" << i << "] Pattern=" << e.tc_pattern 
             << " (orig: [" << e.orig_lo << "," << e.orig_hi << "])" << endl;
    }
}

vector<string> cgfe_to_ternary(const CGFEResult& result, const CGFEConfig& config) {
    vector<string> ternary_list;
    
    for (const auto& entry : result.entries) {
        // The pattern already includes MSC encoding
        ternary_list.push_back(entry.tc_pattern);
    }
    
    return ternary_list;
}

// ===============================================================================
// Module 8: Port Processing Implementation
// ===============================================================================

std::vector<CGFEPort> CGFE_encode_ports(const std::vector<PortRule>& port_table, 
                                        const CGFEConfig& config) {
    std::vector<CGFEPort> result;
    
    for (const auto& port_rule : port_table) {
        CGFEPort cport;
        cport.src_port_lo = port_rule.src_port_lo;
        cport.src_port_hi = port_rule.src_port_hi;
        cport.dst_port_lo = port_rule.dst_port_lo;
        cport.dst_port_hi = port_rule.dst_port_hi;
        cport.priority = port_rule.priority;
        cport.action = port_rule.action;
        
        // Encode source port range
        cport.src_cgfe = cgfe_encode_range(port_rule.src_port_lo, port_rule.src_port_hi, config);
        
        // Encode destination port range
        cport.dst_cgfe = cgfe_encode_range(port_rule.dst_port_lo, port_rule.dst_port_hi, config);
        
        result.push_back(cport);
    }
    
    return result;
}

std::vector<CGFETCAM_Entry> generate_cgfe_tcam_entries(const std::vector<CGFEPort>& cgfe_ports) {
    std::vector<CGFETCAM_Entry> tcam_entries;
    
    // For W=16, c=2: each chunk encodes to 3 bits, 8 chunks total = 24 bits
    CGFEConfig config16 = {16, 2};
    
    for (const auto& cport : cgfe_ports) {
        auto src_patterns = cgfe_to_ternary(cport.src_cgfe, config16);
        auto dst_patterns = cgfe_to_ternary(cport.dst_cgfe, config16);
        
        for (const auto& src_pat : src_patterns) {
            for (const auto& dst_pat : dst_patterns) {
                CGFETCAM_Entry entry;
                entry.src_pattern = src_pat;
                entry.dst_pattern = dst_pat;
                entry.priority = cport.priority;
                entry.action = cport.action;
                tcam_entries.push_back(entry);
            }
        }
    }
    
    return tcam_entries;
}

void print_cgfe_tcam_rules(const std::vector<CGFETCAM_Entry>& tcam_entries,
                           const std::vector<IPRule>& ip_table,
                           const std::string& output_file) {
    std::ostream* out = &std::cout;
    std::ofstream outf;
    
    if (!output_file.empty()) {
        outf.open(output_file);
        if (!outf.is_open()) {
            std::cerr << "[ERROR] Cannot open output file: " << output_file << std::endl;
            return;
        }
        out = &outf;
    }
    
    *out << "# CGFE (Chunked Gray Fence Encoding) TCAM Rules\n";
    *out << "# Format: SRC_IP DST_IP SRC_PORT DST_PORT PROTOCOL ACTION\n";
    *out << "# Port patterns: 24 bits (8 chunks × 3 bits per chunk for W=16, c=2)\n";
    *out << "#\n";
    
    int entry_count = 0;
    for (const auto& ip_rule : ip_table) {
        for (const auto& port_entry : tcam_entries) {
            if (port_entry.priority != ip_rule.priority) {
                continue;
            }
            
            auto ip_to_string = [](uint32_t ip) -> std::string {
                return std::to_string((ip >> 24) & 0xFF) + "." +
                       std::to_string((ip >> 16) & 0xFF) + "." +
                       std::to_string((ip >> 8) & 0xFF) + "." +
                       std::to_string(ip & 0xFF);
            };
            
            std::string src_ip = ip_to_string(ip_rule.src_ip_lo);
            std::string dst_ip = ip_to_string(ip_rule.dst_ip_lo);
            
            // Pad patterns to 24 bits if needed
            std::string src_pat = port_entry.src_pattern;
            std::string dst_pat = port_entry.dst_pattern;
            while (src_pat.length() < 24) src_pat = "0" + src_pat;
            while (dst_pat.length() < 24) dst_pat = "0" + dst_pat;
            
            *out << src_ip << " " 
                 << dst_ip << " "
                 << src_pat << " "
                 << dst_pat << " "
                 << "0x" << std::hex << std::setfill('0') << std::setw(2) << (int)ip_rule.proto << std::dec << " "
                 << port_entry.action << "\n";
            
            entry_count++;
        }
    }
    
    *out << "\n# Total TCAM entries: " << entry_count << "\n";
    
    if (outf.is_open()) {
        outf.close();
    }
}


