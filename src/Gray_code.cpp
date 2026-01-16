/** *************************************************************/
// @Name: Gray_code.cpp
// @Function: SRGE - Symmetric Range Gray Encoding (Paper Implementation)
// @Author: weijzh (weijzh@pcl.ac.cn)
// @Created: 2026-01-15
/************************************************************* */

#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <bitset>
#include <algorithm>
#include "Gray_code.hpp"
#include "Loader.hpp"

using namespace std;

/*
================================================================================
SRGE (Symmetric Range Gray Encoding) 

================================================================================
*/

// ============================================================
// Module 1: Gray Code Conversion (基础转换函数)
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

// Gray 顺序的前驱 (不是 Gray code 的 -1，而是 binary 值的 -1 对应的 Gray code)
uint16_t gray_prev(uint16_t g) {
    uint16_t b = gray_to_binary(g);
    return binary_to_gray(b - 1);
}

// Gray 顺序的后继 (不是 Gray code 的 +1，而是 binary 值的 +1 对应的 Gray code)
uint16_t gray_next(uint16_t g) {
    uint16_t b = gray_to_binary(g);
    return binary_to_gray(b + 1);
}

// ============================================================
// Module 2: Utility Functions
// ============================================================

string bitset_to_string(const bitset<16>& bs, int bits) {
    string s = bs.to_string();
    return s.substr(16 - bits);
}

string gray_to_string(uint16_t g, int bits) {
    return bitset_to_string(bitset<16>(g), bits);
}

void print_srge_result(const SRGEResult& result, const string& label) {
    cout << label << " result (" << result.ternary_entries.size() << " entries):" << endl;
    for (const auto& entry : result.ternary_entries) {
        cout << "  " << entry << endl;
    }
}

// ============================================================
// Module 3: Gray Code LCA Computation (Gray 码公共前缀)
// ============================================================

/**
 * @brief 计算两个 Gray 码的最深公共祖先 (最长公共前缀)
 * 
 * 注意：这是 Gray code 的公共前缀，不是 binary 的！
 * 
 * @param sg 起始 Gray 码
 * @param eg 结束 Gray 码  
 * @param bits 位宽
 * @return LCA 深度 (公共前缀长度，0 表示第一位就不同)
 */
int compute_deepest_gray_lca(uint16_t sg, uint16_t eg, int bits) {
    // 在 Gray code 表示中，找第一个不同的位
    for (int i = bits - 1; i >= 0; --i) {
        if (((sg >> i) & 1) != ((eg >> i) & 1)) {
            return bits - 1 - i;  // 公共前缀长度
        }
    }
    return bits;  // 完全相同
}

/**
 * @brief 计算 Gray 码区间的反射伙伴
 * 
 * 给定一个 Gray 码，计算它关于某一位的反射
 * 
 * @param g Gray 码
 * @param reflect_bit 反射位 (从高位开始计数)
 * @param bits 总位宽
 * @return 反射后的 Gray 码
 */
uint16_t gray_reflect(uint16_t g, int reflect_bit, int bits) {
    // 翻转指定位
    int bit_pos = bits - 1 - reflect_bit;
    return g ^ (1u << bit_pos);
}

/**
 * @brief 计算以 LCA 为中心的 pivot
 * 
 * pivot 是 Gray tree 中 LCA 节点的分界点
 * 在 Gray 码域中，pivot 是左子树的最后一个值的下一个
 * 
 * @param sg 起始 Gray 码
 * @param eg 结束 Gray 码
 * @param lca_depth LCA 深度
 * @param bits 位宽
 * @return pivot 的 binary 值
 */
uint16_t compute_pivot_binary(uint16_t sg, uint16_t eg, int lca_depth, int bits) {
    (void)eg;
    // LCA 深度对应的位是第一个不同的位
    // pivot 是右子树的起点，即 binary 域中的 2^(bits - lca_depth - 1) 的倍数
    
    uint16_t bs = gray_to_binary(sg);
    int split_bit = bits - lca_depth - 1;  // 分裂位在 binary 中的位置
    
    // pivot_binary 是大于等于 bs 的、在 split_bit 位为 1 的最小值
    uint16_t mask = (1u << split_bit);
    uint16_t pivot = (bs & ~(mask - 1)) | mask;
    
    return pivot;
}

// ============================================================
// Module 4: Pivot & Interval Split
// ============================================================

/**
 * @brief 根据 binary pivot 将 Gray 区间分裂为左右子区间
 * 
 * @param sg 起始 Gray 码
 * @param eg 结束 Gray 码
 * @param pivot_binary 分裂点 (binary 值)
 * @param left_start, left_end 左子区间 (输出，binary 值)
 * @param right_start, right_end 右子区间 (输出，binary 值)
 */
void split_by_pivot(
    uint16_t sg, uint16_t eg, uint16_t pivot_binary,
    uint16_t& left_bs, uint16_t& left_be,
    uint16_t& right_bs, uint16_t& right_be
) {
    uint16_t bs = gray_to_binary(sg);
    uint16_t be = gray_to_binary(eg);
    
    // Left = [bs, pivot - 1]
    left_bs = bs;
    left_be = pivot_binary - 1;
    
    // Right = [pivot, be]
    right_bs = pivot_binary;
    right_be = be;
}

// ============================================================
// Module 5: Reflection Merge (论文核心)
// ============================================================

/**
 * @brief 计算 Gray 区间的大小 (binary 域)
 */
uint32_t binary_range_size(uint16_t bs, uint16_t be) {
    if (bs > be) return 0;
    return (uint32_t)(be - bs + 1);
}

/**
 * @brief 构建覆盖区间的 ternary pattern (Gray code 形式)
 */
string build_pattern_for_range(uint16_t bs, uint16_t be, int bits) {
    if (bs > be) return "";
    
    // 计算区间内所有 Gray code 的共同特征
    uint16_t all_ones = 0xFFFF;
    uint16_t all_zeros = 0x0000;
    
    for (uint16_t b = bs; b <= be; ++b) {
        uint16_t g = binary_to_gray(b);
        all_ones &= g;
        all_zeros |= g;
    }
    
    string pattern;
    for (int i = bits - 1; i >= 0; --i) {
        bool is_one = (all_ones >> i) & 1;
        bool is_zero = !((all_zeros >> i) & 1);
        
        if (is_one) pattern += '1';
        else if (is_zero) pattern += '0';
        else pattern += '*';
    }
    
    return pattern;
}

/**
 * @brief 检查一个 binary 区间是否形成有效的 Gray hypercube
 * 
 * 有效的 hypercube 意味着区间内所有 Gray codes 只在固定的几位上变化
 * 
 * @return true 如果是有效的 hypercube
 */
bool is_valid_gray_hypercube(uint16_t bs, uint16_t be, int bits) {
    if (bs > be) return false;
    uint32_t size = be - bs + 1;
    
    // 计算区间内 Gray codes 的变化位
    uint16_t all_ones = 0xFFFF;
    uint16_t all_zeros = 0x0000;
    
    for (uint16_t b = bs; b <= be; ++b) {
        uint16_t g = binary_to_gray(b);
        all_ones &= g;
        all_zeros |= g;
    }
    
    // wildcard 位 = all_zeros 中为 1 且 all_ones 中为 0 的位
    uint16_t wildcard_bits = all_zeros & ~all_ones;
    int num_wildcards = __builtin_popcount(wildcard_bits);
    
    // 有效的 hypercube: size == 2^num_wildcards
    return size == (1u << num_wildcards);
}

/**
 * @brief 在给定区间中找到从起点开始的最大有效 Gray hypercube
 * 
 * @param bs 起始 binary 值
 * @param be 结束 binary 值 (上界)
 * @param bits 位宽
 * @return 最大有效 hypercube 的结束位置
 */
uint16_t find_max_hypercube_from_start(uint16_t bs, uint16_t be, int bits) {
    // 从最大可能的 2 的幂次开始尝试
    uint32_t max_size = be - bs + 1;
    
    // 找到不超过 max_size 的最大 2 的幂次
    uint32_t try_size = 1;
    while (try_size * 2 <= max_size) {
        try_size *= 2;
    }
    
    // 从大到小尝试找有效的 hypercube
    while (try_size >= 1) {
        uint16_t try_end = bs + try_size - 1;
        if (try_end <= be && is_valid_gray_hypercube(bs, try_end, bits)) {
            return try_end;
        }
        try_size /= 2;
    }
    
    return bs;  // 至少返回单点
}

// ============================================================
// Module 6: SRGE 主递归函数 (Top-down 贪心 + 反射消耗)
// ============================================================

/**
 * @brief 在 binary 区间内从起点贪心找最大 Gray hypercube
 * 
 * @param bs 起始 binary 值
 * @param be 结束 binary 值
 * @param bits 位宽
 * @return 最大 hypercube 的结束位置
 */
uint16_t greedy_max_hypercube(uint16_t bs, uint16_t be, int bits) {
    if (bs > be) return bs;
    
    // 从最大可能的 2 的幂次开始尝试
    uint32_t max_size = be - bs + 1;
    uint32_t try_size = 1;
    while (try_size * 2 <= max_size) {
        try_size *= 2;
    }
    
    // 从大到小尝试
    while (try_size >= 1) {
        uint16_t try_end = bs + try_size - 1;
        if (try_end <= be && is_valid_gray_hypercube(bs, try_end, bits)) {
            return try_end;
        }
        try_size /= 2;
    }
    
    return bs;  // 至少单点
}

/**
 * @brief SRGE 递归实现 - Top-down 贪心 + 反射消耗
 * 
 * 算法流程：
 * 1. 计算 Gray LCA，分裂成左右区间
 * 2. 选择较短的一边进行内部贪心合并
 * 3. 将合并结果关于 LCA 反射 → 输出 pattern，同时消耗另一侧对称区间
 * 4. 递归处理两侧剩余区间
 * 
 * @param bs 起始 binary 值
 * @param be 结束 binary 值
 * @param bits 位宽
 * @param results 输出 pattern 集合
 */
void srge_recursive_impl(uint16_t bs, uint16_t be, int bits, vector<string>& results) {
    if (bs > be) return;
    
    // Base case: 单点
    if (bs == be) {
        results.push_back(gray_to_string(binary_to_gray(bs), bits));
        return;
    }
    
    // Base case: 整个区间是 hypercube
    if (is_valid_gray_hypercube(bs, be, bits)) {
        results.push_back(build_pattern_for_range(bs, be, bits));
        return;
    }
    
    // 计算 LCA 和分裂点
    uint16_t sg = binary_to_gray(bs);
    uint16_t eg = binary_to_gray(be);
    int lca_depth = compute_deepest_gray_lca(sg, eg, bits);
    int flip_bit_pos = bits - 1 - lca_depth;
    
    // 找 pivot: Gray 序列中第一个在 lca_depth 位与 sg 不同的值
    int sg_bit = (sg >> flip_bit_pos) & 1;
    uint16_t pivot = 0xFFFF;
    for (uint16_t b = bs; b <= be; b++) {
        uint16_t g = binary_to_gray(b);
        if (((g >> flip_bit_pos) & 1) != sg_bit) {
            pivot = b;
            break;
        }
    }
    
    if (pivot == 0xFFFF) {
        // 无法分裂，作为 hypercube 输出
        results.push_back(build_pattern_for_range(bs, be, bits));
        return;
    }
    
    // 分裂成左右区间
    uint16_t left_bs = bs, left_be = pivot - 1;
    uint16_t right_bs = pivot, right_be = be;
    
    uint32_t left_size = left_be - left_bs + 1;
    uint32_t right_size = right_be - right_bs + 1;
    
    // 选择短的一边进行贪心合并
    bool process_right_first = (right_size <= left_size);
    
    if (process_right_first) {
        // 在右侧贪心合并
        uint16_t merge_end = greedy_max_hypercube(right_bs, right_be, bits);
        uint32_t merge_size = merge_end - right_bs + 1;
        
        // 生成 pattern 并反射
        string pattern = build_pattern_for_range(right_bs, merge_end, bits);
        pattern[lca_depth] = '*';  // 反射：将 LCA 位变为 wildcard
        results.push_back(pattern);
        
        // 左侧消耗对称部分：从右往左数 merge_size 个
        // 左侧剩余 [left_bs, left_be - merge_size]
        uint16_t left_remain_be = left_be - merge_size;
        
        // 右侧剩余 [merge_end + 1, right_be]
        uint16_t right_remain_bs = merge_end + 1;
        
        bool has_left = (left_remain_be >= left_bs);
        bool has_right = (right_remain_bs <= right_be);
        
        // 关键：如果两侧都有剩余，尝试继续反射合并
        if (has_left && has_right) {
            // 对右侧剩余进行贪心，看能否和左侧部分反射合并
            uint16_t r_merge_end = greedy_max_hypercube(right_remain_bs, right_be, bits);
            uint32_t r_merge_size = r_merge_end - right_remain_bs + 1;
            
            // 检查左侧是否有足够的对称部分
            if (r_merge_size <= (left_remain_be - left_bs + 1)) {
                // 左侧对称部分：[left_remain_be - r_merge_size + 1, left_remain_be]
                uint16_t l_sym_bs = left_remain_be - r_merge_size + 1;
                
                // 检查是否能反射合并
                string r_pat = build_pattern_for_range(right_remain_bs, r_merge_end, bits);
                string l_pat = build_pattern_for_range(l_sym_bs, left_remain_be, bits);
                
                // 计算两个区间的 LCA
                uint16_t r_g = binary_to_gray(right_remain_bs);
                uint16_t l_g = binary_to_gray(l_sym_bs);
                int sub_lca = compute_deepest_gray_lca(l_g, r_g, bits);
                
                // 检查除 sub_lca 位外其他位是否相同
                bool can_merge = true;
                for (int i = 0; i < bits; i++) {
                    if (i == sub_lca) continue;
                    if (l_pat[i] != r_pat[i]) {
                        can_merge = false;
                        break;
                    }
                }
                
                if (can_merge) {
                    // 合并并反射
                    l_pat[sub_lca] = '*';
                    results.push_back(l_pat);
                    
                    // 处理左侧剩余的剩余
                    if (l_sym_bs > left_bs) {
                        srge_recursive_impl(left_bs, l_sym_bs - 1, bits, results);
                    }
                    
                    // 处理右侧剩余的剩余
                    if (r_merge_end < right_be) {
                        srge_recursive_impl(r_merge_end + 1, right_be, bits, results);
                    }
                    return;
                }
            }
            
            // 无法反射合并，分别递归
            srge_recursive_impl(left_bs, left_remain_be, bits, results);
            srge_recursive_impl(right_remain_bs, right_be, bits, results);
        } else if (has_left) {
            srge_recursive_impl(left_bs, left_remain_be, bits, results);
        } else if (has_right) {
            srge_recursive_impl(right_remain_bs, right_be, bits, results);
        }
    } else {
        // 在左侧贪心合并
        uint16_t merge_end = greedy_max_hypercube(left_bs, left_be, bits);
        uint32_t merge_size = merge_end - left_bs + 1;
        
        // 生成 pattern 并反射
        string pattern = build_pattern_for_range(left_bs, merge_end, bits);
        pattern[lca_depth] = '*';  // 反射
        results.push_back(pattern);
        
        // 右侧消耗对称部分：从左往右数 merge_size 个
        // 左侧剩余 [merge_end + 1, left_be]
        uint16_t left_remain_bs = merge_end + 1;
        
        // 右侧剩余 [right_bs + merge_size, right_be]
        uint16_t right_remain_bs = right_bs + merge_size;
        
        // 递归处理剩余
        if (left_remain_bs <= left_be) {
            if (right_remain_bs <= right_be) {
                srge_recursive_impl(left_remain_bs, left_be, bits, results);
                srge_recursive_impl(right_remain_bs, right_be, bits, results);
            } else {
                srge_recursive_impl(left_remain_bs, left_be, bits, results);
            }
        } else {
            if (right_remain_bs <= right_be) {
                srge_recursive_impl(right_remain_bs, right_be, bits, results);
            }
        }
    }
}

// 旧接口的包装
void srge_recursive(uint16_t sg, uint16_t eg, int bits, vector<string>& results) {
    uint16_t bs = gray_to_binary(sg);
    uint16_t be = gray_to_binary(eg);
    srge_recursive_impl(bs, be, bits, results);
}

// ============================================================
// Module 7: SRGE 主入口
// ============================================================

/**
 * @brief SRGE 编码主入口 - 将二进制范围编码为 ternary pattern 集合
 * 
 * @param sb 二进制起始值
 * @param eb 二进制结束值
 * @param bits 位宽 (默认 16)
 * @return SRGEResult 包含 ternary pattern 集合
 */
SRGEResult srge_encode(uint16_t sb, uint16_t eb, int bits) {
    SRGEResult result;
    
    if (sb > eb) {
        return result;  // 空范围
    }

    // ✅ 特殊值：整个域，直接 wildcard
    uint32_t max_val = (1u << bits) - 1;
    if (sb == 0 && eb == max_val) {
        result.ternary_entries.push_back(string(bits, '*'));
        return result;
    }    
    
    // 转换为 Gray 码区间
    uint16_t sg = binary_to_gray(sb);
    uint16_t eg = binary_to_gray(eb);
    
    // 注意: binary_to_gray 保持单调性，所以 [sb, eb] 对应连续 Gray 码区间
    // 但 Gray 码的 "连续" 定义需要论文明确
    
    // 调用递归分解
    srge_recursive(sg, eg, bits, result.ternary_entries);
    
    return result;
}

// ============================================================
// Module 8: Port Table SRGE Processing
// ============================================================

vector<GrayCodedPort> SRGE(const vector<PortRule> &port_table) {
    vector<GrayCodedPort> results;
    
    for (const auto &rule : port_table) {
        GrayCodedPort gcp;
        
        // 保存原始端口范围
        gcp.src_port_lo = rule.src_port_lo;
        gcp.src_port_hi = rule.src_port_hi;
        gcp.dst_port_lo = rule.dst_port_lo;
        gcp.dst_port_hi = rule.dst_port_hi;
        
        // 计算 Gray 码
        gcp.src_port_lo_gray_bs = bitset<16>(binary_to_gray(rule.src_port_lo));
        gcp.src_port_hi_gray_bs = bitset<16>(binary_to_gray(rule.src_port_hi));
        gcp.dst_port_lo_gray_bs = bitset<16>(binary_to_gray(rule.dst_port_lo));
        gcp.dst_port_hi_gray_bs = bitset<16>(binary_to_gray(rule.dst_port_hi));
        
        // 应用 SRGE 编码
        gcp.src_srge = srge_encode(rule.src_port_lo, rule.src_port_hi, GRAY_BITS);
        gcp.dst_srge = srge_encode(rule.dst_port_lo, rule.dst_port_hi, GRAY_BITS);
        
        // 复制其他字段
        gcp.priority = rule.priority;
        gcp.action = rule.action;
        
        results.push_back(gcp);
    }
    
    return results;
}



// ===============================================================================
// Module 6: TCAM Entry Generation
// ===============================================================================

std::vector<GrayTCAM_Entry> generate_tcam_entries(const std::vector<GrayCodedPort>& gray_ports) {
    std::vector<GrayTCAM_Entry> tcam_entries;
    
    for (const auto& gp : gray_ports) {
        // Cartesian product: each src_pattern × each dst_pattern
        for (const auto& src_pat : gp.src_srge.ternary_entries) {
            for (const auto& dst_pat : gp.dst_srge.ternary_entries) {
                GrayTCAM_Entry entry;
                entry.src_pattern = src_pat;
                entry.dst_pattern = dst_pat;
                entry.priority = gp.priority;
                entry.action = gp.action;
                tcam_entries.push_back(entry);
            }
        }
    }
    
    return tcam_entries;
}

// ===============================================================================
// Module 7: Output Functions
// ===============================================================================

void print_tcam_rules(const std::vector<GrayTCAM_Entry>& tcam_entries, 
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
    
    *out_stream << "=== TCAM Rules (Gray Code Ternary Format) ===\n\n";
    
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
        
        // Source port pattern (only last 4 bits for readability if 16-bit)
        std::string src_short = entry.src_pattern.substr(entry.src_pattern.length() - 4);
        *out_stream << src_short << "  ";
        
        // Destination port pattern
        std::string dst_short = entry.dst_pattern.substr(entry.dst_pattern.length() - 4);
        *out_stream << dst_short << "   ";
        
        // Protocol
        *out_stream << "0x" << std::hex << std::setw(2) << std::setfill('0') 
                    << (int)ip_rule->proto << "/0xFF   ";
        
        // Action
        *out_stream << std::dec << entry.action;
        
        *out_stream << "\n";
    }
    
    *out_stream << "\n=== Total TCAM Entries: " << tcam_entries.size() << " ===\n";
    
    if (file_stream.is_open()) {
        file_stream.close();
    }
}

// ============================================================
// Test Main 
// ============================================================

#ifdef DEMO_GrayCode_MAIN
int main() {
    cout << "===== SRGE  =====\n\n";
    
    // 测试 Gray 码转换
    cout << "Gray 码转换测试 (4-bit):\n";
    cout << "Binary -> Gray -> Binary\n";
    for (int i = 0; i <= 15; i++) {
        uint16_t g = binary_to_gray(i);
        cout << "  " << i << " -> " << bitset<4>(g) << " -> " << gray_to_binary(g) << endl;
    }
    cout << endl;
    
    // 测试 LCA 计算
    cout << "LCA 计算测试 (Gray code 公共前缀深度):\n";
    auto test_lca = [](int b1, int b2, int bits) {
        uint16_t g1 = binary_to_gray(b1);
        uint16_t g2 = binary_to_gray(b2);
        int lca = compute_deepest_gray_lca(g1, g2, bits);
        cout << "  binary [" << b1 << ", " << b2 << "]"
             << " gray [" << bitset<4>(g1) << ", " << bitset<4>(g2) << "]"
             << " -> LCA depth = " << lca << endl;
    };
    test_lca(6, 14, 4);   // 0101, 1001 -> 第一位不同，depth=0
    test_lca(1, 13, 4);   // 0001, 1011 -> 第一位不同，depth=0
    test_lca(0, 7, 4);    // 0000, 0100 -> 第一位相同，depth>=1
    test_lca(8, 15, 4);   // 1100, 1000 -> 第一位相同，depth>=1
    test_lca(0, 15, 4);   // 0000, 1000 -> 第一位不同，depth=0
    cout << endl;
    
    // 测试 SRGE 编码
    cout << "SRGE 编码测试:\n";
    
    auto test_srge = [](int sb, int eb, int bits) {
        cout << "\n--- Binary range [" << sb << ", " << eb << "] ---\n";
        SRGEResult result = srge_encode(sb, eb, bits);
        cout << "Result (" << result.ternary_entries.size() << " entries):\n";
        for (const auto& e : result.ternary_entries) {
            cout << "  " << e << endl;
        }
    };
    
    test_srge(6, 14, 4);
    cout << "Expected: *10*, 1*1*, 1*01 (3 entries)\n";
    
    test_srge(1, 13, 4);
    cout << "Expected: *1**, *01*, 0001 (3 entries)\n";
    
    test_srge(0, 7, 4);
    cout << "Expected: 0*** (1 entry - complete left subtree)\n";
    
    test_srge(0, 15, 4);
    cout << "Expected: **** (1 entry - complete tree)\n";

    test_srge(1, 6, 4);
    cout << "Expected: 0*1*, 0*01 (2 entry - complete left subtree)\n";
    
    return 0;
}
#endif