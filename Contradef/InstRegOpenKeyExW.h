#pragma once
#ifndef INST_REG_OPEN_KEY_EX_W_H
#define INST_REG_OPEN_KEY_EX_W_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"
#include "utils.h"
#include  "CallContextKey.h"

struct RegOpenKeyExWArgs {
    ADDRINT hKey;
    ADDRINT lpSubKey;
    ADDRINT ulOptions;
    ADDRINT samDesired;
    ADDRINT phkResult;
};

class InstRegOpenKeyExW : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, RegOpenKeyExWArgs> argsMap;
    static ADDRINT nextCallId;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult);
};

#endif // INST_REG_OPEN_KEY_EX_W_H
