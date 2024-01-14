#pragma once

#ifndef INST_REG_QUERY_VALUE_EX_W_H
#define INST_REG_QUERY_VALUE_EX_W_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include "InstrumentationStrategy.h"

typedef UINT64 HWND;
typedef const wchar_t* LPCWSTR;
typedef unsigned int UINT;

struct RegQueryValueExWArgs {
    ADDRINT hKey;
    std::wstring lpValueName;
    ADDRINT lpReserved;
    ADDRINT lpType;
    ADDRINT lpData;
    ADDRINT lpcbData;
};

class InstRegQueryValueExW : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);
private:
    static std::map<ADDRINT, RegQueryValueExWArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData);
};

#endif // INST_REG_QUERY_VALUE_EX_W_H
