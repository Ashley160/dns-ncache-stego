#include <iostream>
#include <cstdio>
#include <string>
#include <regex>
#include <bitset>
#include <fstream>
#include <chrono>

#include "extension_map.h"
#include "dns_tcp_client.h"
using namespace std;

#ifdef DEBUG
#define DEBUG_COUT(x) do { x } while(0)
#else
#define DEBUG_COUT(x) do {} while(0)
#endif

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <nxdomain> <dns_server>\n";
        return 1;
    }

    string nxdomain = argv[1], dns_server = argv[2];
    
    // --- start measure ---
    auto start = chrono::high_resolution_clock::now();

    // Connect once
    DnsTcpClient client;
    if (!client.connect_to(dns_server, "53")) {
        cerr << "Failed to connec to DNS Server\n";
        return 1;
    }

    // Get default Negative TTL
    uint32_t default_ttl = 0;
    string base_domain = "nonexistent." + nxdomain;
    if (!client.query_soa_ttl(base_domain, default_ttl)) {
        cerr << "Failed to get Negative TTL (SOA) for " << base_domain << "\n";
        return 1;
    }
    DEBUG_COUT(cout << "Default Negative TTL (SOA) for " << base_domain
                        << " is " << default_ttl << " seconds\n";);

    // Get Type (1-byte)
    bitset<8> b_type;
    for (size_t i=0; i<8; i++) {
        uint32_t current_ttl = 0;
        string full_domain = to_string(i) + "." + nxdomain;
        if (!client.query_soa_ttl(full_domain, current_ttl)) {
            cerr << "Failed to get Negative TTL (SOA) for " << full_domain << "\n";
            return 1;
        }

        if (current_ttl != default_ttl) {
            b_type[i] = 1;
            DEBUG_COUT(cout << "b_type[" << i << "]: 1\n";);
        }
        else {
            b_type[i] = 0;
            DEBUG_COUT(cout << "b_type[" << i << "]: 0\n";);
        }
    }
    string ext = getTypeKey(b_type.to_ulong());
    DEBUG_COUT(cout << "type: " << b_type << "(" << b_type.to_ulong() << ") "
         << ext << "\n===================\n";);
            

    // Get Length (3-byte)
    bitset<24> b_length;
    for (int i=0; i<3; i++) {
        for (int j=0; j<8; j++) {
            uint32_t current_ttl = 0;
            string full_domain = to_string(8+i*8+j) + "." + nxdomain;
            if (!client.query_soa_ttl(full_domain, current_ttl)) {
                cerr << "Failed to get Negative TTL (SOA) for " << full_domain << "\n";
                return 1;
            }
            if (current_ttl != default_ttl) {
                b_length[(2-i)*8+j] = 1;
                DEBUG_COUT(cout << "b_length[" << 8+i*8+j << "]: 1\n";);
            }
            else {
                b_length[(2-i)*8+j] = 0;
                DEBUG_COUT(cout << "b_length[" << 8+i*8+j << "]: 0\n";);
            }
        }
    }
    DEBUG_COUT(cout << "length: " << b_length << "(" << b_length.to_ulong() << ") "
         << "\n===================\n";);

    // Get Value
    ofstream out("receiver_file." + ext, ios::binary);
    int size = b_length.to_ulong();

    for (int i=0; i<size; i++) {
        bitset<8> bits;
        for (int j=0; j<8; j++) {
            uint32_t current_ttl = 0;
            string full_domain = to_string(32+i*8+j) + "." + nxdomain;
            if (!client.query_soa_ttl(full_domain, current_ttl)) {
                cerr << "Failed to get Negative TTL (SOA) for " << full_domain << "\n";
                return 1;
            }

            if (current_ttl != default_ttl) {
                bits[j] = 1;
                DEBUG_COUT(cout << "bits[" << 32+i*8+j << "]: 1\n";);
            }
            else {
                bits[j] = 0;
                DEBUG_COUT(cout << "bits[" << 32+i*8+j << "]: 0\n";);
            }
        }
        DEBUG_COUT(cout << "value[" << i << "]: " << bits << "\n===================\n";);
        char byte = static_cast<char>(bits.to_ulong());
        out.write(&byte, 1);
    }

    client.close();

    // --- end measure ----
    auto end = chrono::high_resolution_clock::now();
    auto total_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    cout << "receiver runtime: " << total_time << " ms";

    return 0;
}
