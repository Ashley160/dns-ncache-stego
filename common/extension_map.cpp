#include "extension_map.h"
#include <string>
#include <unordered_map>

using std::unordered_map;
using std::string;

unordered_map<string, int> ExtensionTypeMap = {
    {"txt", 1},
    {"png", 2},
    {"jpg", 3},
    {"mp3", 4},
    {"mp4", 5},
    {"out", 6},
    {"zip", 7}
};

string getExtension (const string& filename) {
    size_t dot = filename.find(".");
    if (dot == string::npos) return ""; // dot == "until the end of the string" return "";
    return filename.substr(dot + 1);
}

int getTypeValue (const string& filename) {
    string ext = getExtension(filename);
    return ExtensionTypeMap[ext];
}

string getTypeKey (const int value) {
    for (unordered_map<string, int>::iterator it = ExtensionTypeMap.begin(); it != ExtensionTypeMap.end(); it++) {
        if (it->second == value)
            return it->first;
    }
    return "unknow type";
}
