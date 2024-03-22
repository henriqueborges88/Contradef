#pragma once
#ifndef INST_VIRTUAL_PROTECT_EX_H
#define INST_VIRTUAL_PROTECT_EX_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include "CallContext.h"
#include "utils.h"

struct InstVirtualProtectExArgs {
    ADDRINT hProcess;
    ADDRINT lpAddress;
    ADDRINT dwSize;
    ADDRINT flNewProtect;
    ADDRINT lpflOldProtect;
};

class InstVirtualProtectEx : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstVirtualProtectExArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT lpAddress, ADDRINT dwSize, ADDRINT flNewProtect, ADDRINT lpflOldProtect);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT lpAddress, ADDRINT dwSize, ADDRINT flNewProtect, ADDRINT lpflOldProtect);
};

#endif // INST_VIRTUAL_PROTECT_EX_H
