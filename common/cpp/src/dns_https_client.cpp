#include <curl/curl.h>
#include "dns_https_client.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <mutex>


// ==========================================================
// Constructor / Destructor
// ==========================================================
DnsHttpsClient::DnsHttpsClient(const std::string& server_url)
    : server_url_(server_url)
{
    static std::once_flag curl_init_flag;
    std::call_once(curl_init_flag, []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
}


// ===============================================
// libcurl write callback
// 把收到的資料 append 到 vector 的尾
// curl 收完資料呼叫 callback
// data 收到資料的起始位置; size = 1(libcurl 設計)
// nmemb 這次收到幾個 bytes; userp 是 user point
// ===============================================
static size_t curl_write_cb(void* data, size_t size, size_t nmemb, void* userp)
{
    auto* buf = static_cast<std::vector<uint8_t>*>(userp);
    size_t total = size * nmemb;
    auto* ptr = static_cast<std::uint8_t*>(data);
    buf->insert(buf->end(), ptr, ptr + total);
    return total;
}


// ================
// build dns query
// ================
std::vector<uint8_t> DnsHttpsClient::build_dns_query(const std::string& qname, uint16_t id)
{
    std::vector<uint8_t> msg;
    msg.reserve(12 + qname.size() + 2 + 4);     // 保留空間大小 Header 12,
                                                // QNAME： size+2 = 起始數量, 結尾 null
                                                // QTYPE/QCLASS 4

    // DNS Header (12 byetes)
    append_u16(msg, id);        // ID
    append_u16(msg, 0x0100);    // FLAGS: 0x0100 (RD=1)
    append_u16(msg, 0x0001);    // QDCOUNT
    append_u16(msg, 0x0000);    // ANCOUNT
    append_u16(msg, 0x0000);    // NSCOUNT
    append_u16(msg, 0x0000);    // ARCOUNT

    // QNAME: label format (RFC1035)
    size_t start = 0;
    while (true) {
        size_t dot = qname.find('.', start);
        size_t len = (dot == std::string::npos) ? (qname.size() - start) : (dot - start);
        if (len > 63) return {};

        msg.push_back(static_cast<uint8_t>(len));
        for(size_t i = 0; i < len; ++i)
            msg.push_back(static_cast<uint8_t>(qname[start + i]));

        if (dot == std::string::npos) break;
        start = dot + 1;
        if (start >= qname.size()) break;
    }
    msg.push_back(0x00);

    // QTYPE=A(1), QCLASS=IN(1)
    append_u16(msg, 0x0001);
    append_u16(msg, 0x0001);
    
    return msg;     // DoH 就不用再加 TCP prefix
}


// ==============================
// query raw
// 用 HTTPS POST 送出 DNS query
// 抓出 DNS Response wire format
// ==============================
bool DnsHttpsClient::query_raw(const std::string& qname, std::vector<uint8_t>& out_resp)
{
    // 用時間產生 query id
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    uint16_t id = (uint16_t)(now & 0xFFFF);
        
    std::vector<uint8_t> pkt = build_dns_query(qname, id);
    if (pkt.empty()) {
        std::cerr << "[DoH] build_dns_query failed for: " << qname << "\n";
        return false;
    }

    // curl_easy_init() 建立一個 curl handle
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[DoH] curl_easy_init() failed\n";
        return false;
    }

    // 確保每次拿到的 response 都是乾淨的結果
    out_resp.clear();

    // HTTP Header: RFC8484 (Content-Type 與 Accept 設為 application/dns-message)
    // curl_slist_append 準備 HTTPS headers
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/dns-message");
    headers = curl_slist_append(headers, "Accept: application/dns-message");

    // curl_easy_setopt() 設定 how to behave
    // handle, 設定甚麼，設成甚麼
    curl_easy_setopt(curl, CURLOPT_URL,             server_url_.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,      headers);
    curl_easy_setopt(curl, CURLOPT_POST,            1L);// 1L: long
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,      pkt.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,   static_cast<long>(pkt.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,   curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,       &out_resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,         30L);// 30L: 30 秒timeout

    // 因為目前我的 doh server 沒有 SSL 憑證
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

//    fprintf(stderr, "[DoH] sending %zu bytes\n", pkt.size());
//    for (size_t i = 0; i < pkt.size() && i < 32; i++)
//        fprintf(stderr, "%02X ", pkt[i]);
//    fprintf(stderr, "\n");

    // curl_easy_perform() 真正送出 request
    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    // curl_slist_free_all() 釋放 headers list
    // curl_easy_cleanup() 釋放 curl headle
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[DoH] curl error: " << curl_easy_strerror(res) << "\n";
        return false;
    }
    if (http_code != 200) {
        std::cerr << "[DoH] HTTP " << http_code << " from " << server_url_ << "\n";
        return false;
    }
    if (out_resp.size() < 12) {
        std::cerr << "[DoH] response too short (" << out_resp.size() << " bytes\n";
        return false;
    }
    
    return true;
}


// ====================================
// query_soa_ttl (same to DnsTcpClint)
// ====================================
bool DnsHttpsClient::query_soa_ttl(const std::string& qname, uint32_t& ttl_out) {
    std::vector<uint8_t> resp;
    if (!query_raw(qname, resp)) return false;
    return extract_soa_ttl_from_authority(resp, ttl_out);
}

// ========================================
// Parsing helper (same to DnsTcpClient)
// ========================================
void DnsHttpsClient::append_u16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back((uint8_t)((v >> 8) & 0xFF));
    out.push_back((uint8_t)(v & 0xFF));
}

bool DnsHttpsClient::read_u16(const std::vector<uint8_t>& b, size_t& off, uint16_t& out) {
    if (off + 2 > b.size()) return false;
    out = (uint16_t)((b[off] << 8) | b[off + 1]);
    off += 2;
    return true;
}

bool DnsHttpsClient::read_u32(const std::vector<uint8_t>& b, size_t& off, uint32_t& out) {
    if (off + 4 > b.size()) return false;
    out = ((uint32_t)b[off] << 24) | 
          ((uint32_t)b[off + 1] << 16) |
          ((uint32_t)b[off + 2] << 8)  |  
           (uint32_t)b[off + 3];
    off += 4;
    return true;
}

bool DnsHttpsClient::skip_name(const std::vector<uint8_t>& b, size_t& off) {
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
bool DnsHttpsClient::extract_soa_ttl_from_authority(const std::vector<uint8_t>& resp, uint32_t& ttl_out) {
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






