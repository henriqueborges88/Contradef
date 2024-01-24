#pragma once
#ifndef INST_DEBUG_ACTIVE_PROCESS_H
#define INST_DEBUG_ACTIVE_PROCESS_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include "CallContextKey.h"
#include "utils.h"

struct InstDebugActiveProcessArgs {
    ADDRINT dwProcessId;
};

class InstDebugActiveProcess : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    static std::map<CallContextKey, InstDebugActiveProcessArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT dwProcessId);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT dwProcessId);
};

#endif // INST_DEBUG_ACTIVE_PROCESS_H
