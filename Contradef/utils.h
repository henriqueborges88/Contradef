#pragma once
#ifndef UTILS_H
#define UTILS_H

#include "pin.H"

namespace WindowsAPI
{
#include <Windows.h>
#include <unistd.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <Winternl.h>
}

#include <iostream>
#include <fstream>
#include <clocale>
#include "CallContext.h"
#include <iostream>
#include <string>
#include <cctype>
#include <deque>
#include "NtStructures.h"

bool IsPointer(ADDRINT value);
std::string GetNumericValue(ADDRINT ea, UINT32 size);
std::string GetNumericHexValue(ADDRINT ea, UINT32 size);
std::string GetNumericValueFromRef(VOID* ea, UINT32 size);
bool IsStringPointer(UINT64 addr);
std::string GetStringValueFromRegister(UINT64 value, UINT32 size);
std::wstring ConvertAddrToWideString(ADDRINT addr);
std::string ConvertAddrToAnsiString(ADDRINT addr);
std::string ConvertWideStringToString(const std::wstring& wstr);
void LogWString(const std::wstring& wstr);
std::string PrintNormalized(const std::wstring& wstr);
std::string WStringToString(const std::wstring& wstr);
std::string CopyLPCSTR(ADDRINT addr);
std::string ExtractModuleName(const std::string& fullPath);
std::string toUpperCase(const std::string& str);
VOID PrintTrace(std::deque<std::string>& rtnTrace);
void PrintCallContextMap(std::map<CallContextKey, CallContext*>& callContextMap);
const wchar_t* ConvertToWideString(const std::string& str);
std::string ConvertWStringToUTF8(const std::wstring& wstr);
std::string WideStringToStringFromPointer(ADDRINT addr);
void GetParentProcessName();
DWORD GetProcessIdByName(std::string processName);
std::string getFileName(const std::string& filePath);
#endif // UTILS_H
