#pragma once
#ifndef INST_VIRTUAL_QUERY_EX_H
#define INST_VIRTUAL_QUERY_EX_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include "CallContext.h"
#include "utils.h"
#include "NtStructures.h"

struct InstVirtualQueryExArgs {
    ADDRINT hProcess;
    ADDRINT lpAddress;
    ADDRINT lpBuffer;
    ADDRINT dwLength;
};

class InstVirtualQueryEx : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstVirtualQueryExArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static unsigned long InstVirtualQueryEx::show_module(MEMORY_BASIC_INFORMATION info);
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT lpAddress, ADDRINT lpBuffer, ADDRINT dwLength);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT lpAddress, ADDRINT lpBuffer, ADDRINT dwLength);
};

#endif // INST_VIRTUAL_QUERY_EX_H
