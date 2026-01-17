/** *************************************************************/
// @Name: CGFE_code.cpp
// @Function: CGFE (Chunk-based Gray-code Factored Encoding) - Implementation
// @Author: weijzh (weijzh@pcl.ac.cn)
// @Created: 2026-01-17
// @Description: CGFE encoder with MSC/TC decomposition and reflection
/************************************************************* */

#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <fstream>

#include "CGFE_code.hpp"
#include "Loader.hpp"

using namespace std;

// ===============================================================================
// Module 1: Basic Math Functions
// ===============================================================================

/**
 * MSC(x) = floor(x / BLOCK_SIZE)
 * Most Significant Chunk - the "block index"
 */
int cgfe_msc(uint16_t x, const CGFEConfig& config) {
    return x / config.block_size();
}

/**
 * TC(x) = x mod BLOCK_SIZE
 * Tail Chunk - the offset within the block
 */
int cgfe_tc(uint16_t x, const CGFEConfig& config) {
    return x % config.block_size();
}

/**
 * Start of block: msc * BLOCK_SIZE
 */
uint16_t block_start(int msc, const CGFEConfig& config) {
    return msc * config.block_size();
}

/**
 * End of block: (msc + 1) * BLOCK_SIZE - 1
 */
uint16_t block_end(int msc, const CGFEConfig& config) {
    return (msc + 1) * config.block_size() - 1;
}

// ===============================================================================
// Module 2: Single Point Chunk Encoding (Gray-code based)
// ===============================================================================

/**
 * Binary to Gray code conversion
 */
static uint16_t binary_to_gray(uint16_t n) {
    return n ^ (n >> 1);
}

/**
 * Encode a single chunk value using CGFE formula:
 * CGFE_value(x) = '0'^(2^c - x - 1) + '1'^x
 * 
 * When reflected=true (odd MSC), we encode (max_val - 1 - x) instead of x:
 * This gives the Gray-code reflection property where adjacent blocks have
 * symmetric TC encodings.
 * 
 * Example (c=2, max_val=4):
 * - Normal (even MSC): TC=0→000, TC=1→001, TC=2→011, TC=3→111
 * - Reflected (odd MSC): TC=0→111, TC=1→011, TC=2→001, TC=3→000
 */
static string encode_single_chunk(int x, int chunk_bits, bool reflected = false) {
    int max_val = (1 << chunk_bits);  // 2^c
    
    // Apply reflection by encoding the symmetric value
    if (reflected) {
        x = max_val - 1 - x;
    }
    
    int num_zeros = max_val - x - 1;
    int num_ones = x;
    
    string result;
    result.reserve(max_val - 1);
    
    // Always use normal encoding: 0s first, then 1s
    for (int i = 0; i < num_zeros; i++) {
        result += '0';
    }
    for (int i = 0; i < num_ones; i++) {
        result += '1';
    }
    
    return result;
}

/**
 * Encode a single TC value to ternary pattern
 * Uses Gray-code chunk encoding with parity propagation
 * 
 * The TC value is split into (W-c)/c chunks, each encoded with Gray-code
 * Parity of each chunk affects the encoding direction of the next chunk
 */
string encode_tc_point(int tc, const CGFEConfig& config) {
    string result;
    int tc_bits = config.tc_bits();
    int chunk_size = config.c;
    int num_tc_chunks = tc_bits / chunk_size;
    
    if (num_tc_chunks == 0) {
        // TC is smaller than one chunk, encode directly
        return encode_single_chunk(tc, tc_bits);
    }
    
    int parity = 0;  // Running parity (0 = normal, 1 = reflected)
    
    for (int i = 0; i < num_tc_chunks; i++) {
        // Extract chunk i (from high to low)
        int shift = (num_tc_chunks - 1 - i) * chunk_size;
        int orig_chunk_val = (tc >> shift) & ((1 << chunk_size) - 1);
        
        // Apply reflection if parity is odd
        int enc_chunk_val = orig_chunk_val;
        if (parity) {
            enc_chunk_val = ((1 << chunk_size) - 1) - orig_chunk_val;
        }
        
        // Encode this chunk
        result += encode_single_chunk(enc_chunk_val, chunk_size);
        
        // Update parity based on ENCODED chunk value's LSB (this is the key!)
        parity ^= (enc_chunk_val & 1);
    }
    
    return result;
}

// ===============================================================================
// Module 3: TC Range Encoding
// ===============================================================================

/**
 * Encode a single chunk range [s, e] to ternary pattern
 * Normal:    CGFE_range(s, e) = '0'^(2^c - e - 1) + '*'^(e - s) + '1'^s
 * Reflected: Encode [max-1-e, max-1-s] instead (symmetric range)
 */
static string encode_chunk_range(int s, int e, int chunk_bits, bool reflected = false) {
    assert(s <= e && "Range encoding requires s <= e");
    
    int max_val = (1 << chunk_bits);  // 2^c
    
    // Apply reflection by encoding the symmetric range
    if (reflected) {
        int new_s = max_val - 1 - e;
        int new_e = max_val - 1 - s;
        s = new_s;
        e = new_e;
    }
    
    int num_zeros = max_val - e - 1;
    int num_stars = e - s;
    int num_ones = s;
    
    string result;
    result.reserve(max_val - 1);
    
    // Always use normal encoding: 0s, then *, then 1s
    for (int i = 0; i < num_zeros; i++) {
        result += '0';
    }
    for (int i = 0; i < num_stars; i++) {
        result += '*';
    }
    for (int i = 0; i < num_ones; i++) {
        result += '1';
    }
    
    return result;
}

/**
 * Check if a TC range can be directly encoded (no decomposition needed)
 * Similar to DIRPE's can_directly_encode but for TC space
 */
static bool tc_can_directly_encode(int tc_lo, int tc_hi, const CGFEConfig& config) {
    int chunk_size = config.c;
    int num_tc_chunks = config.tc_bits() / chunk_size;
    int max_chunk_val = (1 << chunk_size) - 1;
    
    if (num_tc_chunks == 0) {
        return tc_lo <= tc_hi;
    }
    
    bool found_diff = false;
    
    for (int i = 0; i < num_tc_chunks; i++) {
        int shift = (num_tc_chunks - 1 - i) * chunk_size;
        int s_chunk = (tc_lo >> shift) & max_chunk_val;
        int e_chunk = (tc_hi >> shift) & max_chunk_val;
        
        if (s_chunk > e_chunk) {
            return false;
        }
        
        if (found_diff) {
            if (s_chunk != 0 || e_chunk != max_chunk_val) {
                return false;
            }
        } else if (s_chunk < e_chunk) {
            found_diff = true;
        }
    }
    
    return true;
}

/**
 * Find split chunk for TC decomposition (high-bit first)
 */
static int tc_find_split_chunk(int tc_lo, int tc_hi, const CGFEConfig& config) {
    int chunk_size = config.c;
    int num_tc_chunks = config.tc_bits() / chunk_size;
    int max_chunk_val = (1 << chunk_size) - 1;
    
    for (int i = 0; i < num_tc_chunks; i++) {
        int shift = (num_tc_chunks - 1 - i) * chunk_size;
        int s_chunk = (tc_lo >> shift) & max_chunk_val;
        int e_chunk = (tc_hi >> shift) & max_chunk_val;
        if (s_chunk != e_chunk) {
            return i;
        }
    }
    return -1;
}

/**
 * Decompose TC range and encode to ternary patterns
 * msc_parity: true if MSC is odd
 */
static vector<string> tc_decompose_and_encode(int tc_lo, int tc_hi, 
                                               const CGFEConfig& config,
                                               int skip_prefix_len,
                                               bool msc_parity);

/**
 * Encode a subrange that is already directly encodable
 * msc_parity: true if MSC is odd (need reflected encoding)
 */
static string tc_encode_direct(int tc_lo, int tc_hi, const CGFEConfig& config, 
                                bool msc_parity = false) {
    string result;
    int chunk_size = config.c;
    int num_tc_chunks = config.tc_bits() / chunk_size;
    int max_chunk_val = (1 << chunk_size) - 1;
    
    if (num_tc_chunks == 0) {
        return encode_chunk_range(tc_lo, tc_hi, config.tc_bits(), msc_parity);
    }
    
    // For CGFE with MSC parity, the TC encoding alternates direction
    // based on MSC parity and chunk index
    int parity = msc_parity ? 1 : 0;
    
    for (int i = 0; i < num_tc_chunks; i++) {
        int shift = (num_tc_chunks - 1 - i) * chunk_size;
        int s_chunk = (tc_lo >> shift) & max_chunk_val;
        int e_chunk = (tc_hi >> shift) & max_chunk_val;
        
        // Encode with current parity
        result += encode_chunk_range(s_chunk, e_chunk, chunk_size, (parity == 1));
        
        // Update parity based on ENCODED chunk value's LSB
        // If parity=1 (reflected), range [s,e] becomes [max-e, max-s]
        // The encoded start value is (max-e) if reflected, else s
        int enc_s_chunk = (parity == 1) ? (max_chunk_val - e_chunk) : s_chunk;
        parity ^= (enc_s_chunk & 1);
    }
    
    return result;
}

/**
 * Encode a TC range [tc_lo, tc_hi] to ternary patterns
 * msc_parity: true if MSC is odd (need reflected encoding)
 */
vector<string> encode_tc_range(int tc_lo, int tc_hi, 
                                const CGFEConfig& config,
                                int skip_prefix_len,
                                bool msc_parity) {
    vector<string> result;
    
    if (tc_lo > tc_hi) {
        return result;
    }
    
    // Handle skip_prefix_len (partial encoding)
    if (skip_prefix_len > 0) {
        int skip_count = skip_prefix_len;
        tc_lo = max(tc_lo, skip_count);
        if (tc_lo > tc_hi) {
            return result;
        }
    }
    
    // Check if directly encodable
    if (tc_can_directly_encode(tc_lo, tc_hi, config)) {
        result.push_back(tc_encode_direct(tc_lo, tc_hi, config, msc_parity));
        return result;
    }
    
    // Need decomposition
    return tc_decompose_and_encode(tc_lo, tc_hi, config, skip_prefix_len, msc_parity);
}

/**
 * Decompose TC range and encode
 * msc_parity: true if MSC is odd
 */
static vector<string> tc_decompose_and_encode(int tc_lo, int tc_hi, 
                                               const CGFEConfig& config,
                                               int skip_prefix_len,
                                               bool msc_parity) {
    vector<string> result;
    
    int chunk_size = config.c;
    int num_tc_chunks = config.tc_bits() / chunk_size;
    int max_chunk_val = (1 << chunk_size) - 1;
    
    // Find split chunk
    int k = tc_find_split_chunk(tc_lo, tc_hi, config);
    if (k == -1) {
        result.push_back(tc_encode_direct(tc_lo, tc_hi, config, msc_parity));
        return result;
    }
    
    // Remaining bits below chunk k
    int remaining_bits = (num_tc_chunks - k - 1) * chunk_size;
    int remaining_mask = (remaining_bits > 0) ? ((1 << remaining_bits) - 1) : 0;
    
    int s_chunk_k = (tc_lo >> remaining_bits) & max_chunk_val;
    int e_chunk_k = (tc_hi >> remaining_bits) & max_chunk_val;
    
    // Extract prefix (bits above chunk k)
    int prefix_bits = k * chunk_size;
    int prefix_shift = config.tc_bits() - prefix_bits;
    int prefix = (prefix_bits > 0) ? (tc_lo >> prefix_shift) : 0;
    
    // Left subrange
    int left_prefix = (prefix << prefix_shift) | (s_chunk_k << remaining_bits);
    int left_end = left_prefix | remaining_mask;
    if (tc_lo <= left_end) {
        auto sub = encode_tc_range(tc_lo, left_end, config, 0, msc_parity);
        result.insert(result.end(), sub.begin(), sub.end());
    }
    
    // Middle subranges
    for (int c = s_chunk_k + 1; c <= e_chunk_k - 1; c++) {
        int mid_base = (prefix << prefix_shift) | (c << remaining_bits);
        int mid_end = mid_base | remaining_mask;
        auto sub = encode_tc_range(mid_base, mid_end, config, 0, msc_parity);
        result.insert(result.end(), sub.begin(), sub.end());
    }
    
    // Right subrange
    int right_base = (prefix << prefix_shift) | (e_chunk_k << remaining_bits);
    if (right_base <= tc_hi && right_base > left_end) {
        auto sub = encode_tc_range(right_base, tc_hi, config, 0, msc_parity);
        result.insert(result.end(), sub.begin(), sub.end());
    }
    
    return result;
}

// ===============================================================================
// Module 4: Reflection Operation (FUNDAMENTAL RETHINK)
// ===============================================================================

/**
 * CGFE Reflection Extension - Correct Understanding
 * 
 * The key insight of CGFE/SRGE is that the encoding scheme itself is designed
 * such that extending the MSC range automatically covers symmetric values in
 * adjacent blocks. This is NOT about flipping 0↔1 in the pattern!
 * 
 * The "Directed Range Encoding" has a built-in property:
 *   - Pattern P generated for range [tc_lo, tc_hi] in block K
 *   - The SAME pattern P in block K+1 matches range [BLOCK_SIZE-1-tc_hi, BLOCK_SIZE-1-tc_lo]
 * 
 * Example: [6,9] with W=4, c=2, BLOCK_SIZE=4
 *   - MSC=1, TC=[2,3] generates pattern P
 *   - Extending MSC to [1,2] means:
 *     - In MSC=1: P matches TC=[2,3] → values 6,7
 *     - In MSC=2: P matches TC=[3-3, 3-2] = TC=[0,1] → values 8,9
 *   - This single entry covers exactly [6,7,8,9]!
 * 
 * IMPORTANT: The TC pattern is NOT modified. The reflection happens implicitly
 * due to the encoding scheme design.
 * 
 * For now, we implement a SIMPLER correct approach:
 *   - The reflection extension simply extends the MSC range
 *   - The encoding scheme's inherent reflection property handles the rest
 */

/**
 * Create a reflection extension entry
 * Simply extend MSC range; the encoding scheme handles reflection implicitly
 * 
 * Note: This works because the directed encoding scheme has the property that
 * the same ternary pattern matches symmetric TC values in adjacent blocks.
 */
static CGFEEntry create_reflection_extension(
    int msc_lo, int msc_hi,
    const string& tc_pattern,
    uint16_t orig_lo, uint16_t orig_hi,
    const CGFEConfig& config) {
    
    CGFEEntry entry;
    entry.msc_lo = msc_lo;
    entry.msc_hi = msc_hi;
    // Do NOT modify the TC pattern - reflection is implicit in the encoding scheme
    entry.tc_pattern = tc_pattern;
    entry.orig_lo = orig_lo;
    entry.orig_hi = orig_hi;
    return entry;
}

/**
 * Compute the symmetric TC range in an adjacent block
 * If we have TC range [lo, hi] in block K, the symmetric range in block K±1 is:
 * [BLOCK_SIZE - 1 - hi, BLOCK_SIZE - 1 - lo]
 */
static pair<int, int> symmetric_tc_range(int tc_lo, int tc_hi, const CGFEConfig& config) {
    int bs = config.block_size();
    int sym_lo = bs - 1 - tc_hi;
    int sym_hi = bs - 1 - tc_lo;
    return {sym_lo, sym_hi};
}

// ===============================================================================
// Module 5: MSC Range Encoding
// ===============================================================================

/**
 * Encode MSC range [msc_lo, msc_hi] to ternary pattern
 * Uses same CGFE chunk encoding
 */
string encode_msc_range(int msc_lo, int msc_hi, const CGFEConfig& config) {
    int msc_bits = config.msc_bits();
    return encode_chunk_range(msc_lo, msc_hi, msc_bits);
}

// ===============================================================================
// Module 6: Main CGFE Algorithm
// ===============================================================================

/**
 * Main CGFE encoding algorithm
 * 
 * Algorithm steps:
 * 1. Compute MSC for s and e
 * 2. If same block -> encode TC range directly (with MSC parity)
 * 3. Compute Δ = msc_e - msc_s
 * 4. If Δ is odd -> single-side reflection strategy
 * 5. If Δ is even -> double-side reflection strategy
 * 
 * KEY INSIGHT: In CGFE, TC encoding is REFLECTED in odd MSC blocks.
 * The SAME ternary pattern matches DIFFERENT TC values in adjacent blocks:
 * - TC=t in MSC=k has the same encoding as TC=(BLOCK_SIZE-1-t) in MSC=k±1
 * 
 * This allows a single entry with MSC range to cover symmetric TC ranges!
 * This works for BOTH single-chunk and multi-chunk TC due to parity propagation.
 */
CGFEResult cgfe_encode_range(uint16_t s, uint16_t e, 
                              const CGFEConfig& config,
                              int skip_prefix_len) {
    CGFEResult result;
    
    if (s > e) {
        return result;
    }
    
    // Step 0: Handle partial encoding skip
    if (skip_prefix_len > (int)e) {
        return result;
    }
    
    // Step 1: Compute MSC and TC
    int msc_s = cgfe_msc(s, config);
    int msc_e = cgfe_msc(e, config);
    int tc_s = cgfe_tc(s, config);
    int tc_e = cgfe_tc(e, config);
    int bs = config.block_size();
    
    // Step 2: Same block case (recursive base)
    if (msc_s == msc_e) {
        bool msc_parity = (msc_s % 2 == 1);
        vector<string> tc_patterns = encode_tc_range(tc_s, tc_e, config, 0, msc_parity);
        
        for (const auto& tc_pat : tc_patterns) {
            CGFEEntry entry;
            entry.msc_lo = msc_s;
            entry.msc_hi = msc_s;
            entry.tc_pattern = tc_pat;
            entry.orig_lo = s;
            entry.orig_hi = e;
            result.entries.push_back(entry);
        }
        return result;
    }
    
    // Step 3: Multi-block case - use reflection extension
    // r1 = [s, end_of_block(msc_s)] -> TC range [tc_s, BLOCK_SIZE-1]
    // r3 = [start_of_block(msc_e), e] -> TC range [0, tc_e]
    // r2 = middle complete blocks (if any)
    
    int r1_tc_lo = tc_s;
    int r1_tc_hi = bs - 1;
    int r3_tc_lo = 0;
    int r3_tc_hi = tc_e;
    
    int delta = msc_e - msc_s;
    
    // =========================================================================
    // CGFE Reflection Extension Strategy
    // 
    // Key insight: A pattern encoded for TC value t in MSC=k will match:
    // - TC=t in all MSC with same parity as k
    // - TC=(BLOCK_SIZE-1-t) in all MSC with opposite parity
    // 
    // For Δ odd: MSC endpoints have opposite parity
    //   - r1 (TC=[tc_s, bs-1]) in msc_s, when extended to msc_e, covers TC=[0, bs-1-tc_s]
    //   - r3 needs TC=[0, tc_e] in msc_e
    //   - If tc_e <= bs-1-tc_s, r3 is fully covered by r1's reflection
    //   - Otherwise, need to encode remaining part of r3
    // 
    // IMPORTANT: To avoid over-coverage, we need to carefully split the ranges
    // =========================================================================
    
    if (delta % 2 == 1) {
        // Δ is odd: msc_s and msc_e have opposite parity
        
        bool r1_parity = (msc_s % 2 == 1);
        bool r3_parity = (msc_e % 2 == 1);
        
        // r1's TC range [tc_s, bs-1] reflects to [0, bs-1-tc_s] in msc_e
        int r1_sym_hi = bs - 1 - tc_s;  // r1 covers [0, r1_sym_hi] in msc_e
        
        // r3 needs [0, tc_e] in msc_e
        // Intersection: [0, min(r1_sym_hi, tc_e)]
        int common_hi = min(r1_sym_hi, tc_e);
        
        // Split r1 into:
        // - Part that reflects to cover r3: TC=[bs-1-common_hi, bs-1] in msc_s
        // - Part that doesn't help r3: TC=[tc_s, bs-1-common_hi-1] in msc_s (if any)
        
        int r1_extend_lo = bs - 1 - common_hi;  // Start of r1's extendable part
        int r1_alone_lo = tc_s;                  // Start of r1's alone part
        int r1_alone_hi = r1_extend_lo - 1;      // End of r1's alone part
        
        // 1. Encode r1's alone part (TC=[tc_s, bs-1-common_hi-1]) if it exists
        if (r1_alone_lo <= r1_alone_hi) {
            vector<string> alone_patterns = encode_tc_range(r1_alone_lo, r1_alone_hi, config, 0, r1_parity);
            for (const auto& tc_pat : alone_patterns) {
                CGFEEntry entry;
                entry.msc_lo = msc_s;
                entry.msc_hi = msc_s;
                entry.tc_pattern = tc_pat;
                entry.orig_lo = block_start(msc_s, config) + r1_alone_lo;
                entry.orig_hi = block_start(msc_s, config) + r1_alone_hi;
                result.entries.push_back(entry);
            }
        }
        
        // 2. Encode r1's extendable part with MSC range [msc_s, msc_e]
        // TC=[r1_extend_lo, bs-1] in msc_s reflects to [0, common_hi] in msc_e
        vector<string> extend_patterns = encode_tc_range(r1_extend_lo, bs - 1, config, 0, r1_parity);
        for (const auto& tc_pat : extend_patterns) {
            CGFEEntry entry;
            entry.msc_lo = msc_s;
            entry.msc_hi = msc_e;
            entry.tc_pattern = tc_pat;
            entry.orig_lo = s;
            entry.orig_hi = e;
            result.entries.push_back(entry);
        }
        
        // 3. Encode r3's remaining part if r1's reflection doesn't fully cover r3
        // r1 covers [0, r1_sym_hi] in msc_e, r3 needs [0, tc_e]
        // Remaining: [r1_sym_hi+1, tc_e] if tc_e > r1_sym_hi
        if (tc_e > r1_sym_hi) {
            int remaining_lo = r1_sym_hi + 1;
            int remaining_hi = tc_e;
            vector<string> remaining_patterns = encode_tc_range(remaining_lo, remaining_hi, config, 0, r3_parity);
            for (const auto& tc_pat : remaining_patterns) {
                CGFEEntry entry;
                entry.msc_lo = msc_e;
                entry.msc_hi = msc_e;
                entry.tc_pattern = tc_pat;
                entry.orig_lo = block_start(msc_e, config) + remaining_lo;
                entry.orig_hi = block_start(msc_e, config) + remaining_hi;
                result.entries.push_back(entry);
            }
        }
        
        // 4. Handle middle blocks (r2) if any
        for (int m = msc_s + 1; m <= msc_e - 1; m++) {
            bool m_parity = (m % 2 == 1);
            int covered_lo, covered_hi;
            
            if (m_parity == r1_parity) {
                // Same parity as msc_s: extendable part covers [r1_extend_lo, bs-1]
                covered_lo = r1_extend_lo;
                covered_hi = bs - 1;
            } else {
                // Opposite parity: extendable part covers [0, common_hi]
                covered_lo = 0;
                covered_hi = common_hi;
            }
            
            // Encode uncovered parts
            if (covered_lo > 0) {
                vector<string> left_patterns = encode_tc_range(0, covered_lo - 1, config, 0, m_parity);
                for (const auto& tc_pat : left_patterns) {
                    CGFEEntry entry;
                    entry.msc_lo = m;
                    entry.msc_hi = m;
                    entry.tc_pattern = tc_pat;
                    entry.orig_lo = block_start(m, config);
                    entry.orig_hi = block_start(m, config) + covered_lo - 1;
                    result.entries.push_back(entry);
                }
            }
            
            if (covered_hi < bs - 1) {
                vector<string> right_patterns = encode_tc_range(covered_hi + 1, bs - 1, config, 0, m_parity);
                for (const auto& tc_pat : right_patterns) {
                    CGFEEntry entry;
                    entry.msc_lo = m;
                    entry.msc_hi = m;
                    entry.tc_pattern = tc_pat;
                    entry.orig_lo = block_start(m, config) + covered_hi + 1;
                    entry.orig_hi = block_end(m, config);
                    result.entries.push_back(entry);
                }
            }
        }
        
        return result;
    }
    
    // Step 4: Δ is even - double-side reflection
    // msc_s and msc_e have same parity
    // Strategy: extend r1 leftward and r3 rightward
    {
        bool r1_parity = (msc_s % 2 == 1);
        bool r3_parity = (msc_e % 2 == 1);
        
        // r1 extends to cover [msc_s, msc_e-1]
        // In msc_e-1 (opposite parity), r1 covers [0, bs-1-tc_s]
        vector<string> r1_patterns = encode_tc_range(tc_s, bs - 1, config, 0, r1_parity);
        for (const auto& tc_pat : r1_patterns) {
            CGFEEntry entry;
            entry.msc_lo = msc_s;
            entry.msc_hi = msc_e - 1;
            entry.tc_pattern = tc_pat;
            entry.orig_lo = s;
            entry.orig_hi = block_end(msc_e - 1, config);
            result.entries.push_back(entry);
        }
        
        // r3 extends to cover [msc_s+1, msc_e]
        // In msc_s+1 (opposite parity), r3 covers [bs-1-tc_e, bs-1]
        vector<string> r3_patterns = encode_tc_range(0, tc_e, config, 0, r3_parity);
        for (const auto& tc_pat : r3_patterns) {
            CGFEEntry entry;
            entry.msc_lo = msc_s + 1;
            entry.msc_hi = msc_e;
            entry.tc_pattern = tc_pat;
            entry.orig_lo = block_start(msc_s + 1, config);
            entry.orig_hi = e;
            result.entries.push_back(entry);
        }
        
        return result;
    }
}

// ===============================================================================
// Module 7: Utility Functions
// ===============================================================================

/**
 * Print CGFE result for debugging
 */
void print_cgfe_result(const CGFEResult& result, const string& label) {
    if (!label.empty()) {
        cout << label << endl;
    }
    
    cout << "Total entries: " << result.entries.size() << endl;
    for (size_t i = 0; i < result.entries.size(); i++) {
        const auto& e = result.entries[i];
        cout << "  [" << i << "] MSC=[" << e.msc_lo << "," << e.msc_hi << "] "
             << "TC=" << e.tc_pattern 
             << " (orig: [" << e.orig_lo << "," << e.orig_hi << "])" << endl;
    }
}

/**
 * Convert CGFE entries to full ternary strings
 */
vector<string> cgfe_to_ternary(const CGFEResult& result, const CGFEConfig& config) {
    vector<string> ternary_list;
    
    for (const auto& entry : result.entries) {
        string msc_pat = encode_msc_range(entry.msc_lo, entry.msc_hi, config);
        string full = msc_pat + entry.tc_pattern;
        ternary_list.push_back(full);
    }
    
    return ternary_list;
}

// ===============================================================================
// Test Main (compile with -DDEMO_CGFE_MAIN)
// ===============================================================================

#ifdef DEMO_CGFE_MAIN
int main() {
    cout << "=== CGFE (Chunk-based Gray-code Factored Encoding) Test ===\n\n";
    
    // Configuration: W=6, c=2
    CGFEConfig config;
    config.W = 6;
    config.c = 2;
    
    cout << "Configuration: W=" << config.W << ", c=" << config.c << endl;
    cout << "BLOCK_SIZE = 2^(W-c) = 2^" << (config.W - config.c) << " = " << config.block_size() << endl;
    cout << "NUM_BLOCKS = 2^c = " << config.num_blocks() << endl;
    cout << endl;
    
    // =========================================================================
    // Test Case 0: Paper Example [6, 9] with W=4, c=2
    // =========================================================================
    cout << "*** Test 0: Paper Example [6, 9] (W=4, c=2) ***\n";
    CGFEConfig config4 = {4, 2};  // W=4, c=2, BLOCK_SIZE=4
    cout << "BLOCK_SIZE = " << config4.block_size() << endl;
    cout << "MSC(6) = 1, MSC(9) = 2 -> Δ = 1 (odd)\n";
    cout << "r1 = [6, 7] (TC=[2,3]), r3 = [8, 9] (TC=[0,1])\n";
    cout << "|r1| = 2, |r3| = 2 -> short = r1\n";
    cout << "Expected: [0*1 00*]\n\n";
    
    CGFEResult r0 = cgfe_encode_range(6, 9, config4);
    print_cgfe_result(r0, "Result:");
    
    vector<string> t0 = cgfe_to_ternary(r0, config4);
    cout << "Full ternary:\n";
    for (const auto& s : t0) {
        cout << "  " << s << endl;
    }
    cout << endl;
    
    // =========================================================================
    // Test Case 0.5: [2, 9] with W=4, c=2
    // =========================================================================
    cout << "*** Test 0.5: [2, 9] (W=4, c=2) ***\n";
    cout << "BLOCK_SIZE = " << config4.block_size() << endl;
    cout << "MSC(2) = 0, MSC(9) = 2 -> Δ = 2 (even)\n";
    cout << "r1 = [2, 3] (TC=[2,3]), r3 = [8, 9] (TC=[0,1])\n";
    cout << "Expected: [00* *11] [0*1 00*]\n\n";
    
    CGFEResult r05 = cgfe_encode_range(2, 9, config4);
    print_cgfe_result(r05, "Result:");
    
    vector<string> t05 = cgfe_to_ternary(r05, config4);
    cout << "Full ternary:\n";
    for (const auto& s : t05) {
        cout << "  " << s << endl;
    }
    cout << endl;
    
    // =========================================================================
    // Test Case 1: Same Block [18, 23] - Recursive Base
    // =========================================================================
    cout << "*** Test 1: Same Block [18, 23] ***\n";
    cout << "MSC(18) = 18/16 = 1, MSC(23) = 23/16 = 1 -> Same block\n";
    cout << "TC(18) = 2, TC(23) = 7 -> Encode TC range [2, 7]\n";
    cout << "Expected: 1-2 TC encodings, no reflection\n\n";
    
    CGFEResult r1 = cgfe_encode_range(18, 23, config);
    print_cgfe_result(r1, "Result:");
    
    vector<string> t1 = cgfe_to_ternary(r1, config);
    cout << "Full ternary:\n";
    for (const auto& s : t1) {
        cout << "  " << s << endl;
    }
    cout << endl;
    
    // =========================================================================
    // Test Case 2: Δ Odd [14, 53] - Single-side Reflection
    // =========================================================================
    cout << "*** Test 2: Δ Odd [14, 53] ***\n";
    cout << "MSC(14) = 0, MSC(53) = 3 -> Δ = 3 (odd)\n";
    cout << "r1 = [14, 15], r3 = [48, 53]\n";
    cout << "Expected: Single-side reflection, short side = r1\n\n";
    
    CGFEResult r2 = cgfe_encode_range(14, 53, config);
    print_cgfe_result(r2, "Result:");
    
    vector<string> t2 = cgfe_to_ternary(r2, config);
    cout << "Full ternary:\n";
    for (const auto& s : t2) {
        cout << "  " << s << endl;
    }
    cout << endl;
    
    // =========================================================================
    // Test Case 3: Δ Even [14, 45] - Double-side Reflection
    // =========================================================================
    cout << "*** Test 3: Δ Even [14, 45] ***\n";
    cout << "MSC(14) = 0, MSC(45) = 2 -> Δ = 2 (even)\n";
    cout << "r1 = [14, 15], r3 = [32, 45]\n";
    cout << "|r1| + |r3| = 2 + 14 = 16 >= BLOCK_SIZE(16)\n";
    cout << "Expected: Double-side reflection, r2 not explicitly encoded\n\n";
    
    CGFEResult r3 = cgfe_encode_range(14, 45, config);
    print_cgfe_result(r3, "Result:");
    
    vector<string> t3 = cgfe_to_ternary(r3, config);
    cout << "Full ternary:\n";
    for (const auto& s : t3) {
        cout << "  " << s << endl;
    }
    cout << endl;
    
    // =========================================================================
    // Extra Test: [26, 36] - Same as DIRPE test
    // =========================================================================
    cout << "*** Extra Test: [26, 36] ***\n";
    cout << "MSC(26) = 1, MSC(36) = 2 -> Δ = 1 (odd)\n";
    cout << "r1 = [26, 31], r3 = [32, 36]\n";
    cout << "Expected: Single-side reflection\n\n";
    
    CGFEResult r4 = cgfe_encode_range(26, 36, config);
    print_cgfe_result(r4, "Result:");
    
    vector<string> t4 = cgfe_to_ternary(r4, config);
    cout << "Full ternary:\n";
    for (const auto& s : t4) {
        cout << "  " << s << endl;
    }
    cout << endl;
    
    cout << "=== CGFE Test Complete ===\n";
    return 0;
}
#endif
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
    
    for (const auto& cport : cgfe_ports) {
        // Get all ternary patterns for source and destination ports
        auto src_patterns = cgfe_to_ternary(cport.src_cgfe, CGFEConfig{16, 2});
        auto dst_patterns = cgfe_to_ternary(cport.dst_cgfe, CGFEConfig{16, 2});
        
        // Create cartesian product of src/dst patterns
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
    
    // Header
    *out << "# CGFE (Chunk-based Gray-code Factored Encoding) TCAM Rules\n";
    *out << "# Format: SRC_IP DST_IP SRC_PORT DST_PORT PROTOCOL ACTION\n";
    *out << "#\n";
    
    // Generate TCAM entries by combining IP and port dimensions
    int entry_count = 0;
    for (const auto& ip_rule : ip_table) {
        for (const auto& port_entry : tcam_entries) {
            // Only combine matching priorities
            if (port_entry.priority != ip_rule.priority) {
                continue;
            }
            
            // Format IP addresses
            auto ip_to_string = [](uint32_t ip) -> std::string {
                return std::to_string((ip >> 24) & 0xFF) + "." +
                       std::to_string((ip >> 16) & 0xFF) + "." +
                       std::to_string((ip >> 8) & 0xFF) + "." +
                       std::to_string(ip & 0xFF);
            };
            
            std::string src_ip = ip_to_string(ip_rule.src_ip_lo);
            std::string dst_ip = ip_to_string(ip_rule.dst_ip_lo);
            
            *out << src_ip << " " 
                 << dst_ip << " "
                 << port_entry.src_pattern << " "
                 << port_entry.dst_pattern << " "
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