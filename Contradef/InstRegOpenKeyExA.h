#pragma once
#ifndef INST_REG_OPEN_KEY_EX_A_H
#define INST_REG_OPEN_KEY_EX_A_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include "utils.h"
#include  "CallContextKey.h"

struct InstRegOpenKeyExAArgs {
    ADDRINT hKey;
    ADDRINT lpSubKey;
    ADDRINT ulOptions;
    ADDRINT samDesired;
    ADDRINT phkResult;
};

class InstRegOpenKeyExA : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    static std::map<CallContextKey, InstRegOpenKeyExAArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult);
};

#endif // INST_REG_OPEN_KEY_EX_A_H