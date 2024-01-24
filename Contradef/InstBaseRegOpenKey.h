#pragma once
#ifndef INST_BASE_REG_OPEN_KEY_H
#define INST_BASE_REG_OPEN_KEY_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include "utils.h"
#include  "CallContextKey.h"

struct InstBaseRegOpenKeyArgs {
    ADDRINT hKey;
    ADDRINT lpSubKey;
    ADDRINT phkResult;
};

class InstBaseRegOpenKey : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    static std::map<CallContextKey, InstBaseRegOpenKeyArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult);
};

#endif // INST_BASE_REG_OPEN_KEY_H
