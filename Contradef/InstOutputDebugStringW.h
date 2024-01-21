#pragma once
#ifndef INST_OUTPUT_DEBUG_STRING_W_H
#define INST_OUTPUT_DEBUG_STRING_W_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"
#include "utils.h"
#include  "CallContextKey.h"

struct InstOutputDebugStringWArgs {
    ADDRINT lpOutputString;
};

class InstOutputDebugStringW : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstOutputDebugStringWArgs> argsMap;
    static ADDRINT nextCallId;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpOutputString);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpOutputString);
};

#endif // INST_OUTPUT_DEBUG_STRING_W_H
