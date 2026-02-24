#include "dns_tcp_client.h"

#include <iostream>
#include <cstring>
#include <chrono>

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

DnsTcpClient::DnsTcpClient() : sockfd_(-1) {}

DnsTcpClient::~DnsTcpClient() {
    close();
}

static void dump_hex(const std::vector<uint8_t>& b, size_t n = 32) {
    size_t lim = (b.size() < n) ? b.size() : n;
    for (size_t i = 0; i < lim; i++) {
        printf("%02X%s", b[i], ((i + 1) % 16 == 0) ? "\n" : " ");
    }
    if (lim % 16 != 0) printf("\n");
}

// 反覆呼叫 send() 直到把 len bytes 全部傳完，中途遇到錯誤馬上回傳 false
bool DnsTcpClient::send_all(int fd, const uint8_t* buf, size_t len) {
    size_t offset = 0;
    while (offset < len) {
        ssize_t n = ::send(fd, buf + offset, len - offset, 0);      // n 表示實際送出的 bytes
        if (n <= 0) return false;       // n == -1 ERROR, n == 0 TCP EOF
        offset += (size_t)n;
    }
    return true;
}

// 反覆呼叫 recv() 直到把 len bytes 全部收完，中途遇到錯誤馬上回傳 false
bool DnsTcpClient::recv_all(int fd, uint8_t* buf, size_t len) {
    size_t offset = 0;
    while (offset < len) {
        ssize_t n = ::recv(fd, buf + offset, len - offset, 0);      // n 表示實際收到的 bytes
        if (n <= 0) return false;
        offset += (size_t)n;
    }
    return true;
}

// 目的:把 v 拆成兩個 bytes, 依序放進 out
// big-endian / network order
void DnsTcpClient::append_u16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back((uint8_t)((v >> 8) & 0xFF));
    out.push_back((uint8_t)(v & 0xFF));
}

// Create DNS/TCP packet
std::vector<uint8_t> DnsTcpClient::build_dns_query_tcp(const std::string& qname, uint16_t id = 0x1234) {
    std::vector<uint8_t> msg;
    msg.reserve(12 + qname.size() + 2 + 4);     // 保留空間大小 Header 12,
                                                // QNAME qname.size()-dot+label(dot+1)+1 = qname.size() + 2, 
                                                // QTYPE/QCLASS 4

    // DNS Header (12 byetes)
    append_u16(msg, id);        // ID
    append_u16(msg, 0x0100);    // FLAGS: 0x0100 (RD=1)
    append_u16(msg, 0x0001);    // QDCOUNT
    append_u16(msg, 0x0000);    // ANCOUNT
    append_u16(msg, 0x0000);    // NSCOUNT
    append_u16(msg, 0x0000);    // ARCOUNT

    // QNAME: label format
    size_t start = 0;
    while (true) {
        size_t dot = qname.find('.', start);
        size_t len = (dot == std::string::npos) ? (qname.size() - start) : (dot - start);
        if (len > 63) return {};    // invalid label length (RFC1035_2.3.4.)

        // push label length & label
        msg.push_back((uint8_t)len);
        for (size_t i = 0; i < len; i++) {
            msg.push_back((uint8_t)qname[start + i]);
        }

        if (dot == std::string::npos) break;
        start = dot + 1;
        if (start >= qname.size()) break; // trailing dot case
    }
    msg.push_back(0x00); // end of QNAME

    // QTYPE=A(1), QCLASS=IN(1)
    append_u16(msg, 0x0001);
    append_u16(msg, 0x0001);

    // TCP prefix: 2-byte length
    uint16_t mlen = (uint16_t)msg.size();   // DNS message 長度(不含 TCP prefix)
    std::vector<uint8_t> packet;                 
    packet.reserve(2 + msg.size());         // prefix length + msg length
    append_u16(packet, mlen);               // prefix len
    packet.insert(packet.end(), msg.begin(), msg.end());    // insert DNS message
    return packet;
}
    
// 把 sockfd_ 連到 DNS server:53
bool DnsTcpClient::connect_to(const std::string& server, const std::string& port) {
    close();    // 避免重複呼叫 connect_to 先把上一個關掉

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;    // SOCK_STREAM -> TCP

    addrinfo* res = nullptr;
    int rc = ::getaddrinfo(server.c_str(), port.c_str(), &hints, &res);     // &res: output (linked list of addrinfo)
    if (rc != 0) {
        std::cerr << "getaddrinfo failed: " << gai_strerror(rc) << std::endl;
        return false;
    }

    // socket fd, -1 表示尚未建立成功
    int fd = -1;
    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        // 失敗就繼續
        if (fd < 0) continue;

        // 成功回傳 0 後就指定 fockfd_ 設成 fd
        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            sockfd_ = fd;
            break;
        }

        // 失敗要把剛剛建立的 socket 關掉，並重設 fd = -1
        ::close(fd);
        fd = -1;
    }
    
    // 釋放不會再用到的 linked list
    ::freeaddrinfo(res);

    // 如果 sockfd_ 還是 -1 表示所有 server 都連不上
    if (sockfd_ < 0) {
        std::cerr << "connect failed to " << server << ":" << port << std::endl;
        return false;
    }

    return true;
}


void DnsTcpClient::close() {
    if (sockfd_ >= 0) {
        ::close(sockfd_);
        sockfd_ = -1;
    }
}


// 送一筆 DNS query
bool DnsTcpClient::query_raw(const std::string& qname, std::vector<uint8_t>& out_resp) {
    if (sockfd_ < 0) return false;

    // 用時間當 seed，產生簡單的 id（避免每次都一樣）
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    uint16_t id = (uint16_t)(now & 0xFFFF);

    // 建立 DNS/TCP packet
    std::vector<uint8_t> pkt = build_dns_query_tcp(qname, id);
    if (pkt.empty()) {
        std::cerr << "build_dns_query_tcp failed for: " << qname << std::endl;
        return false;
    }

    // Send DNS/TCP packet
    //std::cout << "Sending DNS/TCP query: " << qname << " (bytes=" << pkt.size() << ")\n";
    if (!send_all(sockfd_, pkt.data(), pkt.size())) {
        std::cerr << "send_all failed\n";
        return false;
    }

    // 必須把 response 讀出來，否則 TCP stream 會堆積卡住
    uint8_t lenbuf[2];
    if (!recv_all(sockfd_, lenbuf, 2)) {
        std::cerr << "recv length failed\n";
        return false;
    }
    // 把兩個 bytes 組回一個 2-bytes 整數 ([0]*256+[1])
    uint16_t rlen = (uint16_t)((lenbuf[0] << 8) | lenbuf[1]);
    std::vector<uint8_t> resp(rlen);
    out_resp.assign(rlen, 0);

    if (rlen == 0) {
        std::cerr << "recv length=0 (invalid DNS/TCP response)\n";
        return false;
    }
   
    if (!recv_all(sockfd_, out_resp.data(), out_resp.size())) {
        std::cerr << "recv body failed\n";
        return false;
    }
    
    //printf("RESP first bytes (len=%u):\n", (unsigned)out_resp.size());
    //dump_hex(out_resp, 32);

    return true;
}

// ---- Minimal DNS parsing for SOA TTL in Authority section ----
// 從 byte buffer b 的位置 off 讀出一個 16-bit unsigned int。
bool DnsTcpClient::read_u16(const std::vector<uint8_t>& b, size_t& off, uint16_t& out) {
    if (off + 2 > b.size()) return false;
    out = (uint16_t)((b[off] << 8) | b[off + 1]);
    off += 2;
    return true;
}

// 跟 read_u16 一樣概念，只是這次要讀 4 bytes（32-bit）
bool DnsTcpClient::read_u32(const std::vector<uint8_t>& b, size_t& off, uint32_t& out) {
    if (off + 4 > b.size()) return false;
    out = ((uint32_t)b[off] << 24) | ((uint32_t)b[off + 1] << 16) |
          ((uint32_t)b[off + 2] << 8)  |  (uint32_t)b[off + 3];
    off += 4;
    return true;
}

// Skip a DNS name (handles compression pointers)
bool DnsTcpClient::skip_name(const std::vector<uint8_t>& b, size_t& off) {
    if (off >= b.size()) return false;

    while (true) {
        if (off >= b.size()) return false;
        uint8_t len = b[off];

        // compression pointer: 11xxxxxx xxxxxxxx (RFC1035_4.1.4)
        if ((len & 0xC0) == 0xC0) {
            if (off + 2 > b.size()) return false;
            off += 2;
            return true;
        }

        // end of name
        if (len == 0x00) {
            off += 1;
            return true;
        }

        // normal label
        off += 1;
        if (off + len > b.size()) return false;
        off += len;
    }
}

// Return true and set ttl_out if finds SOA RR in Authority section
bool DnsTcpClient::extract_soa_ttl_from_authority(const std::vector<uint8_t>& resp, uint32_t& ttl_out) {
    if (resp.size() < 12) return false;     // DNS header 小於 12 bytes 代表資料不完整

    size_t off = 0;
    uint16_t id, flags, qd, an, ns, ar;
    if (!read_u16(resp, off, id)) return false;
    if (!read_u16(resp, off, flags)) return false;
    if (!read_u16(resp, off, qd)) return false;
    if (!read_u16(resp, off, an)) return false;
    if (!read_u16(resp, off, ns)) return false;
    if (!read_u16(resp, off, ar)) return false;

    // Skip questions
    for (uint16_t i = 0; i < qd; i++) {
        if (!skip_name(resp, off)) return false;
        uint16_t qtype, qclass;
        if (!read_u16(resp, off, qtype)) return false;
        if (!read_u16(resp, off, qclass)) return false;
    }
    
    /*
        目的：
        從目前 offset 開始，解析一筆 Resource Record (RR)
        讀出 TYPE / TTL，然後把整筆 RR 跳過

        因為只在這裡使用，所以用 lambda function 寫
    */
    auto skip_rr = [&](uint32_t& ttl, uint16_t& type, bool& is_soa) -> bool {
        if (!skip_name(resp, off)) return false;

        uint16_t rr_type, rr_class, rdlen;
        uint32_t rr_ttl;

        if (!read_u16(resp, off, rr_type)) return false;
        if (!read_u16(resp, off, rr_class)) return false;
        if (!read_u32(resp, off, rr_ttl)) return false;
        if (!read_u16(resp, off, rdlen)) return false;

        if (off + rdlen > resp.size()) return false;

        type = rr_type;
        ttl = rr_ttl;
        is_soa = (rr_type == 6); // SOA

        off += rdlen;
        return true;
    };

    // Skip answers
    for (uint16_t i = 0; i < an; i++) {
        uint32_t ttl; uint16_t type; bool is_soa;
        if (!skip_rr(ttl, type, is_soa)) return false;
    }

    // Authority: find SOA
    for (uint16_t i = 0; i < ns; i++) {
        uint32_t ttl; uint16_t type; bool is_soa;
        if (!skip_rr(ttl, type, is_soa)) return false;
        if (is_soa) {
            ttl_out = ttl;
            return true;
        }
    }

    return false;
}

bool DnsTcpClient::query_soa_ttl(const std::string& qname, uint32_t& ttl_out) {
    std::vector<uint8_t> resp;
    if (!query_raw(qname, resp)) return false;
    return extract_soa_ttl_from_authority(resp, ttl_out);
}


