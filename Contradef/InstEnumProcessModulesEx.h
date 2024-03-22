#pragma once
#ifndef INST_ENUM_PROCESS_MODULES_EX_H
#define INST_ENUM_PROCESS_MODULES_EX_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContext.h"

struct InstEnumProcessModulesExArgs {
    ADDRINT hProcess;
    ADDRINT lphModule;
    ADDRINT cb;
    ADDRINT lpcbNeeded;
    ADDRINT dwFilterFlag;
};

class InstEnumProcessModulesEx : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstEnumProcessModulesExArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded, ADDRINT dwFilterFlag);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded, ADDRINT dwFilterFlag);
};

#endif // INST_ENUM_PROCESS_MODULES_EX_H
