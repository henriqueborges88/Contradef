#pragma once
#ifndef UTILS_H
#define UTILS_H

#include "pin.H"

namespace WindowsAPI
{
#include <Windows.h>
#include <unistd.h>
}

#include <iostream>
#include <fstream>
#include <clocale>
#include "CallContext.h"
#include <iostream>
#include <string>
#include <cctype>
#include <deque>

std::wstring ConvertAddrToWideString(ADDRINT addr);
std::string ConvertAddrToAnsiString(ADDRINT addr);
std::string ConvertWideStringToString(const std::wstring& wstr);
void LogWString(const std::wstring& wstr);
std::string PrintNormalized(const std::wstring& wstr);
std::string WStringToString(const std::wstring& wstr);
std::string ExtractModuleName(const std::string& fullPath);
std::string toUpperCase(const std::string& str);
VOID PrintTrace(std::deque<std::string>& rtnTrace);
void PrintCallContextMap(std::map<CallContextKey, CallContext*>& callContextMap);
const wchar_t* ConvertToWideString(const std::string& str);
std::string ConvertWStringToUTF8(const std::wstring& wstr);
std::string WideStringFromPointer(ADDRINT addr);
#endif // UTILS_H
