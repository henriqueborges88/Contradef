#pragma once
#ifndef INST_ENUM_PROCESS_MODULES_H
#define INST_ENUM_PROCESS_MODULES_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContext.h"

struct InstEnumProcessModulesArgs {
    ADDRINT hProcess;
    ADDRINT lphModule;
    ADDRINT cb;
    ADDRINT lpcbNeeded;
};

class InstEnumProcessModules : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstEnumProcessModulesArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded);
};

#endif // INST_ENUM_PROCESS_MODULES_H
