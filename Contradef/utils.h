#pragma once
#ifndef UTILS_H
#define UTILS_H

#include "pin.H"
#include <string>
#include <cctype>

std::wstring ConvertAddrToWideString(ADDRINT addr);
std::string ConvertAddrToAnsiString(ADDRINT addr);
std::wstring ReadWideString(ADDRINT addr);
std::string ExtractModuleName(const std::string& fullPath);
std::string toUpperCase(const std::string& str);

#endif // UTILS_H
