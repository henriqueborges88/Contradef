#pragma once
#ifndef INST_REG_QUERY_VALUE_A_H
#define INST_REG_QUERY_VALUE_A_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContextKey.h"
#include "utils.h"

struct InstRegQueryValueAArgs {
    ADDRINT hKey;
    ADDRINT lpSubKey;
    ADDRINT lpValue;
    ADDRINT lpcbValue;
};

class InstRegQueryValueA : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    static std::map<CallContextKey, InstRegQueryValueAArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT lpValue, ADDRINT lpcbValue);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT lpValue, ADDRINT lpcbValue);
};

#endif // INST_REG_QUERY_VALUE_A_H
