#include <iostream>
#include <cstdio>
#include <string>
#include <regex>
#include <bitset>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>

#include "extension_map.h"
#include "dns_tcp_client.h"
using namespace std;

// Global accumulators for timing (microseconds)
atomic<long long> g_dns_time_us{0};

void receive_byte(
    int             byte_index,
    int             byte_offset,
    const string&   nxdomain,
    const string&   dns_server,
    uint32_t        default_ttl,
    vector<char>&   result)
{
    DnsTcpClient client;
    if (!client.connect_to(dns_server, "53")) {
        cerr << "connect failed (byte " << byte_index << ")\n";
        result[byte_index] = 0;
        return;
    }

    bitset<8> bits;
    for (int i = 0; i < 8; ++i) {
    #ifdef DEBUG
        cout << "bits[" << byte_offset+i << "]: " << bits[i] << "\n";
    #endif
        uint32_t current_ttl = 0;
        string full_domain = to_string(byte_offset+i) + "." + nxdomain;

        // DNS tranmission time
        auto dns_start = chrono::high_resolution_clock::now();
        if (!client.query_soa_ttl(full_domain, current_ttl)) {
            cerr << "Failed to get SOA TTL for " << full_domain << "\n";
            return;
        }
        auto dns_end = chrono::high_resolution_clock::now();
        g_dns_time_us += chrono::duration_cast<chrono::microseconds>(dns_end - dns_start).count();
        bits[i] = (current_ttl != default_ttl) ? 1 : 0;
    }
#ifdef DEBUG
    cout << "value[" << byte_index << "]: " << bits << "\n=============\n";
#endif
    result[byte_index] = static_cast<char>(bits.to_ulong());
    client.close();
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <nxdomain> <dns_server> <num_threads>\n";
        return 1;
    }
    string nxdomain = argv[1], dns_server = argv[2];
    int num_threads = atoi(argv[3]);
    
    // total timing start
    auto total_start = chrono::high_resolution_clock::now();
    
    // Negativity Detection timing start
    auto neg_start = chrono::high_resolution_clock::now();

    // Connect once
    DnsTcpClient client;
    if (!client.connect_to(dns_server, "53")) {
        cerr << "Failed to connec to DNS Server\n";
        return 1;
    }

    // Get default Negative TTL
    uint32_t default_ttl = 0;
    string base_domain = "nonexistent." + nxdomain;
    auto dns_default_ttl_start = chrono::high_resolution_clock::now();
    if (!client.query_soa_ttl(base_domain, default_ttl)) {
        cerr << "Failed to get Negative TTL (SOA) for " << base_domain << "\n";
        return 1;
    }
    auto dns_default_ttl_end = chrono::high_resolution_clock::now();
    g_dns_time_us += chrono::duration_cast<chrono::microseconds>(dns_default_ttl_end - dns_default_ttl_start).count();
#ifdef DEBUG 
    cout << "Default Negative TTL (SOA) for " << base_domain
        << " is " << default_ttl << " seconds\n";
#endif

    // Get Type (1-byte)
    bitset<8> b_type;
    for (size_t i=0; i<8; i++) {
        uint32_t current_ttl = 0;
        string full_domain = to_string(i) + "." + nxdomain;
        auto dns_type_start = chrono::high_resolution_clock::now();
        if (!client.query_soa_ttl(full_domain, current_ttl)) {
            cerr << "Failed to get Negative TTL (SOA) for " << full_domain << "\n";
            return 1;
        }
        auto dns_type_end = chrono::high_resolution_clock::now();
        g_dns_time_us += chrono::duration_cast<chrono::microseconds>(dns_type_end - dns_type_start).count();
        b_type[i] = (current_ttl != default_ttl) ? 1 : 0;
    }
    string ext = getTypeKey(b_type.to_ulong());
    client.close();
#ifdef DEBUG
    cout << "type: " << b_type << "(" << b_type.to_ulong() << ") "
        << ext << "\n===============\n";
#endif
            

    // Get Length (3-byte)
    vector<char> length_result(3, 0);
    vector<thread> length_workers;
    length_workers.reserve(3);
    for (int i = 0; i < 3; ++i) {
        length_workers.emplace_back(receive_byte, i, 8 + i * 8,
                            cref(nxdomain), cref(dns_server),
                            default_ttl, ref(length_result));
    }
    for (auto& t: length_workers) t.join();
    length_workers.clear();

    bitset<24> b_length;
    for (int i = 0; i < 3; ++i) {
        bitset<8> b(length_result[i]);
        for(int j = 0; j < 8; ++j)
            b_length[(2-i)*8+j] = b[j];
    }
    int size = b_length.to_ulong();
#ifdef DEBUG
    cout << "length: " << b_length << "(" << b_length.to_ulong() << ") "
         << "\n===================\n";
#endif

    // Get Value
    vector<char> result(size, 0);
    vector<thread> workers;
    workers.reserve(num_threads);

    for (int i = 0; i < size; ++i) {
        int bit_offset = 32 + i * 8;
        workers.emplace_back(receive_byte, i, bit_offset,
                            cref(nxdomain), cref(dns_server),
                            default_ttl, ref(result));

        if ((int)workers.size() == num_threads || i == size - 1) {
            for (auto& t: workers) t.join();
            workers.clear();
        }
    }
    // Negativity Detection timer end
    auto neg_end = chrono::high_resolution_clock::now();
    auto neg_time = chrono::duration_cast<chrono::milliseconds>(neg_end - neg_start).count();

    // Decapsulation timer start
    auto decap_start = chrono::high_resolution_clock::now();

    // Write result ot file
    ofstream out("receiver_file." + ext, ios::binary);
    for (int i = 0; i < size; ++i)
        out.write(&result[i], 1);

    // Decapsulation timer end
    auto decap_end = chrono::high_resolution_clock::now();
    auto decap_time = chrono::duration_cast<chrono::milliseconds>(decap_end - decap_start).count();

    // Total timer end
    auto total_end = chrono::high_resolution_clock::now();
    auto total_time = chrono::duration_cast<chrono::milliseconds>(total_end - total_start).count();

    cout << "[Timing Breakdown]\n";
    cout << "  Negativity Detection time : " << neg_time  << " ms\n";
    cout << "    └─ DNS query time       : " << (g_dns_time_us.load() / 1000.0) 
            << " ms (accumulated across threads)\n";
    cout << "  Decapsulation time        : " << decap_time << " ms\n";
    cout << "  Receiver total time       : " << total_time << " ms\n";


    return 0;
}
