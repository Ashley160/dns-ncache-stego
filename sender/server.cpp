#include <iostream>
#include <fstream>
#include <bitset>
#include <string>
#include <unistd.h>
#include <cstring>
#include <unordered_map>
#include <cstdint>
#include "../common/extension_map.h"
using namespace std;

#ifdef DEBUG
#define DEBUG_COUT(x) do { x } while(0)
#else
#define DEBUG_COUT(x) do {} while(0)
#endif

bool embed_message(const string filename, char* nxdomain){
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

#ifdef DEBUG    // Show default type and length
    cout << "Default Type: " << bitset<8>(type[0]) << endl;
    cout << "Default Length: " << bitset<8>(length[0]) << " " 
                               << bitset<8>(length[1]) << " " 
                               << bitset<8>(length[2]) << endl;
#endif

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
	
    // Initialize output file
    ofstream out("s_domain.txt");

    // Step1: Insert type (8-bit) to output file
    bitset<8> b_type(type[0]);
    for (size_t i = 0; i < b_type.size(); i++) {
        DEBUG_COUT(cout << "b_type[" << i << "]: " << b_type[i] << endl;);
        if (b_type[i] == 1) {
            out << i << "." << nxdomain << endl;
        }
    }

    // Step2: Insert length (24-bit) to output file
    for (size_t i = 0; i < 3; i++) {
        bitset<8> b_length(length[i]);
        DEBUG_COUT(cout << "===================\n" 
             << "length[" << i << "]: " << length[i] << "  "
             << "b_length holds " << b_length << endl;);
        for (size_t j = 0; j < b_length.size(); j++) {
            DEBUG_COUT(cout << "b_length[" << 8+i*8+j << "]: " << b_length[j] << endl;);
            if (b_length[j] == 1) {
                out << 8 + i*8+j << "." << nxdomain << endl;
            }
        }
    }

    // Step3: Insert value to output file
    for (size_t i = 0; i < size; i++) {
        bitset<8> bits(value[i]);
        DEBUG_COUT(cout << "===================\n"
             << "value[" << i << "]: " << value[i] << "  "
             << "bits holds " << bits << endl;);
        for (size_t j = 0; j < bits.size(); j++) {
            DEBUG_COUT(cout << "bits[" << 32 + i*8+j << "]: " << bits[j] << endl;);
            if (bits[j] == 1) {
                out << 32 + i*8+j << "." << nxdomain << endl;
            }
        }
    }
       
    // Close output file and free memory
    out.close(); 
    delete[] value;
    return true;
}

int main(int argc, char** argv){
    // Check command line arguments
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <filename> <nxdomain> <dns_server>" << endl;
        return 1;
    }

    if(embed_message(argv[1], argv[2])){
        cout << "embed_message() succeeded.\n";
    }
    else{
        cerr << "embed_message() failed.\n";
        return 1;
    }

	// const string filename = argv[1];
	// char* nxdomain = argv[2];
	
}
