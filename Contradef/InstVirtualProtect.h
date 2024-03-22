#pragma once
#ifndef INST_VIRTUAL_PROTECT_H
#define INST_VIRTUAL_PROTECT_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include "CallContext.h"
#include "utils.h"

struct InstVirtualProtectArgs {
    ADDRINT lpAddress;
    ADDRINT dwSize;
    ADDRINT flNewProtect;
    ADDRINT lpflOldProtect;
};

class InstVirtualProtect : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstVirtualProtectArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpAddress, ADDRINT dwSize, ADDRINT flNewProtect, ADDRINT lpflOldProtect);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpAddress, ADDRINT dwSize, ADDRINT flNewProtect, ADDRINT lpflOldProtect);
};

#endif // INST_VIRTUAL_PROTECT_H
