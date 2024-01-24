#pragma once
#ifndef INST_REG_QUERY_VALUE_EX_A_H
#define INST_REG_QUERY_VALUE_EX_A_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContextKey.h"
#include "utils.h"

struct InstRegQueryValueExAArgs {
    ADDRINT hKey;
    ADDRINT lpValueName;
    ADDRINT lpReserved;
    ADDRINT lpType;
    ADDRINT lpData;
    ADDRINT lpcbData;
};

class InstRegQueryValueExA : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    static std::map<CallContextKey, InstRegQueryValueExAArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData);
};

#endif // INST_REG_QUERY_VALUE_EX_A_H
