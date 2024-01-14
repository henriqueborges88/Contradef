#pragma once
#ifndef INST_CHECK_REMOTE_DEBUGGER_PRESENT_H
#define INST_CHECK_REMOTE_DEBUGGER_PRESENT_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"

struct InstCheckRemoteDebuggerPresentArgs {
    ADDRINT hProcess;
    ADDRINT pbDebuggerPresent;
};

class InstCheckRemoteDebuggerPresent : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstCheckRemoteDebuggerPresentArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT hProcess, ADDRINT pbDebuggerPresent);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT pbDebuggerPresent);
};

#endif // INST_CHECK_REMOTE_DEBUGGER_PRESENT_H
