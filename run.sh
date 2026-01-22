#!/bin/bash
# 完整的编译和运行脚本
# 用途：一键编译和运行 CGFE 程序

set -euo pipefail  # 任何错误就停止，未定义变量也视为错误

# 始终定位到脚本所在目录，避免大小写或路径不一致问题
PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_ROOT"

echo "=============================================================================="
echo "---------------------------CGFE implementation--------------------------------"
echo "=============================================================================="
echo ""

# 选择可用的编译器，优先 g++-11，其次 g++
CXX_BIN="${CXX:-g++-11}"
if ! command -v "$CXX_BIN" >/dev/null 2>&1; then
    if command -v g++ >/dev/null 2>&1; then
        CXX_BIN="g++"
    else
        echo "✗ 未找到可用的 g++ 编译器，请安装 g++ 或 g++-11"
        exit 1
    fi
fi
echo "使用编译器: $CXX_BIN"

# 1. 清理旧的二进制文件
echo "[Step 1] Cleaning old binaries..."
rm -f src/CGFE
echo "✓ 清理完成"
echo ""

# 2. 编译
echo "[2/5] 编译 ($CXX_BIN -std=c++17)..."
"$CXX_BIN" -std=c++17 -fdiagnostics-color=always -g \
    src/main.cpp \
    src/CGFE_code.cpp \
    src/Gray_code.cpp \
    src/Chunk_code.cpp \
    src/Loader.cpp \
    src/Prefix_code.cpp \
    -o src/CGFE 2>&1
echo "✓ 编译成功"
echo ""

# 3. 验证二进制文件存在
echo "[3/5] 验证二进制文件..."
if [ -f src/CGFE ]; then
    ls -lh src/CGFE
    echo "✓ 二进制文件已生成"
else
    echo "✗ 错误：二进制文件未生成！"
    exit 1
fi
echo ""

# 4. 运行程序（使用 C++ 代码中的默认规则文件）
echo "[4/5] 运行程序..."
echo "提示：使用 CGFE.cpp 中定义的默认规则文件"
echo "     如需指定规则文件，请运行: ./src/CGFE <规则文件路径>"
echo "================================= 程序输出 ===================================="
./src/CGFE
echo "================================= 程序完成 ===================================="
echo ""

# # 5. 检查输出文件
# echo "[5/5] 检查生成的输出文件..."
# if [ -f src/output/final_ip_table_cidr.txt ]; then
#     echo "✓ final_ip_table_cidr.txt ($(wc -l < src/output/final_ip_table_cidr.txt) 行)"
# fi
# if [ -f src/output/meta_merged.txt ]; then
#     echo "✓ meta_merged.txt ($(wc -l < src/output/meta_merged.txt) 行)"
# fi
# if [ -f src/output/SRC_TCAM_Table.txt ]; then
#     echo "✓ SRC_TCAM_Table.txt ($(wc -l < src/output/SRC_TCAM_Table.txt) 行)"
# fi
# if [ -f src/output/SRC_SRAM_Table.txt ]; then
#     echo "✓ SRC_SRAM_Table.txt ($(wc -l < src/output/SRC_SRAM_Table.txt) 行)"
# fi
# if [ -f src/output/DST_TCAM_Table.txt ]; then
#     echo "✓ DST_TCAM_Table.txt ($(wc -l < src/output/DST_TCAM_Table.txt) 行)"
# fi
# if [ -f src/output/DST_SRAM_Table.txt ]; then
#     echo "✓ DST_SRAM_Table.txt ($(wc -l < src/output/DST_SRAM_Table.txt) 行)"
# fi
echo ""
echo "=============================================================================="
echo "                               ✓ 所有步骤完成！"
echo "=============================================================================="