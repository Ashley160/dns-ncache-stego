#pragma once
#include <unordered_map>
#include <string>

using std::unordered_map;
using std::string;

extern unordered_map<string, int> ExtensionTypeMap;

string getExtension(const string& filename);
int getTypeValue(const string& filenmae);
string getTypeKey(const int value);
