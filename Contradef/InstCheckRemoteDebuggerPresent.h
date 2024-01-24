#pragma once
#ifndef INST_CHECK_REMOTE_DEBUGGER_PRESENT_H
#define INST_CHECK_REMOTE_DEBUGGER_PRESENT_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContextKey.h"

struct InstCheckRemoteDebuggerPresentArgs {
    ADDRINT hProcess;
    ADDRINT pbDebuggerPresent;
};

class InstCheckRemoteDebuggerPresent : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    static std::map<CallContextKey, InstCheckRemoteDebuggerPresentArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT pbDebuggerPresent);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT pbDebuggerPresent);
};

#endif // INST_CHECK_REMOTE_DEBUGGER_PRESENT_H