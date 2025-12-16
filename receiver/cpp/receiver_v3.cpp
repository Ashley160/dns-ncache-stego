#include <iostream>
#include <cstdio>
#include <string>
#include <regex>
#include <bitset>
#include <fstream>
#include <chrono>
#include <vector>
#include <thread>
#include "extension_map.h"
using namespace std;

#ifdef DEBUG
#define DEBUG_COUT(x) do { x } while(0)
#else
#define DEBUG_COUT(x) do {} while(0)
#endif

#define CMD_BUF_SIZE 256
#define LINE_BUF_SIZE 512

string transmit_ttl(int index, string nxdomain, string dns_server, regex ttl_regex) {
    string full_domain = to_string(index) + "." + nxdomain;
    string cmd = "dig +https +tcp +noall +authority @" + dns_server + " " + full_domain;

    char ttl_buffer[64] = {0};
    string ttl;

    FILE * fp = popen(cmd.c_str(), "r");
    if (!fp) return ttl;

    while(fgets(ttl_buffer, sizeof(ttl_buffer), fp)) {
        DEBUG_COUT(cout << ttl_buffer << endl;);
        cmatch match;
        if (regex_search(ttl_buffer, match, ttl_regex)) {
            ttl = match[1].str();
            break;
        }
    }
    pclose(fp);
    return ttl;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <nxdomain> <dns_server>\n";
        return 1;
    }

    string nxdomain = argv[1], dns_server = argv[2];

    char ttl_buffer[64] = {0};
    string cmd, default_ttl, current_ttl;
    regex ttl_regex(R"(\s+(\d+)\s+IN\s+SOA\s+)");
    
    // --- start measure ---
    auto start = chrono::high_resolution_clock::now();

    // Get Negative TTL
    cmd = "dig +https +tcp +noall +authority @" + dns_server + " nonexistent." + nxdomain;
    FILE* ttl_fp = popen(cmd.c_str(), "r");
    if (!ttl_fp){
        cerr << "Failed to get Negative TTL\n";
        return 1;
    }
    while(fgets(ttl_buffer, sizeof(ttl_buffer), ttl_fp)) {
        DEBUG_COUT(cout << ttl_buffer << endl;);
        cmatch match;
        if (regex_search(ttl_buffer, match, ttl_regex)){
            DEBUG_COUT(cout << "Negative TTL (SOA) for nonexistene." << nxdomain << " is " << match[1] << " seconds\n";);
            default_ttl = match[1].str();
            break;
        }
    }
    pclose(ttl_fp);

    // Declare Thread 
    vector<thread> workers;
    workers.reserve(8);

    // Get Type (1-byte)
    bitset<8> b_type;
    string type_ttls[8];

    for (int i = 0; i < 8; i++) {
        workers.emplace_back([i, nxdomain, dns_server, ttl_regex, &type_ttls]{
            type_ttls[i] = transmit_ttl(i, nxdomain, dns_server, ttl_regex);
        });
    }

    for (auto &t : workers) if (t.joinable()) t.join();
    workers.clear();

    for (int i = 0; i < 8; i++) {
        const string &ttl = type_ttls[i];
        if (!ttl.empty() && ttl != default_ttl) {
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
    string length_ttls[24];

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 8; j++) {
            workers.emplace_back([i, j, nxdomain, dns_server, ttl_regex, &length_ttls]{
                int index = 8 + i*8 + j;
                length_ttls[j] = transmit_ttl(index, nxdomain, dns_server, ttl_regex);
            });
        }
        for (auto &t : workers) if (t.joinable()) t.join();
        workers.clear();

        for (int j = 0; j < 8; j++) {
            const string &ttl = length_ttls[j];
            if (!ttl.empty() && ttl != default_ttl) {
                b_length[(2-i)*8 + j] = 1;
                DEBUG_COUT(cout << "b_length[" << 8+i*8+j << "]: 1\n";);
            }
            else {
                b_length[(2-i)*8 + j] = 0;
                DEBUG_COUT(cout << "b_length[" << 8+i*8+j << "]: 0\n";);
            }
        }
    }

    DEBUG_COUT(cout << "length: " << b_length << "(" << b_length.to_ulong() << ") "
         << "\n===================\n";);


    // Get Value
    ofstream out("receiver_file." + ext, ios::binary);
    int size = b_length.to_ulong();

    for (int i = 0; i < size; i++) {
        bitset<8> bits;
        string ttls[8];

        for (int j = 0; j < 8; j++) {
            workers.emplace_back([i, j, nxdomain, dns_server, ttl_regex, &ttls]{
                int index = 32 + i*8 + j;
                ttls[j] = transmit_ttl(index, nxdomain, dns_server, ttl_regex);
            });
        }
        for (auto &t : workers) if (t.joinable()) t.join();
        workers.clear();

        for (int j = 0; j < 8; j++) {
            const string &ttl = ttls[j];
            if (!ttl.empty() && ttl != default_ttl) {
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

    // --- end measure ----
    auto end = chrono::high_resolution_clock::now();
    auto total_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    cout << "receiver runtime: " << total_time << " ms";

    return 0;
}
