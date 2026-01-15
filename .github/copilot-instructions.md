# CGFE Codebase Guide

## Project Overview
CGFE is a **5-dimensional packet classification/firewall rule processing system** that splits network ACL rules into IP tables and Port tables for optimized lookup. The system processes rules with 5 dimensions: source IP, destination IP, source port, destination port, and protocol.

## Core Architecture

### Rule Processing Pipeline
1. **Load**: Parse `.rules` files into `Rule5D` structures ([Loader.cpp](../src/Loader.cpp))
2. **Split**: Decompose 5D rules into separate IP and Port tables ([Loader.hpp](../src/Loader.hpp))
3. **Process**: Main logic in [CGFE.cpp](../src/CGFE.cpp) (currently empty, under development)

### Data Structures (Loader.hpp)
- **Rule5D**: Full 5-dimensional rule with IP/port ranges, protocol, priority, and action
  - `range[d][0/1]`: low/high bounds for each dimension (d=0-4)
  - Action format: `"0x1000/0x1000"` or `"0x0000/0x0200"` (string, not hex)
- **IPRule**: IP-specific fields (src/dst IP ranges, protocol, prefix lengths)
  - Includes `merged_R` vector for tracking merged rules and `rmax_id` field
- **PortRule**: Port-specific fields (src/dst port ranges, action string, priority)

### Rule File Format
Tab or space-separated ACL rules following this pattern:
```
@SRC_IP/MASK    DST_IP/MASK    SPORT_LO : SPORT_HI    DPORT_LO : DPORT_HI    PROTO/PROTO_MASK    ACTION
```

Example:
```
@171.102.10.228/31    34.228.25.12/32    0 : 65535    1712 : 1712    0x06/0xFF    0x1000/0x1000
```

## Key Implementation Details

### IP Range Calculation
- CIDR notation (e.g., `/24`) converted to inclusive ranges: `[low, high]`
- `/32` represents single IP points (lo == hi) - critical for line intersection tests
- IP addresses stored as `uint32_t` in big-endian order: `(a<<24)|(b<<16)|(c<<8)|d`

### Prefix Length Semantics
- IP dimensions: Store actual CIDR mask length (0-32)
- Port dimensions: `0` = exact match (lo==hi), `1` = range (lo<hi)
- Protocol: `0` = exact, `1` = wildcard (mask 0x00)

### Priority Rules
- Higher priority value = more specific rule (processed first)
- Priority assigned sequentially during load (rule_count++)
- Preserved across IP/Port table split

### Action Field Evolution
The codebase handles **both legacy and current formats**:
- **Current**: String format `"0x1000/0x1000"` stored in `Rule5D.action` and `PortRule.action`
- **Legacy**: Separate `action_flags/action_mask` integers (auto-converted to string)

## Test Files & Validation

### Test Rule Sets (`src/ACL_rules/test-rules/`)
- **Line_test.rules**: 10 rules testing `/32` point intersections (line detection)
- **Point_test.rules**: Point-in-rectangle containment tests
- **Region_test.rules**: 2D rectangle overlap scenarios

Each test set has accompanying README with expected results and visual diagrams.

### Production Rule Sets
Organized by source type (acl1-5, fw1-5, ipc1-2) with naming pattern:
```
{type}_{size}_{variant}_{density}.rules
```
- size: `50k`, `100k` (number of rules)
- variant: `16` (unclear meaning - check history)
- density: `0.5` to `0.9` (rule overlap/intersection ratio)
- suffix `_c`: Unknown variant (check commit history)

## Build & Development

### Compilation
No build system currently present. Expected manual compilation:
```bash
g++ -std=c++17 -O2 src/CGFE.cpp src/Loader.cpp -o cgfe
```

### Code Organization
- **src/**: All source code (no separate include directory)
- **src/ACL_rules/**: Test and production rule files (gitignored)
- **.history/**: VS Code local history (gitignored)

### Debugging with Test Data
Use small test sets first to validate changes:
```bash
./cgfe src/ACL_rules/test-rules/Line_test.rules
```

## Important Conventions

### Error Handling
- File I/O: Exit with `stderr` message if file cannot be opened
- Invalid rules: Print warning to `stderr` and **skip** (don't exit)
- Validation checks: IP octets (0-255), ports (0-65535), port ordering (lo≤hi)

### Memory Management
- Uses modern C++ containers (`std::vector`, `std::array`)
- No raw `new`/`delete` - rely on RAII
- Pass large structures by const reference

### Naming Patterns
- Lowercase with underscores: `load_rules_from_file`, `ip_table`
- Type aliases: `using u32 = uint32_t;`
- Range variables: `_lo`/`_hi` suffixes for low/high bounds

## Next Steps for Development
The main processing logic in [CGFE.cpp](../src/CGFE.cpp) is currently empty. Expected implementation:
1. CLI argument parsing for rule file input
2. Call `load_rules_from_file()` and `split_rules()`
3. Build IP classification structure (interval tree, decision tree, or geometric approach)
4. Implement lookup/query function for packet classification
5. Add performance benchmarking against rule sets
