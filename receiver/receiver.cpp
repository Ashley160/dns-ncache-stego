#include <iostream>
#include <cstdio>
#include <string>
#include <regex>
#include <bitset>
#include <fstream>
#include "../common/extension_map.h"
using namespace std;

#ifdef DEBUG
#define DEBUG_COUT(x) do { x } while(0)
#else
#define DEBUG_COUT(x) do {} while(0)
#endif

#define CMD_BUF_SIZE 256
#define LINE_BUF_SIZE 512

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << "<nxdomain> <dns_server>\n";
        return 1;
    }

    string nxdomain = argv[1], dns_server = argv[2];

    char ttl_buffer[64] = {0};
    string cmd, default_ttl, current_ttl;
    regex ttl_regex(R"(\s+(\d+)\s+IN\s+SOA\s+)");
    
    // Get Negative TTL
    cmd = "dig +tcp +noall +authority @" + dns_server + " nonexistent." + nxdomain;
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

    // Get Type (1-byte)
    bitset<8> b_type;
    for (size_t i=0; i<8; i++) {
        string full_domain = to_string(i) + "." + nxdomain;
        cmd = "dig +tcp +noall +authority @" + dns_server + " " + full_domain;
        FILE* fp = popen(cmd.c_str(), "r");
        if (!fp) { cerr << "Faild to dig \n"; return 1; }
        while (fgets(ttl_buffer, sizeof(ttl_buffer), fp)) {
            DEBUG_COUT(cout << ttl_buffer << endl;);
            cmatch match;
            if (regex_search(ttl_buffer, match, ttl_regex)) {
                DEBUG_COUT( cout << "Negative TTL for " << full_domain << " is " << match[1] << " seconds\n";);
                current_ttl = match[1].str();
                break;
            }
        }
        pclose(fp);
        if (current_ttl != default_ttl) {
            b_type[i] = 1;
            //cout << "b_type[" << i << "]: 1\n";
        }
        else {
            b_type[i] = 0;
            //cout << "b_type[" << i << "]: 0\n";
        }
    }
    string ext = getTypeKey(b_type.to_ulong());
    cout << "type: " << b_type << "(" << b_type.to_ulong() << ") "
         << ext << "\n===================\n";
            

    // Get Length (3-byte)
    bitset<24> b_length;
    for (int i=0; i<3; i++) {
        for (int j=0; j<8; j++) {
            string full_domain = to_string(8+i*8+j) + "." + nxdomain;
            cmd = "dig +tcp +noall +authority @" + dns_server + " " + full_domain;
            FILE* fp = popen(cmd.c_str(), "r");
            if (!fp) { cerr << "Faild to dig \n"; return 1; }
//            bool timeout = false;
            while(fgets(ttl_buffer, sizeof(ttl_buffer), fp)) {
                DEBUG_COUT(cout << ttl_buffer << endl;);
                
                /* Test for timeout */
//                string line(ttl_buffer);
//                if (line.find("timed out") != string::npos) {
//                    timeout = true;
//                    break;
//                }
                
                /* Catch Negative TTL */
                cmatch match;
                if (regex_search(ttl_buffer, match, ttl_regex)) {
                    DEBUG_COUT( cout << "Negative TTL for " << full_domain << " is " << match[1] << " seconds\n";);
                    current_ttl = match[1].str();
                    break;
                }
            }
            pclose(fp);
            if (current_ttl != default_ttl) {
                b_length[(2-i)*8+j] = 1;
                //cout << "b_length[" << 8+i*8+j << "]: 1\n";
            }
            else {
                b_length[(2-i)*8+j] = 0;
                //cout << "b_length[" << 8+i*8+j << "]: 0\n";
            }
        }
    }
    cout << "length: " << b_length << "(" << b_length.to_ulong() << ") "
         << "\n===================\n";

    // Get Value
    ofstream out("receiver_file." + ext, ios::binary);
    int size = b_length.to_ulong();

    for (int i=0; i<size; i++) {
        bitset<8> bits;
        for (int j=0; j<8; j++) {
            current_ttl.clear();
            string full_domain = to_string(32+i*8+j) + "." + nxdomain;
            cmd = "dig +tcp +noall +authority @" + dns_server + " " + full_domain;
            FILE* fp = popen(cmd.c_str(), "r");
            if (!fp) { cerr << "Faild to dig \n"; return 1; }
//            bool timeout = false;
            while(fgets(ttl_buffer, sizeof(ttl_buffer), fp)) {
                DEBUG_COUT(cout << ttl_buffer << endl;);
                
                /* Test for timeout */
//                string line(ttl_buffer);
//                if (line.find("timed out") != string::npos) {
//                    timeout = true;
//                    break;
//                }
                
                cmatch match;
                if (regex_search(ttl_buffer, match, ttl_regex)) {
                    DEBUG_COUT( cout << "Negative TTL for " << full_domain << " is " << match[1] << " seconds\n";);
                    current_ttl = match[1].str();
                    break;
                }
            }
            pclose(fp);
            if (current_ttl != default_ttl) {
                bits[j] = 1;
                //cout << "bits[" << 32+i*8+j << "]: 1\n";
            }
            else {
                bits[j] = 0;
                //cout << "bits[" << 32+i*8+j << "]: 0\n";
            }
        }
        cout << "value[" << i << "]: " << bits << "\n===================\n";
        char byte = static_cast<char>(bits.to_ulong());
        out.write(&byte, 1);
    }

    return 0;
}
