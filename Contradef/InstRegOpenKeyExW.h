#pragma once
#pragma once

#ifndef INST_REG_OPEN_KEY_EX_W_H
#define INST_REG_OPEN_KEY_EX_W_H

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

struct RegOpenKeyExWArgs {
    ADDRINT hKey;
    std::wstring lpSubKey;
    ADDRINT ulOptions;
    ADDRINT samDesired;
    ADDRINT phkResult;
};

class InstRegOpenKeyExW : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, RegOpenKeyExWArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult);
};

#endif // INST_REG_OPEN_KEY_EX_W_H
