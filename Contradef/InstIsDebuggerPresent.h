#pragma once
#ifndef INST_IS_DEBUGGER_PRESENT_H
#define INST_IS_DEBUGGER_PRESENT_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContextKey.h"

class InstIsDebuggerPresent : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr);
};

#endif // INST_IS_DEBUGGER_PRESENT_H
