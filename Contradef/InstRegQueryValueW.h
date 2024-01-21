#pragma once
#ifndef INST_REG_QUERY_VALUE_W_H
#define INST_REG_QUERY_VALUE_W_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContextKey.h"
#include "utils.h"

struct InstRegQueryValueWArgs {
    ADDRINT hKey;
    ADDRINT lpSubKey;
    ADDRINT lpValue;
    ADDRINT lpcbValue;
};

class InstRegQueryValueW : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstRegQueryValueWArgs> argsMap;
    static ADDRINT nextCallId;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT lpValue, ADDRINT lpcbValue);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT lpValue, ADDRINT lpcbValue);
};

#endif // INST_REG_QUERY_VALUE_W_H
