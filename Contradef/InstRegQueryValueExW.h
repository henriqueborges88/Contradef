#pragma once
#ifndef INST_REG_QUERY_VALUE_EX_W_H
#define INST_REG_QUERY_VALUE_EX_W_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"
#include "utils.h"
#include  "CallContextKey.h"

typedef UINT64 HWND;
typedef const wchar_t* LPCWSTR;
typedef unsigned int UINT;

struct RegQueryValueExWArgs {
    ADDRINT hKey;
    ADDRINT lpValueName;
    ADDRINT lpReserved;
    ADDRINT lpType;
    ADDRINT lpData;
    ADDRINT lpcbData;
};

class InstRegQueryValueExW : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, RegQueryValueExWArgs> argsMap;
    static ADDRINT nextCallId;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData);
};

#endif // INST_REG_QUERY_VALUE_EX_W_H
