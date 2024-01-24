#pragma once
#ifndef INST_REG_OPEN_KEY_W_H
#define INST_REG_OPEN_KEY_W_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include "utils.h"
#include  "CallContextKey.h"

struct InstRegOpenKeyWArgs {
    ADDRINT hKey;
    ADDRINT lpSubKey;
    ADDRINT phkResult;
};

class InstRegOpenKeyW : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    static std::map<CallContextKey, InstRegOpenKeyWArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult);
};

#endif // INST_REG_OPEN_KEY_W_H
