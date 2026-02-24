#pragma once
#include <string>
#include <vector>
#include <cstdint>

class DnsTcpClient {
public:
    DnsTcpClient();
    ~DnsTcpClient();

    DnsTcpClient(const DnsTcpClient&) = delete;
    DnsTcpClient& operator=(const DnsTcpClient&) = delete;

    bool connect_to(const std::string& server, const std::string& port = "53");
    void close();

    // Send a DNS query over TCP and return raw DNS response (WITHOUT TCP length prefix)
    bool query_raw(const std::string& qname, std::vector<uint8_t>& out_resp);

    // Convenience: query and extract SOA RR TTL in Authority section (for NXDOMAIN negative caching TTL behavior)
    bool query_soa_ttl(const std::string& qname, uint32_t& ttl_out);

private:
    int sockfd_;

    // I/O helpers
    static bool send_all(int fd, const uint8_t* buf, size_t len);
    static bool recv_all(int fd, uint8_t* buf, size_t len);

    // DNS builders/parsers
    static void append_u16(std::vector<uint8_t>& out, uint16_t v);
    static std::vector<uint8_t> build_dns_query_tcp(const std::string& qname, uint16_t id);

    static bool read_u16(const std::vector<uint8_t>& b, size_t& off, uint16_t& out);
    static bool read_u32(const std::vector<uint8_t>& b, size_t& off, uint32_t& out);
    static bool skip_name(const std::vector<uint8_t>& b, size_t& off);
    static bool extract_soa_ttl_from_authority(const std::vector<uint8_t>& resp, uint32_t& ttl_out);
};
