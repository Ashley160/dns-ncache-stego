#pragma once

#include <string>
#include <vector>
#include <cstdint>

class DnsHttpsClient {
public:
    // server_url: DoH endpoint
    explicit DnsHttpsClient(const std::string& server_url = "https://163.22.22.81/dns-query");
    // 送出 A record query，把完整 DNS response wire format 存進 out_resp
    bool query_raw(const std::string& qname, std::vector<uint8_t>& out_resp);
    // 解析 response-authoritative 中 SOA TTL
    bool query_soa_ttl(const std::string& qname, uint32_t& ttl_out);

    
private:
    std::string server_url_;
    std::vector<uint8_t> build_dns_query(const std::string& qname, uint16_t id);

    // DNS builders/parsers
    static void append_u16(std::vector<uint8_t>& out, uint16_t v);
    static bool read_u16(const std::vector<uint8_t>& b, size_t& off, uint16_t& out);
    static bool read_u32(const std::vector<uint8_t>& b, size_t& off, uint32_t& out);
    static bool skip_name(const std::vector<uint8_t>& b, size_t& off);
    bool extract_soa_ttl_from_authority(const std::vector<uint8_t>& resp, uint32_t& ttl_out);
};
