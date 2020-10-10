#ifndef __UTIL__
#define __UTIL__
#include<string>
#include<vector>
std::string& Strim(std::string &s, const std::string & del);
std::vector<std::string> Split(const std::string & input, const std::string& regex);

#endif //__UTIL__

