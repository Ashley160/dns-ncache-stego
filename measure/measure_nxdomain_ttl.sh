#!/bin/bash

# ============================================================
# measure_nxdomain_ttl.sh
# 量測各層級 NXDOMAIN SOA TTL
# Usage: ./measure_nxdomain_ttl.sh [resolver]
# ============================================================

RESOLVER="${1:-163.22.2.1}"
LABEL="2026invalid"
OUTPUT="nxdomain_ttl_results.tsv"

# 顏色
GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[1;33m"
NC="\033[0m"

# 欄位標題
echo -e "Domain\tLevel\tStatus\tTTL\tSOA_from" > "$OUTPUT"
echo -e "${YELLOW}Resolver: ${RESOLVER}${NC}"
echo -e "${YELLOW}Output:   ${OUTPUT}${NC}"
echo "------------------------------------------------------------"

# ============================================================
# 查詢函數
# $1 = 完整 domain name
# $2 = 層級描述（如 "1層 Root"）
# ============================================================
query() {
    local full="$1"
    local level="$2"

    local raw
    raw=$(dig "$full" A @"$RESOLVER" +time=5 +tries=2 2>/dev/null)

    local status ttl soa_ns

    status=$(echo "$raw" | grep -oP "status: \K[A-Z]+")
    ttl=$(echo "$raw" | grep -P "IN\s+SOA" | awk '{print $2}')
    soa_ns=$(echo "$raw" | grep -P "IN\s+SOA" | awk '{print $1}' | sed 's/\.$//')

    if [[ -z "$ttl" ]]; then
        ttl="N/A"
        soa_ns="N/A"
    fi

    if [[ "$status" == "NXDOMAIN" ]]; then
        echo -e "${GREEN}[NXDOMAIN]${NC} [${level}] ${full}"
        echo -e "           TTL=${ttl}  SOA_from=${soa_ns}"
    elif [[ "$status" == "SERVFAIL" ]]; then
        echo -e "${RED}[SERVFAIL]${NC} [${level}] ${full} — 無 SOA"
    else
        echo -e "${YELLOW}[${status:-TIMEOUT}]${NC} [${level}] ${full}"
    fi

    echo -e "${full}\t${level}\t${status:-TIMEOUT}\t${ttl}\t${soa_ns}" >> "$OUTPUT"

    sleep 0.5
}

# ============================================================
# 層級 1：Root
# ============================================================
echo -e "\n${YELLOW}=== 層級 1: Root ===${NC}"
query "invalid."                          "1層 Root"

# ============================================================
# 層級 2：TLD
# ============================================================
echo -e "\n${YELLOW}=== 層級 2: TLD ===${NC}"
query "${LABEL}.org."                     "2層 TLD"
query "${LABEL}.edu."                     "2層 TLD"
query "${LABEL}.com."                     "2層 TLD"
query "${LABEL}.tw."                      "2層 TLD"

# ============================================================
# 層級 3：SLD
# ============================================================
echo -e "\n${YELLOW}=== 層級 3: SLD ===${NC}"
query "${LABEL}.mit.edu."                 "3層 SLD"
query "${LABEL}.harvard.edu."             "3層 SLD"
query "${LABEL}.edu.tw."                  "3層 SLD"

# ============================================================
# 層級 4：Sub-SLD
# ============================================================
echo -e "\n${YELLOW}=== 層級 4: Sub-SLD ===${NC}"
query "${LABEL}.ncnu.edu.tw."        "4層 Sub-SLD"
query "${LABEL}.nthu.edu.tw."          "4層 Sub-SLD"
query "${LABEL}.ntu.edu.tw."          "4層 Sub-SLD"

# ============================================================
# 結果摘要
# ============================================================
echo ""
echo "============================================================"
echo "結果已儲存至 ${OUTPUT}"
echo ""
echo "NXDOMAIN 統計："
grep "NXDOMAIN" "$OUTPUT" | awk -F'\t' '{printf "  %-45s  層級=%-12s  TTL=%-8s  SOA=%s\n", $1, $2, $4, $5}'
echo ""
echo "SERVFAIL / 無法取得 SOA："
grep -E "SERVFAIL|TIMEOUT" "$OUTPUT" | awk -F'\t' '{print "  " $1 " [" $2 "]"}'
echo "============================================================"
