#include "Prefix_code.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <cmath>

static std::string ip_to_string(uint32_t ip)
{
    return std::to_string((ip >> 24) & 0xFF) + "." +
           std::to_string((ip >> 16) & 0xFF) + "." +
           std::to_string((ip >> 8) & 0xFF) + "." +
           std::to_string(ip & 0xFF);
}

std::vector<std::pair<uint16_t, uint16_t>> port_range_to_prefixes(uint16_t lo, uint16_t hi)
{
    std::vector<std::pair<uint16_t, uint16_t>> prefixes;
    if (lo > hi)
        return prefixes;

    uint32_t s = lo;
    while (s <= hi)
    {
        // Find largest block aligned at s and not exceeding hi
        uint32_t max_block = 1;
        int best_k = 0;
        for (int k = 0; k <= 16; ++k)
        {
            uint32_t block = (k == 16) ? (1u << 16) : (1u << k);
            if ((s % block) == 0 && (s + block - 1) <= hi)
            {
                max_block = block;
                best_k = k;
            }
            else
            {
                break;
            }
        }
        // Compute mask: high bits fixed, low best_k bits wildcard
        uint16_t mask = (best_k >= 16) ? 0 : (uint16_t)(0xFFFFu << best_k);

        prefixes.emplace_back((uint16_t)s, mask);
        s += max_block;
    }
    return prefixes;
}

void TCAM_Port_Expansion(
    const std::vector<Rule5D> &rules,
    std::vector<TCAM_Entry> &tcam_entries)
{
    tcam_entries.clear();
    std::cout << "[TCAM_Port_Expansion] Starting port range expansion...\n";

    size_t total_entries = 0;

    for (size_t rule_idx = 0; rule_idx < rules.size(); rule_idx++)
    {
        const auto &rule = rules[rule_idx];

        uint16_t src_port_lo = (uint16_t)rule.range[2][0];
        uint16_t src_port_hi = (uint16_t)rule.range[2][1];
        uint16_t dst_port_lo = (uint16_t)rule.range[3][0];
        uint16_t dst_port_hi = (uint16_t)rule.range[3][1];

        auto src_prefixes = port_range_to_prefixes(src_port_lo, src_port_hi);
        auto dst_prefixes = port_range_to_prefixes(dst_port_lo, dst_port_hi);

        for (const auto &src_prefix : src_prefixes)
        {
            for (const auto &dst_prefix : dst_prefixes)
            {
                TCAM_Entry entry;
                entry.Src_IP_lo = rule.range[0][0];
                entry.Src_IP_hi = rule.range[0][1];
                entry.Dst_IP_lo = rule.range[1][0];
                entry.Dst_IP_hi = rule.range[1][1];
                entry.Src_Port_prefix = src_prefix.first;
                entry.Src_Port_mask = src_prefix.second;
                entry.Dst_Port_prefix = dst_prefix.first;
                entry.Dst_Port_mask = dst_prefix.second;
                entry.Proto = (uint8_t)rule.range[4][0];
                entry.action = rule.action; // use action string from Rule5D
                entry.rule_id = rule_idx;

                tcam_entries.push_back(entry);
                total_entries++;
            }
        }
    }

    std::cout << "[TCAM_Port_Expansion] Expansion completed: "
              << rules.size() << " rules -> "
              << tcam_entries.size() << " TCAM entries\n";
    std::cout << "[TCAM_Port_Expansion] Average expansion ratio: "
              << std::fixed << std::setprecision(2)
              << (rules.empty() ? 0.0 : (double)tcam_entries.size() / rules.size()) << "x\n";
}

static std::string port_to_binary_with_mask(uint16_t prefix, uint16_t mask)
{
    std::string result;
    for (int i = 15; i >= 0; --i)
    {
        uint16_t bit = (prefix >> i) & 1;
        uint16_t mask_bit = (mask >> i) & 1;

        if (mask_bit == 1)
        {
            // Bit is fixed
            result += (bit ? '1' : '0');
        }
        else
        {
            // Bit is wildcard
            result += '*';
        }
    }
    return result;
}

void print_prefix_tcam_rules(
    const std::vector<TCAM_Entry> &tcam_entries,
    const std::vector<Rule5D> &rules,
    const std::string &output_file)
{
    std::ostream *out = &std::cout;
    std::ofstream outf;
    if (!output_file.empty())
    {
        // ensure directory exists
        size_t pos = output_file.find_last_of('/');
        if (pos != std::string::npos)
        {
            std::string dir = output_file.substr(0, pos);
            system(("mkdir -p " + dir).c_str());
        }
        outf.open(output_file);
        if (outf.is_open())
            out = &outf;
    }

    *out << "=== Prefix Coding (Binary Port Expansion) TCAM Rules ===\n\n";

    for (const auto &e : tcam_entries)
    {
        const auto &rule = rules[e.rule_id];
        int src_mask_len = rule.prefix_length[0];
        int dst_mask_len = rule.prefix_length[1];

        std::string src_ip = ip_to_string(e.Src_IP_lo) + "/" + std::to_string(src_mask_len);
        std::string dst_ip = ip_to_string(e.Dst_IP_lo) + "/" + std::to_string(dst_mask_len);

        // Convert port prefix/mask to binary with wildcards
        std::string src_port_binary = port_to_binary_with_mask(e.Src_Port_prefix, e.Src_Port_mask);
        std::string dst_port_binary = port_to_binary_with_mask(e.Dst_Port_prefix, e.Dst_Port_mask);

        *out << "@" << src_ip << " " << dst_ip << " "
             << src_port_binary << " " << dst_port_binary << " "
             << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)e.Proto << std::dec << "/0xFF "
             << e.action << "\n";
    }

    *out << "\n=== Total TCAM Entries: " << tcam_entries.size() << " ===\n";

    if (outf.is_open())
        outf.close();
}
