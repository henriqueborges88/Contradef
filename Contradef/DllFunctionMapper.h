#pragma once
#ifndef DLL_FUNCTION_MAPPER_H
#define DLL_FUNCTION_MAPPER_H

#include "Utils.h"
#include <vector>
#include <algorithm>

struct FunctionRange {
    WindowsAPI::DWORD64 start;
    WindowsAPI::DWORD64 end;
    std::string name;
};

std::string GetFunctionFromAddress(unsigned long long address, unsigned int pid);

#endif // DLL_FUNCTION_MAPPER_H