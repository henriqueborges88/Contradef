#pragma once
#ifndef INST_REG_OPEN_KEY_A_H
#define INST_REG_OPEN_KEY_A_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"
#include "utils.h"
#include  "CallContextKey.h"

struct InstRegOpenKeyAArgs {
    ADDRINT hKey;
    ADDRINT lpSubKey;
    ADDRINT phkResult;
};

class InstRegOpenKeyA : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstRegOpenKeyAArgs> argsMap;
    static ADDRINT nextCallId;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult);
};

#endif // INST_REG_OPEN_KEY_A_H
